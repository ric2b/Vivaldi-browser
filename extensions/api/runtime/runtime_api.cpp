// Copyright (c) 2016-2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/runtime/runtime_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/devtools/devtools_connector.h"
#include "ui/vivaldi_browser_window.h"
#include "app/vivaldi_apptools.h"
#include "browser/vivaldi_runtime_feature.h"
#include "extensions/api/window/window_private_api.h"
#include "extensions/schema/runtime_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "prefs/vivaldi_pref_names.h"
#include "ui/vivaldi_rootdocument_handler.h"
#include "ui/vivaldi_ui_utils.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#endif  // BUILDFLAG(IS_MAC)

namespace extensions {

namespace {

class ProfileStorageObserver : public ProfileAttributesStorage::Observer {
 public:
  ProfileStorageObserver();

  // This is never called.
  ~ProfileStorageObserver() override = default;

  static ProfileStorageObserver& GetInstance();

  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;
  void OnProfileAuthInfoChanged(const base::FilePath& profile_path) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;
  void OnProfileSigninRequiredChanged(
      const base::FilePath& profile_path) override;
  void OnProfileIsOmittedChanged(const base::FilePath& profile_path) override;

  void UpdateProfiles();
};

ProfileStorageObserver::ProfileStorageObserver() {
  if (!::vivaldi::IsVivaldiRunning())
    return;
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();

  // Register this as an observer of the info cache.
  storage.AddObserver(this);
}

/* static */
ProfileStorageObserver& ProfileStorageObserver::GetInstance() {
  static base::NoDestructor<ProfileStorageObserver> instance;
  return *instance;
}

void ProfileStorageObserver::UpdateProfiles() {
  ::vivaldi::BroadcastEventToAllProfiles(
      vivaldi::runtime_private::OnProfilesUpdated::kEventName);
}

void ProfileStorageObserver::OnProfileAdded(
    const base::FilePath& profile_path) {
  UpdateProfiles();
}

void ProfileStorageObserver::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const std::u16string& profile_name) {
  UpdateProfiles();
}

void ProfileStorageObserver::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const std::u16string& old_profile_name) {
  UpdateProfiles();
}

void ProfileStorageObserver::OnProfileAuthInfoChanged(
    const base::FilePath& profile_path) {
  UpdateProfiles();
}

void ProfileStorageObserver::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  UpdateProfiles();
}

void ProfileStorageObserver::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  UpdateProfiles();
}

void ProfileStorageObserver::OnProfileSigninRequiredChanged(
    const base::FilePath& profile_path) {
  UpdateProfiles();
}

void ProfileStorageObserver::OnProfileIsOmittedChanged(
    const base::FilePath& profile_path) {
  UpdateProfiles();
}

}  // namespace

// static
void RuntimeAPI::Init() {
  ProfileStorageObserver::GetInstance();
}

// static
void RuntimeAPI::OnProfileAvatarChanged(Profile* profile) {
  ProfileStorageObserver::GetInstance().OnProfileAvatarChanged(
      profile->GetPath());
}

ExtensionFunction::ResponseAction RuntimePrivateExitFunction::Run() {
  using vivaldi::runtime_private::Exit::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!browser_shutdown::IsTryingToQuit()) {
    // Free any open devtools if the user selects Exit from the menu.
    DevtoolsConnectorAPI::CloseAllDevtools();

    chrome::CloseAllBrowsersAndQuit();
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction RuntimePrivateRestartFunction::Run() {
  ::vivaldi::RestartBrowser();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
RuntimePrivateGetAllFeatureFlagsFunction::Run() {
  namespace Results = vivaldi::runtime_private::GetAllFeatureFlags::Results;
  using vivaldi::runtime_private::FeatureFlagInfo;

  std::vector<FeatureFlagInfo> results;
  const vivaldi_runtime_feature::FeatureMap& feature_map =
      vivaldi_runtime_feature::GetAllFeatures();
  const vivaldi_runtime_feature::EnabledSet* enabled_set =
      vivaldi_runtime_feature::GetEnabled(browser_context());
  for (auto& i : feature_map) {
    if (i.second.inactive)
      continue;
    const std::string& name = i.first;
    const vivaldi_runtime_feature::Feature& feature = i.second;
    FeatureFlagInfo flag;
    flag.name = name;
    flag.friendly_name = feature.friendly_name;
    flag.description = feature.description;
    flag.locked = feature.locked;
    flag.value = enabled_set && enabled_set->contains(name);
    results.push_back(std::move(flag));
  }
  return RespondNow(ArgumentList(Results::Create(results)));
}

ExtensionFunction::ResponseAction
RuntimePrivateSetFeatureEnabledFunction::Run() {
  using vivaldi::runtime_private::SetFeatureEnabled::Params;
  namespace Results = vivaldi::runtime_private::SetFeatureEnabled::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool found = vivaldi_runtime_feature::Enable(
      browser_context(), params->feature_name, params->enable);
  return RespondNow(ArgumentList(Results::Create(found)));
}

ExtensionFunction::ResponseAction RuntimePrivateIsGuestSessionFunction::Run() {
  namespace Results = vivaldi::runtime_private::IsGuestSession::Results;

  bool is_guest = false;
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  if (service->GetBoolean(prefs::kBrowserGuestModeEnabled)) {
    content::WebContents* web_contents = GetSenderWebContents();
    Profile* profile = Profile::FromBrowserContext(web_contents->GetBrowserContext());
    is_guest = profile->IsGuestSession();
  }
  return RespondNow(ArgumentList(Results::Create(is_guest)));
}

ExtensionFunction::ResponseAction RuntimePrivateHasGuestSessionFunction::Run() {
  namespace Results = vivaldi::runtime_private::HasGuestSession::Results;

  bool has_guest = false;
  for (Browser* browser : *BrowserList::GetInstance()) {
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

  // First test available browsers and open a new guest window if we already
  // have a browser for a guest window to allow for multiple guest windows.
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->IsGuestSession()) {
      chrome::NewWindow(browser);
      return RespondNow(ArgumentList(Results::Create(true)));
    }
  }

  // Otherwise we create the first guest window if possible.
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  DCHECK(service->GetBoolean(prefs::kBrowserGuestModeEnabled));

  bool success = service->GetBoolean(prefs::kBrowserGuestModeEnabled);

  if (success) {
    profiles::SwitchToGuestProfile();
  }

  return RespondNow(ArgumentList(Results::Create(success)));
}

ExtensionFunction::ResponseAction
RuntimePrivateCloseGuestSessionFunction::Run() {
  namespace Results = vivaldi::runtime_private::CloseGuestSession::Results;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  profiles::CloseProfileWindows(profile);

  return RespondNow(ArgumentList(Results::Create(true)));
}

ExtensionFunction::ResponseAction
RuntimePrivateOpenProfileSelectionWindowFunction::Run() {
  namespace Results =
      vivaldi::runtime_private::OpenProfileSelectionWindow::Results;

  // If this is a guest session, close all the guest browser windows.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsGuestSession()) {
    profiles::CloseProfileWindows(profile);
  } else {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kBackgroundModeManager));
  }
  return RespondNow(ArgumentList(Results::Create(true)));
}

ExtensionFunction::ResponseAction RuntimePrivateGetUserProfilesFunction::Run() {
  using vivaldi::runtime_private::GetUserProfiles::Params;
  namespace Results = vivaldi::runtime_private::GetUserProfiles::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ProfileManager* manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage = manager->GetProfileAttributesStorage();
  bool active_only = params->active_only ? *params->active_only : false;

  // Find the active entry.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  ProfileAttributesEntry* active_entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());

  std::vector<vivaldi::runtime_private::UserProfile> profiles;
  bool has_custom_avatars = false;

  for (ProfileAttributesEntry* entry : storage.GetAllProfilesAttributes()) {
    vivaldi::runtime_private::UserProfile user_profile;

    if (entry->IsSupervised() /* || entry->IsChild() */) {
      // Skip supervised accounts.
      continue;
    }
    if (!active_only || (active_only && active_entry == entry)) {
      const size_t icon_index = entry->GetAvatarIconIndex();

      user_profile.active = (active_entry == entry);
      user_profile.guest = false;
      user_profile.name = base::UTF16ToUTF8(entry->GetUserName());
      if (user_profile.name.empty()) {
        user_profile.name = base::UTF16ToUTF8(entry->GetName());
      }
      user_profile.image = profiles::GetDefaultAvatarIconUrl(icon_index);
      user_profile.image_index = icon_index;
      user_profile.path = entry->GetPath().AsUTF8Unsafe();

      // Check for custom profile image.
      std::string custom_avatar = ::vivaldi::GetImagePathFromProfilePath(
          vivaldiprefs::kVivaldiProfileImagePath, user_profile.path);
      if (!custom_avatar.empty()) {
        // We set the path here, then convert it to base64 in a separate
        // operation below.
        user_profile.custom_avatar = custom_avatar;
        has_custom_avatars = true;
      }
      profiles.push_back(std::move(user_profile));
    }
  }
  if (!active_entry) {
    // We might be a guest profile.
    if (profile->IsGuestSession()) {
      vivaldi::runtime_private::UserProfile user_profile;

      // We'll add a "fake" user_profile here.
      user_profile.active = true;
      user_profile.guest = true;
      user_profile.name = "Guest";  // Translated on the js side.

      profiles.push_back(std::move(user_profile));
    }
  }
  if (has_custom_avatars) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(
            &RuntimePrivateGetUserProfilesFunction::ProcessImagesOnWorkerThread,
            this, std::move(profiles)));

    return RespondLater();
  } else {
    return RespondNow(ArgumentList(Results::Create(profiles)));
  }
}

void RuntimePrivateGetUserProfilesFunction::ProcessImagesOnWorkerThread(
    std::vector<vivaldi::runtime_private::UserProfile> profiles) {
  for (auto& profile : profiles) {
    if (profile.custom_avatar.empty()) {
      continue;
    }
    base::FilePath path = base::FilePath::FromUTF8Unsafe(profile.custom_avatar);
    base::File file(path, base::File::FLAG_READ | base::File::FLAG_OPEN);
    if (!file.IsValid()) {
      profile.custom_avatar = "";
      continue;
    }
    int64_t len64 = file.GetLength();
    if (len64 < 0 || len64 >= (static_cast<int64_t>(1) << 31) ||
        static_cast<int>(len64) == 0) {
      LOG(ERROR) << "Unexpected file length for " << path.value() << " - "
                 << len64;
      profile.custom_avatar = "";
      continue;
    }
    int len = static_cast<int>(len64);

    std::vector<unsigned char> buffer;

    buffer.resize(static_cast<size_t>(len));
    int read_len = file.Read(0, reinterpret_cast<char*>(buffer.data()), len);
    if (read_len != len) {
      LOG(ERROR) << "Failed to read " << len << "bytes from " << path.value();
      profile.custom_avatar = "";
      continue;
    }
    std::string image_data;
    std::string_view base64_input(
        reinterpret_cast<const char*>(&buffer.data()[0]), buffer.size());
    image_data = base::Base64Encode(base64_input);

    // Mime type does not matter
    image_data.insert(0, base::StringPrintf("data:image/png;base64,"));

    profile.custom_avatar.swap(image_data);
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RuntimePrivateGetUserProfilesFunction::FinishProcessImagesOnUIThread,
          this, std::move(profiles)));
}

void RuntimePrivateGetUserProfilesFunction::FinishProcessImagesOnUIThread(
    std::vector<vivaldi::runtime_private::UserProfile> profiles) {
  namespace Results = vivaldi::runtime_private::GetUserProfiles::Results;

  return Respond(ArgumentList(Results::Create(profiles)));
}

ExtensionFunction::ResponseAction
RuntimePrivateOpenNamedProfileFunction::Run() {
  using vivaldi::runtime_private::OpenNamedProfile::Params;
  namespace Results = vivaldi::runtime_private::OpenNamedProfile::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool success = false;
  ProfileManager* manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage = manager->GetProfileAttributesStorage();
  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();

  for (auto* entry : entries) {
    if (entry->GetPath().AsUTF8Unsafe() == params->profile_path) {
      profiles::SwitchToProfile(entry->GetPath(), false);
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

  for (size_t index = 0; index < profiles::GetDefaultAvatarIconCount();
       index++) {
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

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string name = base::UTF8ToUTF16(params->name);
  size_t index = params->avatar_index;
  bool success = false;

  if (index <= profiles::GetDefaultAvatarIconCount()) {
    Profile* profile = Profile::FromBrowserContext(browser_context());
    PrefService* pref_service = profile->GetPrefs();
    size_t old_index = pref_service->GetInteger(prefs::kProfileAvatarIndex);
    pref_service->SetInteger(prefs::kProfileAvatarIndex, index);
    pref_service->SetBoolean(prefs::kProfileUsingDefaultAvatar, false);
    pref_service->SetBoolean(prefs::kProfileUsingGAIAAvatar, false);

    if (old_index != index) {
      // User selected a new image, clear the custom avatar.
      ::vivaldi::SetImagePathForProfilePath(
          vivaldiprefs::kVivaldiProfileImagePath, "",
          profile->GetPath().AsUTF8Unsafe());
    }

    base::TrimWhitespace(name, base::TRIM_ALL, &name);
    if (!name.empty()) {
      profiles::UpdateProfileName(profile, name);
      success = true;
    }
    if (params->create_desktop_icon.has_value()) {
      if (ProfileShortcutManager::IsFeatureEnabled()) {
        ProfileShortcutManager* shortcut_manager =
            g_browser_process->profile_manager()->profile_shortcut_manager();
        DCHECK(shortcut_manager);

        if (params->create_desktop_icon.value()) {
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

ExtensionFunction::ResponseAction RuntimePrivateCreateProfileFunction::Run() {
  using vivaldi::runtime_private::CreateProfile::Params;
  namespace Results = vivaldi::runtime_private::CreateProfile::Results;

  if (!profiles::IsMultipleProfilesEnabled()) {
    return RespondNow(ArgumentList(Results::Create(false)));
  }
  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string name = base::UTF8ToUTF16(params->name);
  size_t index = params->avatar_index;
  bool create_shortcut = params->create_desktop_icon;

  ProfileManager::CreateMultiProfileAsync(
      name, index, false,
      base::BindRepeating(
          &RuntimePrivateCreateProfileFunction::OnProfileCreated, this,
          create_shortcut));
  return RespondLater();
}

void RuntimePrivateCreateProfileFunction::OnProfileCreated(
    bool create_shortcut,
    Profile* profile) {
  CreateShortcutAndShowSuccess(create_shortcut, profile);
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
  OpenNewWindowForProfile(profile);
}

void RuntimePrivateCreateProfileFunction::OpenNewWindowForProfile(
    Profile* profile) {
  profiles::OpenBrowserWindowForProfile(
      base::BindOnce(
          &RuntimePrivateCreateProfileFunction::OnBrowserReadyCallback, this),
      false,  // Don't create a window if one already exists.
      true,   // Create a first run window.
      false,  // There is no need to unblock all extensions because we only open
              // browser window if the Profile is not locked. Hence there is no
              // extension blocked.
      profile);
}

void RuntimePrivateCreateProfileFunction::OnBrowserReadyCallback(
    Browser* browser) {
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

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::FilePath profile_path =
      base::FilePath::FromUTF8Unsafe(params->profile_path);

  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);

  if (profile) {
    GatherStatistics(profile);
  } else {
    // Mark the profile to be a gather-profile that does not need a
    // vivaldirootdocumenthandler.
    MarkProfilePathForNoVivaldiClient(profile_path);
    g_browser_process->profile_manager()->LoadProfileByPath(
        profile_path, false,
        base::BindOnce(
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
    ClearProfilePathForNoVivaldiClient(profile_path);
    Respond(ArgumentList(Results::Create(results_)));
  }
}

ExtensionFunction::ResponseAction RuntimePrivateDeleteProfileFunction::Run() {
  namespace Results = vivaldi::runtime_private::DeleteProfile::Results;
  using vivaldi::runtime_private::DeleteProfile::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::FilePath profile_path =
      base::FilePath::FromUTF8Unsafe(params->profile_path);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetProfile(profile_path);
  DCHECK(profile);
  if (profile) {
    // Deleting a profile will also close all its windows, so make sure
    // we mark it as being from a profile close/delete so we can avoid
    // any confirmation dialogs that might allow the user to abort
    // the window close.
    extensions::VivaldiWindowsAPI::WindowsForProfileClosing(profile);
  }

  bool delete_profile_allowed = signin_util::IsProfileDeletionAllowed(profile);

  if (delete_profile_allowed) {
    webui::DeleteProfileAtPath(profile_path,
                               ProfileMetrics::DELETE_PROFILE_SETTINGS);
  }

  return RespondNow(ArgumentList(Results::Create(delete_profile_allowed)));
}

ExtensionFunction::ResponseAction
RuntimePrivateHasDesktopShortcutFunction::Run() {
  namespace Results = vivaldi::runtime_private::HasDesktopShortcut::Results;
#if BUILDFLAG(IS_WIN)
  if (ProfileShortcutManager::IsFeatureEnabled()) {
    ProfileShortcutManager* shortcut_manager =
        g_browser_process->profile_manager()->profile_shortcut_manager();
    DCHECK(shortcut_manager);
    Profile* profile = Profile::FromBrowserContext(browser_context());
    shortcut_manager->HasProfileShortcuts(
        profile->GetPath(),
        base::BindOnce(
            &RuntimePrivateHasDesktopShortcutFunction::OnHasProfileShortcuts,
            this));
    return RespondLater();
  } else {
    return RespondNow(ArgumentList(Results::Create(false, false)));
  }
#else
  return RespondNow(ArgumentList(Results::Create(false, false)));
#endif  // BUILDFLAG(IS_WIN)
}

void RuntimePrivateHasDesktopShortcutFunction::OnHasProfileShortcuts(
    bool has_shortcuts) {
  namespace Results = vivaldi::runtime_private::HasDesktopShortcut::Results;

  return Respond(ArgumentList(Results::Create(has_shortcuts, true)));
}

}  // namespace extensions
