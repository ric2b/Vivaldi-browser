// Copyright (c) 2016-2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/runtime/runtime_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/vivaldi_apptools.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_restrictions.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#if defined(OS_MACOSX)
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_paths.h"
#endif  // defined(OS_MACOSX)
#include "chrome/common/pref_names.h"
#include "components/download/public/background_service/download_service.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#include "extensions/api/window/window_private_api.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/schema/runtime_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "prefs/vivaldi_pref_names.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/devtools/devtools_connector.h"
#include "ui/vivaldi_ui_utils.h"

using base::PathService;

namespace extensions {

const char kRuntimeFeaturesFilename[] = "features.json";

using vivaldi::runtime_private::FeatureFlagInfo;

FeatureEntry::FeatureEntry() {}

VivaldiRuntimeFeatures::VivaldiRuntimeFeatures(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
  LoadRuntimeFeatures();
}

VivaldiRuntimeFeatures::VivaldiRuntimeFeatures()
    : profile_(nullptr), weak_ptr_factory_(this) {}

VivaldiRuntimeFeatures::~VivaldiRuntimeFeatures() {}

/* static */
bool VivaldiRuntimeFeatures::IsEnabled(content::BrowserContext* context,
                                       const std::string& feature_name) {
  extensions::VivaldiRuntimeFeatures* features =
      extensions::VivaldiRuntimeFeaturesFactory::GetForBrowserContext(context);
  DCHECK(features);
  FeatureEntry* feature = features->FindNamedFeature(feature_name);
  DCHECK(feature);
  // Check different flags for official builds and sopranos/internal builds.
#if defined(OFFICIAL_BUILD)
  return (feature && feature->enabled);
#else
  return (feature && feature->internal_enabled);
#endif  // defined(OFFICIAL_BUILD)
}

void VivaldiRuntimeFeatures::LoadRuntimeFeatures() {
  // This might be called outside the startup, eg. during creation of a guest
  // window, so need to allow IO.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::FilePath path;

#if defined(OS_MACOSX)
  if(PathService::Get(chrome::DIR_RESOURCES, &path)) {
#else
  if (PathService::Get(base::DIR_MODULE, &path)) {
#endif  // defined(OS_MACOSX)
    path = path.AppendASCII(kRuntimeFeaturesFilename);

    store_ = new JsonPrefStore(path,
                               std::unique_ptr<PrefFilter>(),
                               base::CreateSequencedTaskRunnerWithTraits(
                                  { base::MayBlock()}) .get());
    store_->ReadPrefs();

    if (!GetFlags(&flags_)) {
      LOG(ERROR) << "Unable to locate the \"flags\" preference file: "
                 << path.value();
    }
  } else {
    LOG(ERROR) << "Unable to locate the \"flags\" preference file.";
  }
}

bool VivaldiRuntimeFeatures::GetFlags(FeatureEntryMap* flags) {
  DCHECK(store_.get());

  const base::Value* flags_value;
  if (!store_->GetValue("flags", &flags_value))
    return false;

  const base::DictionaryValue* dict;
  if (!flags_value->GetAsDictionary(&dict))
    return false;

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    const base::Value* root = NULL;
    if (!dict->GetWithoutPathExpansion(it.key(), &root)) {
      LOG(WARNING) << "Invalid entry in \"" << kRuntimeFeaturesFilename
                   << "\" file.";
      continue;
    }
    const base::DictionaryValue* value = NULL;
    if (root->GetAsDictionary(&value)) {
      FeatureEntry* entry = new FeatureEntry;

      for (base::DictionaryValue::Iterator values_it(*value);
           !values_it.IsAtEnd(); values_it.Advance()) {
        const base::Value* sub_value = NULL;
        if (value->GetWithoutPathExpansion(values_it.key(), &sub_value)) {
          if (values_it.key() == "description") {
            sub_value->GetAsString(&entry->description);
          } else if (values_it.key() == "friendly_name") {
            sub_value->GetAsString(&entry->friendly_name);
          } else if (values_it.key() == "value") {
            std::string value;
            if (sub_value->GetAsString(&value)) {
              entry->enabled = (value == "true");
              entry->default_enabled = entry->enabled;
            }
          } else if (values_it.key() == "internal_value") {
            std::string value;
            if (sub_value->GetAsString(&value)) {
              entry->internal_enabled = (value == "true");
              entry->internal_default_enabled = entry->internal_enabled;
            }
          }
        }
      }
      // Check if this has been enabled or disabled on the cmd line.
      std::string cmd_switch = "enable-feature:";
      cmd_switch.append(it.key());

      if (cl->HasSwitch(cmd_switch)) {
        // always enable this feature and force it always on
        entry->internal_enabled = entry->enabled = true;
        entry->force_value = true;
      }
      cmd_switch = "disable-feature:";
      cmd_switch.append(it.key());
      if (cl->HasSwitch(cmd_switch)) {
        // always disable this feature and force it always off
        entry->internal_enabled = entry->enabled = false;
        entry->force_value = true;
      }
      flags->insert(std::make_pair(it.key(), entry));
    }
  }
  const base::ListValue* list =
      profile_->GetPrefs()->GetList(vivaldiprefs::kVivaldiExperiments);

  // Check for any features that might be enabled by the user now.
  for (auto& flag_entry : *flags) {
    base::ListValue::const_iterator it =
        list->Find(base::Value(flag_entry.first));
    if (it != list->end() && flag_entry.second->force_value == false) {
      flag_entry.second->enabled = true;
    }
  }
  return true;
}

FeatureEntry* VivaldiRuntimeFeatures::FindNamedFeature(
    const std::string& feature_name) {
  FeatureEntryMap::iterator it = flags_.find(feature_name);
  if (it != flags_.end()) {
    return (*it).second;
  }
  return nullptr;
}

const FeatureEntryMap& VivaldiRuntimeFeatures::GetAllFeatures() {
  return flags_;
}

VivaldiRuntimeFeatures* VivaldiRuntimeFeaturesFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<VivaldiRuntimeFeatures*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

VivaldiRuntimeFeaturesFactory* VivaldiRuntimeFeaturesFactory::GetInstance() {
  return base::Singleton<VivaldiRuntimeFeaturesFactory>::get();
}

VivaldiRuntimeFeaturesFactory::VivaldiRuntimeFeaturesFactory()
    : BrowserContextKeyedServiceFactory(
          "VivaldiRuntimeFeatures",
          BrowserContextDependencyManager::GetInstance()) {
  if (!::vivaldi::IsVivaldiRunning() && !::vivaldi::IsDebuggingVivaldi())
    return;
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();

  // Register this as an observer of the info cache.
  storage.AddObserver(this);
}

VivaldiRuntimeFeaturesFactory::~VivaldiRuntimeFeaturesFactory() {
  if (!::vivaldi::IsVivaldiRunning() && !::vivaldi::IsDebuggingVivaldi())
    return;

  // Browser process is gone on exit, no need to remove listener then.
  if (g_browser_process) {
    ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();

    storage.RemoveObserver(this);
  }
}

KeyedService* VivaldiRuntimeFeaturesFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new VivaldiRuntimeFeatures(Profile::FromBrowserContext(profile));
}

bool VivaldiRuntimeFeaturesFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

bool VivaldiRuntimeFeaturesFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* VivaldiRuntimeFeaturesFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Redirected in incognito.
  return extensions::ExtensionsBrowserClient::Get()->GetOriginalContext(
      context);
}

void VivaldiRuntimeFeaturesFactory::UpdateProfiles() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> active_profiles = profile_manager->GetLoadedProfiles();

  // We need to notify all profiles.
  for (Profile* profile : active_profiles) {
    DCHECK(profile);
    std::unique_ptr<base::ListValue> args(new base::ListValue);

    ::vivaldi::BroadcastEvent(
        vivaldi::runtime_private::OnProfilesUpdated::kEventName,
        std::move(args), profile);
  }
}

void VivaldiRuntimeFeaturesFactory::OnProfileAdded(
    const base::FilePath& profile_path) {
  UpdateProfiles();
}

void VivaldiRuntimeFeaturesFactory::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  UpdateProfiles();
}

void VivaldiRuntimeFeaturesFactory::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  UpdateProfiles();
}

void VivaldiRuntimeFeaturesFactory::OnProfileAuthInfoChanged(
    const base::FilePath& profile_path) {
  UpdateProfiles();
}

void VivaldiRuntimeFeaturesFactory::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  UpdateProfiles();
}

void VivaldiRuntimeFeaturesFactory::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  UpdateProfiles();
}

void VivaldiRuntimeFeaturesFactory::OnProfileSigninRequiredChanged(
    const base::FilePath& profile_path) {
  UpdateProfiles();
}

void VivaldiRuntimeFeaturesFactory::OnProfileIsOmittedChanged(
    const base::FilePath& profile_path) {
  UpdateProfiles();
}

ExtensionFunction::ResponseAction RuntimePrivateExitFunction::Run() {
  // Free any open devtools if the user selects Exit from the menu.
  DevtoolsConnectorAPI::CloseAllDevtools(browser_context());

  VivaldiUtilitiesAPI::CloseAllThumbnailWindows(browser_context());

  chrome::CloseAllBrowsersAndQuit();

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
RuntimePrivateGetAllFeatureFlagsFunction::Run() {
  namespace Results = vivaldi::runtime_private::GetAllFeatureFlags::Results;

  extensions::VivaldiRuntimeFeatures* features =
      extensions::VivaldiRuntimeFeaturesFactory::GetForBrowserContext(
          browser_context());

  const FeatureEntryMap& flags = features->GetAllFeatures();
  std::vector<FeatureFlagInfo> results;

  for (auto& entry : flags) {
    FeatureFlagInfo* flag = new FeatureFlagInfo;
    flag->name = entry.first;
    flag->friendly_name = entry.second->friendly_name;
    flag->description = entry.second->description;
    flag->value = entry.second->enabled ? "true" : "false";
    flag->internal_value = entry.second->internal_enabled ? "true" : "false";
    flag->locked = entry.second->force_value;

    results.push_back(std::move(*flag));
  }
  return RespondNow(ArgumentList(Results::Create(results)));
}

ExtensionFunction::ResponseAction
RuntimePrivateSetFeatureEnabledFunction::Run() {
  using vivaldi::runtime_private::SetFeatureEnabled::Params;
  namespace Results = vivaldi::runtime_private::SetFeatureEnabled::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  extensions::VivaldiRuntimeFeatures* features =
      extensions::VivaldiRuntimeFeaturesFactory::GetForBrowserContext(
          browser_context());

  bool found = false;
  FeatureEntry* entry = features->FindNamedFeature(params->feature_name);
  bool enable_entry = params->enable;
  DCHECK(entry);
  if (entry && !entry->force_value) {
    // Update the entry in-memory only so the list is always up-to-date.
    entry->internal_enabled = entry->enabled = enable_entry;
    found = true;
    ListPrefUpdate update(
        Profile::FromBrowserContext(browser_context())->GetPrefs(),
        vivaldiprefs::kVivaldiExperiments);
    base::ListValue* experiments_list = update.Get();

    experiments_list->Clear();
    const extensions::FeatureEntryMap& flags = features->GetAllFeatures();
    for (auto& flag : flags) {
      // Enable it if it's different than the default
      if (flag.second->enabled &&
          flag.second->default_enabled != flag.second->enabled) {
        experiments_list->AppendString(flag.first);
      }
    }
  }
  return RespondNow(ArgumentList(Results::Create(found)));
}

ExtensionFunction::ResponseAction RuntimePrivateIsGuestSessionFunction::Run() {
  namespace Results = vivaldi::runtime_private::IsGuestSession::Results;

  Profile *profile = Profile::FromBrowserContext(browser_context());
  bool is_guest = false;
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  if (service->GetBoolean(prefs::kBrowserGuestModeEnabled) &&
      !profile->IsSupervised()) {
    is_guest = profile->IsGuestSession();
  }
  return RespondNow(ArgumentList(Results::Create(is_guest)));
}

ExtensionFunction::ResponseAction RuntimePrivateHasGuestSessionFunction::Run() {
  namespace Results = vivaldi::runtime_private::HasGuestSession::Results;

  bool has_guest = false;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->IsGuestSession()) {
      has_guest = true;
      break;
    }
  }
  return RespondNow(ArgumentList(Results::Create(has_guest)));
}

ExtensionFunction::ResponseAction
RuntimePrivateSwitchToGuestSessionFunction::Run() {
  namespace Results = vivaldi::runtime_private::SwitchToGuestSession::Results;

  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  DCHECK(service->GetBoolean(prefs::kBrowserGuestModeEnabled));

  bool success = service->GetBoolean(prefs::kBrowserGuestModeEnabled);

  if (success) {
    profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
  }

  return RespondNow(ArgumentList(Results::Create(success)));
}

ExtensionFunction::ResponseAction
RuntimePrivateCloseGuestSessionFunction::Run() {
  namespace Results = vivaldi::runtime_private::CloseGuestSession::Results;

  profiles::CloseGuestProfileWindows();

  return RespondNow(ArgumentList(Results::Create(true)));
}

ExtensionFunction::ResponseAction
RuntimePrivateOpenProfileSelectionWindowFunction::Run() {
  namespace Results =
      vivaldi::runtime_private::OpenProfileSelectionWindow::Results;

  // If this is a guest session, close all the guest browser windows.
  Profile *profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsGuestSession()) {
    profiles::CloseGuestProfileWindows();
  } else {
    UserManager::Show(base::FilePath(),
                      profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
  }
  return RespondNow(ArgumentList(Results::Create(true)));
}

ExtensionFunction::ResponseAction RuntimePrivateGetUserProfilesFunction::Run() {
  using vivaldi::runtime_private::GetUserProfiles::Params;
  namespace Results = vivaldi::runtime_private::GetUserProfiles::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ProfileManager* manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage = manager->GetProfileAttributesStorage();

  // Find the active entry.
  Profile *profile = Profile::FromBrowserContext(browser_context());
  ProfileAttributesEntry* active_entry = nullptr;
  storage.GetProfileAttributesWithPath(profile->GetPath(), &active_entry);

  std::vector<vivaldi::runtime_private::UserProfile> profiles;

  for (ProfileAttributesEntry* entry : storage.GetAllProfilesAttributes()) {
    vivaldi::runtime_private::UserProfile profile;

    if (entry->IsSupervised() || entry->IsChild()) {
      // Skip supervised accounts.
      continue;
    }
    const size_t icon_index = entry->GetAvatarIconIndex();

    profile.active = (active_entry == entry);
    profile.guest = false;
    profile.name = base::UTF16ToUTF8(entry->GetUserName());
    if (profile.name.empty()) {
      profile.name = base::UTF16ToUTF8(entry->GetName());
    }
    profile.image = profiles::GetDefaultAvatarIconUrl(icon_index);
    profile.image_index = icon_index;
    profile.path = entry->GetPath().AsUTF8Unsafe();

    profiles.push_back(std::move(profile));
  }
  if (!active_entry) {
    // We might be a guest profile.
    if (profile->IsGuestSession()) {
      vivaldi::runtime_private::UserProfile profile;

      // We'll add a "fake" profile here.
      profile.active = true;
      profile.guest = true;
      profile.name = "Guest"; // Translated on the js side.

      profiles.push_back(std::move(profile));
    }
  }

  return RespondNow(ArgumentList(Results::Create(profiles)));
}

ExtensionFunction::ResponseAction
RuntimePrivateOpenNamedProfileFunction::Run() {
  using vivaldi::runtime_private::OpenNamedProfile::Params;
  namespace Results = vivaldi::runtime_private::OpenNamedProfile::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool success = false;
  ProfileManager* manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage = manager->GetProfileAttributesStorage();
  std::vector<ProfileAttributesEntry*> entries =
    storage.GetAllProfilesAttributes();

  for (auto* entry : entries) {
    if (entry->GetPath().AsUTF8Unsafe() == params->profile_path) {
      profiles::SwitchToProfile(entry->GetPath(), false,
                                ProfileManager::CreateCallback(),
                                ProfileMetrics::SWITCH_PROFILE_ICON);
      success = true;
      break;
    }
  }
  return RespondNow(ArgumentList(Results::Create(success)));
}

ExtensionFunction::ResponseAction
RuntimePrivateCloseActiveProfileFunction::Run() {
  namespace Results = vivaldi::runtime_private::OpenNamedProfile::Results;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  extensions::VivaldiWindowsAPI::WindowsForProfileClosing(profile);

  profiles::CloseProfileWindows(profile);

  return RespondNow(ArgumentList(Results::Create(true)));
}

ExtensionFunction::ResponseAction
RuntimePrivateGetUserProfileImagesFunction::Run() {
  namespace Results = vivaldi::runtime_private::GetUserProfileImages::Results;
  std::vector<vivaldi::runtime_private::UserProfileImage> images;
  size_t placeholder_index = profiles::GetPlaceholderAvatarIndex();

  for (size_t index = 0; index < profiles::GetDefaultAvatarIconCount(); index++) {
    if (index == placeholder_index) {
      // Placeholder, skip it.
      continue;
    }
    vivaldi::runtime_private::UserProfileImage image;

    image.name = l10n_util::GetStringUTF8(
      profiles::GetDefaultAvatarLabelResourceIDAtIndex(index));
    image.index = index;
    // Avatar is served via a chrome://theme/ url.
    image.image = profiles::GetDefaultAvatarIconUrl(index);
    images.push_back(std::move(image));
  }
  return RespondNow(ArgumentList(Results::Create(images)));
}

ExtensionFunction::ResponseAction
RuntimePrivateUpdateActiveProfileFunction::Run() {
  using vivaldi::runtime_private::UpdateActiveProfile::Params;
  namespace Results = vivaldi::runtime_private::UpdateActiveProfile::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::string16 name = base::UTF8ToUTF16(params->name);
  size_t index = params->avatar_index;
  bool success = false;

  if (index <= profiles::GetDefaultAvatarIconCount()) {
    Profile* profile = Profile::FromBrowserContext(browser_context());
    PrefService* pref_service = profile->GetPrefs();
    pref_service->SetInteger(prefs::kProfileAvatarIndex, index);
    pref_service->SetBoolean(prefs::kProfileUsingDefaultAvatar, false);
    pref_service->SetBoolean(prefs::kProfileUsingGAIAAvatar, false);

    base::TrimWhitespace(name, base::TRIM_ALL, &name);
    if (!name.empty()) {
      profiles::UpdateProfileName(profile, name);
      success = true;
    }
    if (params->create_desktop_icon.get()) {
      if (ProfileShortcutManager::IsFeatureEnabled()) {
        ProfileShortcutManager* shortcut_manager =
          g_browser_process->profile_manager()->profile_shortcut_manager();
        DCHECK(shortcut_manager);

        if (*params->create_desktop_icon.get()) {
          shortcut_manager->CreateProfileShortcut(profile->GetPath());
        } else {
          shortcut_manager->RemoveProfileShortcuts(profile->GetPath());
        }
      }
    }
  }
  return RespondNow(ArgumentList(Results::Create(success)));
}

ExtensionFunction::ResponseAction
RuntimePrivateGetProfileDefaultsFunction::Run() {
  namespace Results = vivaldi::runtime_private::GetProfileDefaults::Results;

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  std::string name = base::UTF16ToUTF8(storage.ChooseNameForNewProfile(0));

  return RespondNow(ArgumentList(Results::Create(name)));
}

ExtensionFunction::ResponseAction
RuntimePrivateCreateProfileFunction::Run() {
  using vivaldi::runtime_private::CreateProfile::Params;
  namespace Results = vivaldi::runtime_private::CreateProfile::Results;

  if (!profiles::IsMultipleProfilesEnabled()) {
    return RespondNow(ArgumentList(Results::Create(false)));
  }
  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::string16 name = base::UTF8ToUTF16(params->name);
  std::string avatar_url = params->avatar_url;
  bool create_shortcut = params->create_desktop_icon;

  ProfileManager::CreateMultiProfileAsync(
      name, avatar_url,
      base::BindRepeating(
          &RuntimePrivateCreateProfileFunction::OnProfileCreated, this,
          create_shortcut));
  return RespondLater();
}

void RuntimePrivateCreateProfileFunction::OnProfileCreated(
    bool create_shortcut,
    Profile* profile,
    Profile::CreateStatus status) {
  namespace Results = vivaldi::runtime_private::CreateProfile::Results;

  switch (status) {
    case Profile::CREATE_STATUS_LOCAL_FAIL: {
      Respond(ArgumentList(Results::Create(false)));
      break;
    }
    case Profile::CREATE_STATUS_CREATED: {
      // Do nothing for an intermediate status.
      break;
    }
    case Profile::CREATE_STATUS_INITIALIZED: {
      CreateShortcutAndShowSuccess(create_shortcut, profile);
      break;
    }
    case Profile::CREATE_STATUS_CANCELED:
    case Profile::CREATE_STATUS_REMOTE_FAIL:
    case Profile::MAX_CREATE_STATUS: {
      NOTREACHED();
      Respond(ArgumentList(Results::Create(false)));
      break;
    }
  }
}

void RuntimePrivateCreateProfileFunction::CreateShortcutAndShowSuccess(
    bool create_shortcut,
    Profile* profile) {
  if (create_shortcut) {
    DCHECK(ProfileShortcutManager::IsFeatureEnabled());
    if (ProfileShortcutManager::IsFeatureEnabled()) {
      ProfileShortcutManager* shortcut_manager =
        g_browser_process->profile_manager()->profile_shortcut_manager();
      DCHECK(shortcut_manager);
      if (shortcut_manager)
        shortcut_manager->CreateProfileShortcut(profile->GetPath());
    }
  }
  // Opening the new window must be the last action, after all callbacks
  // have been run, to give them a chance to initialize the profile.
  OpenNewWindowForProfile(profile, Profile::CREATE_STATUS_INITIALIZED);
}

void RuntimePrivateCreateProfileFunction::OpenNewWindowForProfile(
    Profile* profile,
    Profile::CreateStatus status) {
  profiles::OpenBrowserWindowForProfile(
      base::BindRepeating(
          &RuntimePrivateCreateProfileFunction::OnBrowserReadyCallback, this),
      false,  // Don't create a window if one already exists.
      true,   // Create a first run window.
      false,  // There is no need to unblock all extensions because we only open
              // browser window if the Profile is not locked. Hence there is no
              // extension blocked.
      profile, status);
}

void RuntimePrivateCreateProfileFunction::OnBrowserReadyCallback(
    Profile* profile,
    Profile::CreateStatus profile_create_status) {
  namespace Results = vivaldi::runtime_private::CreateProfile::Results;

  if (!did_respond()) {
    Respond(ArgumentList(Results::Create(true)));
  }
}

RuntimePrivateGetProfileStatisticsFunction::
    RuntimePrivateGetProfileStatisticsFunction() {}
RuntimePrivateGetProfileStatisticsFunction::
    ~RuntimePrivateGetProfileStatisticsFunction() {}

ExtensionFunction::ResponseAction
RuntimePrivateGetProfileStatisticsFunction::Run() {
  using vivaldi::runtime_private::GetProfileStatistics::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::FilePath profile_path =
      base::FilePath::FromUTF8Unsafe(params->profile_path);

  Profile* profile = g_browser_process->profile_manager()->
    GetProfileByPath(profile_path);

  if (profile) {
    GatherStatistics(profile);
  } else {
    g_browser_process->profile_manager()->LoadProfileByPath(
        profile_path, false,
        base::Bind(
            &RuntimePrivateGetProfileStatisticsFunction::GatherStatistics,
            this));
  }
  return RespondLater();
}

void RuntimePrivateGetProfileStatisticsFunction::GatherStatistics(
    Profile* profile) {
  if (profile) {
    ProfileStatisticsFactory::GetForProfile(profile)->GatherStatistics(
        base::BindRepeating(&RuntimePrivateGetProfileStatisticsFunction::
                                GetProfileStatsCallback,
                            this, profile->GetPath()));
  } else {
    Respond(Error("Failed to load profile"));
  }
}

void RuntimePrivateGetProfileStatisticsFunction::GetProfileStatsCallback(
    base::FilePath profile_path,
    profiles::ProfileCategoryStats result) {
  namespace Results = vivaldi::runtime_private::GetProfileStatistics::Results;
  using vivaldi::runtime_private::ProfileStatEntry;

  if (result.size() == profiles::kProfileStatisticsCategories.size()) {
    // We've received all results.
    for (const auto& item : result) {
      vivaldi::runtime_private::ProfileStatEntry stat;
      stat.category = item.category;
      stat.count = item.count;
      results_.push_back(std::move(stat));
    }
    Respond(ArgumentList(Results::Create(results_)));
  }
}

// MaybeScheduleProfileForDeletion does not consistently call the callback, so we
// can't rely on it being called.
namespace runtime_api {
void DeleteProfileCallback(std::unique_ptr<ScopedKeepAlive> keep_alive,
                           Profile* profile) {
  webui::OpenNewWindowForProfile(profile);
}
}  // namespace runtime_api

ExtensionFunction::ResponseAction
RuntimePrivateDeleteProfileFunction::Run() {
  namespace Results = vivaldi::runtime_private::DeleteProfile::Results;
  using vivaldi::runtime_private::DeleteProfile::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::FilePath profile_path =
    base::FilePath::FromUTF8Unsafe(params->profile_path);

  g_browser_process->profile_manager()->MaybeScheduleProfileForDeletion(
      profile_path,
      base::BindOnce(
          &runtime_api::DeleteProfileCallback,
          std::make_unique<ScopedKeepAlive>(KeepAliveOrigin::PROFILE_HELPER,
                                            KeepAliveRestartOption::DISABLED)),
      ProfileMetrics::DELETE_PROFILE_SETTINGS);

  return RespondNow(ArgumentList(Results::Create(true)));
}

ExtensionFunction::ResponseAction
RuntimePrivateHasDesktopShortcutFunction::Run() {
  namespace Results = vivaldi::runtime_private::HasDesktopShortcut::Results;
#if defined(OS_WIN)
  if (ProfileShortcutManager::IsFeatureEnabled()) {
    ProfileShortcutManager* shortcut_manager =
      g_browser_process->profile_manager()->profile_shortcut_manager();
    DCHECK(shortcut_manager);
    Profile* profile = Profile::FromBrowserContext(browser_context());
    shortcut_manager->HasProfileShortcuts(
      profile->GetPath(),
      base::Bind(
        &RuntimePrivateHasDesktopShortcutFunction::OnHasProfileShortcuts,
        this));
    return RespondLater();
  } else {
    return RespondNow(ArgumentList(Results::Create(false, false)));
  }
#else
  return RespondNow(ArgumentList(Results::Create(false, false)));
#endif  // defined(OS_WIN)
}

void RuntimePrivateHasDesktopShortcutFunction::OnHasProfileShortcuts(
    bool has_shortcuts) {
  namespace Results = vivaldi::runtime_private::HasDesktopShortcut::Results;

  return Respond(ArgumentList(Results::Create(has_shortcuts, true)));
}

}  // namespace extensions
