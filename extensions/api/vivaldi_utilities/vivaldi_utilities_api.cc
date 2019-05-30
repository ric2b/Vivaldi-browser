//
// Copyright (c) 2015-2018 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "app/vivaldi_version_info.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/power_monitor/power_monitor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/settings_utils.h"
#include "chrome/browser/ui/webui/site_settings_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/datasource/vivaldi_data_source_api.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/api/runtime/runtime_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "prefs/vivaldi_pref_names.h"
#include "ui/lights/razer_chroma_handler.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/vivaldi_ui_utils.h"
#include "url/url_constants.h"

#if defined(OS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/password_manager/password_manager_util_mac.h"
#endif

namespace {
bool IsValidUserId(const std::string& user_id) {
  uint64_t value;
  return !user_id.empty() && base::HexStringToUInt64(user_id, &value) &&
         value > 0;
}

}  // anonymous namespace

namespace extensions {

const char kBaseFileMappingUrl[] = "chrome://vivaldi-data/local-image/";

namespace {

ContentSetting vivContentSettingFromString(const std::string& name) {
  ContentSetting setting;
  content_settings::ContentSettingFromString(name, &setting);
  return setting;
}

ContentSettingsType vivContentSettingsTypeFromGroupName(
  const std::string& name) {
  return site_settings::ContentSettingsTypeFromGroupName(name);
}

std::string vivContentSettingToString(ContentSetting setting) {
  return content_settings::ContentSettingToString(setting);
}

}  // namespace

VivaldiUtilitiesAPI::VivaldiUtilitiesAPI(content::BrowserContext* context)
    : browser_context_(context),
      password_access_authenticator_(
          base::BindRepeating(&VivaldiUtilitiesAPI::OsReauthCall,
                              base::Unretained(this))) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this,
                                 vivaldi::utilities::OnScroll::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::utilities::OnDownloadManagerReady::kEventName);

  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor) {
    power_monitor->AddObserver(this);
  }
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(
          profile->GetOriginalProfile());
  manager->AddObserver(this);

  if (VivaldiRuntimeFeatures::IsEnabled(context, "razer_chroma_support")) {
    razer_chroma_handler_.reset(
        new RazerChromaHandler(Profile::FromBrowserContext(context)));
  }
}

VivaldiUtilitiesAPI::~VivaldiUtilitiesAPI() {}

void VivaldiUtilitiesAPI::Shutdown() {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  content::DownloadManager* manager =
    content::BrowserContext::GetDownloadManager(
      profile->GetOriginalProfile());
  manager->RemoveObserver(this);

  for (auto it : key_to_values_map_) {
    // Get rid of the allocated items
    delete it.second;
  }
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor) {
    power_monitor->RemoveObserver(this);
  }
  if (razer_chroma_handler_ && razer_chroma_handler_->IsAvailable()) {
    razer_chroma_handler_->Shutdown();
  }
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI> >::
    DestructorAtExit g_factory_utils = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI>*
VivaldiUtilitiesAPI::GetFactoryInstance() {
  return g_factory_utils.Pointer();
}

// static
void VivaldiUtilitiesAPI::ScrollType(content::BrowserContext* browser_context,
                                     int scrollType) {
  ::vivaldi::BroadcastEvent(vivaldi::utilities::OnScroll::kEventName,
                            vivaldi::utilities::OnScroll::Create(scrollType),
                            browser_context);
}

void VivaldiUtilitiesAPI::OnListenerAdded(const EventListenerInfo& details) {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
  if (details.event_name ==
      vivaldi::utilities::OnDownloadManagerReady::kEventName) {
    Profile* profile = Profile::FromBrowserContext(browser_context_);
    content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(
        profile->GetOriginalProfile());
    if (manager->IsManagerInitialized()) {
      ::vivaldi::BroadcastEvent(
          vivaldi::utilities::OnDownloadManagerReady::kEventName,
          vivaldi::utilities::OnDownloadManagerReady::Create(),
          browser_context_);
    }
  }
}

// Returns true if the key didn't not exist previously, false if it updated
// an existing value
bool VivaldiUtilitiesAPI::SetSharedData(const std::string& key,
                                        base::Value* value) {
  bool retval = true;
  auto it = key_to_values_map_.find(key);
  if (it != key_to_values_map_.end()) {
    delete it->second;
    key_to_values_map_.erase(it);
    retval = false;
  }
  key_to_values_map_.insert(std::make_pair(key, value->DeepCopy()));
  return retval;
}

// Looks up an existing key/value pair, returns nullptr if the key does not
// exist.
const base::Value* VivaldiUtilitiesAPI::GetSharedData(const std::string& key) {
  auto it = key_to_values_map_.find(key);
  if (it != key_to_values_map_.end()) {
    return it->second;
  }
  return nullptr;
}

bool VivaldiUtilitiesAPI::SetDialogPostion(int window_id,
                                           const std::string& dialog,
                                           const gfx::Rect& rect,
                                           const std::string& flow_direction) {
  bool retval = true;
  for (auto it = dialog_to_point_list_.begin();
       it != dialog_to_point_list_.end(); ++it) {
    if ((*it)->window_id() == window_id && dialog == (*it)->dialog_name()) {
      dialog_to_point_list_.erase(it);
      retval = false;
      break;
    }
  }
  dialog_to_point_list_.push_back(std::make_unique<DialogPosition>(
      window_id, dialog, rect, flow_direction));
  return retval;
}

gfx::Rect VivaldiUtilitiesAPI::GetDialogPosition(int window_id,
                                                 const std::string& dialog,
                                                 std::string* flow_direction) {
  for (auto& item : dialog_to_point_list_) {
    if (item->window_id() == window_id && dialog == item->dialog_name()) {
      if (flow_direction) {
        *flow_direction = item->flow_direction();
      }
      return item->rect();
    }
  }
  return gfx::Rect();
}

void VivaldiUtilitiesAPI::CloseAllThumbnailWindows() {
  // This will close all app windows currently generating thumbnails.
  AppWindowRegistry::AppWindowList windows =
      AppWindowRegistry::Get(browser_context_)
          ->GetAppWindowsForApp(::vivaldi::kVivaldiAppId);

  for (auto* window : windows) {
    if (window->thumbnail_window()) {
      window->CloseWindow();
    }
  }
}

bool VivaldiUtilitiesAPI::AuthenticateUser(gfx::NativeWindow window) {
  native_window_ = window;

  bool success = password_access_authenticator_.EnsureUserIsAuthenticated(
    password_manager::ReauthPurpose::VIEW_PASSWORD);

  native_window_ = nullptr;

  return success;
}

bool VivaldiUtilitiesAPI::OsReauthCall(
  password_manager::ReauthPurpose purpose) {
#if defined(OS_WIN)
  return password_manager_util_win::AuthenticateUser(native_window_, purpose);
#elif defined(OS_MACOSX)
  return password_manager_util_mac::AuthenticateUser(purpose);
#else
  return true;
#endif
}

bool VivaldiUtilitiesAPI::IsRazerChromaAvailable() {
  return (razer_chroma_handler_ && razer_chroma_handler_->IsAvailable());
}

bool VivaldiUtilitiesAPI::IsRazerChromaReady() {
  return (razer_chroma_handler_ && razer_chroma_handler_->IsReady());
}

// Set RGB color of the configured Razer Chroma devices.
bool VivaldiUtilitiesAPI::SetRazerChromaColors(RazerChromaColors& colors) {
  DCHECK(razer_chroma_handler_);
  if (!razer_chroma_handler_) {
    return false;
  }
  razer_chroma_handler_->SetColors(colors);
  return true;
}

void VivaldiUtilitiesAPI::OnPowerStateChange(bool on_battery_power) {
  // Implement if needed
}

void VivaldiUtilitiesAPI::OnSuspend() {
  ::vivaldi::BroadcastEvent(vivaldi::utilities::OnSuspend::kEventName,
                            vivaldi::utilities::OnSuspend::Create(),
                            browser_context_);
}

void VivaldiUtilitiesAPI::OnResume() {
  ::vivaldi::BroadcastEvent(vivaldi::utilities::OnResume::kEventName,
                            vivaldi::utilities::OnResume::Create(),
                            browser_context_);
}

// DownloadManager::Observer implementation
void VivaldiUtilitiesAPI::OnManagerInitialized() {
  ::vivaldi::BroadcastEvent(
      vivaldi::utilities::OnDownloadManagerReady::kEventName,
      vivaldi::utilities::OnDownloadManagerReady::Create(), browser_context_);
}

void VivaldiUtilitiesAPI::OnPasswordIconStatusChanged(int window_id,
                                                      bool state) {
  ::vivaldi::BroadcastEvent(
      vivaldi::utilities::OnPasswordIconStatusChanged::kEventName,
      vivaldi::utilities::OnPasswordIconStatusChanged::Create(window_id, state),
      browser_context_);
}

VivaldiUtilitiesAPI::DialogPosition::DialogPosition(
    int window_id,
    const std::string& dialog_name,
    const gfx::Rect rect,
    const std::string& flow_direction)
    : window_id_(window_id),
      dialog_name_(dialog_name),
      rect_(rect),
      flow_direction_(flow_direction) {}

ExtensionFunction::ResponseAction UtilitiesShowPasswordDialogFunction::Run() {
  Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
                      dispatcher()->GetAssociatedWebContents());
  chrome::ManagePasswordsForPage(browser);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction UtilitiesPrintFunction::Run() {
  using vivaldi::utilities::Print::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = params->tab_id;

  content::WebContents* tabstrip_contents =
    ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id, browser_context());
  if (tabstrip_contents) {
    Browser* browser = chrome::FindBrowserWithWebContents(tabstrip_contents);
    if (browser) {
      chrome::Print(browser);
    }
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
UtilitiesClearAllRecentlyClosedSessionsFunction::Run() {
  namespace Results =
      vivaldi::utilities::ClearAllRecentlyClosedSessions::Results;

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  bool result = false;
  if (tab_restore_service) {
    result = true;
    tab_restore_service->ClearEntries();
  }
  return RespondNow(ArgumentList(Results::Create(result)));
}

ExtensionFunction::ResponseAction UtilitiesGetUniqueUserIdFunction::Run() {
  using vivaldi::utilities::GetUniqueUserId::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string user_id = g_browser_process->local_state()->GetString(
      vivaldiprefs::kVivaldiUniqueUserId);

  if (!IsValidUserId(user_id)) {
    // Run this as task as this may do blocking IO like reading a file or a
    // platform registry.
    base::PostTaskWithTraits(
        FROM_HERE, { base::MayBlock() },
        base::BindOnce(
            &UtilitiesGetUniqueUserIdFunction::GetUniqueUserIdTask,
            this, params->legacy_user_id));
    return RespondLater();
  } else {
    RespondOnUIThread(user_id, false);
    return AlreadyResponded();
  }
}

void UtilitiesGetUniqueUserIdFunction::GetUniqueUserIdTask(
    const std::string& legacy_user_id) {
  // Note: We do not refresh the copy of the user id stored in the OS profile
  // if it is missing because we do not want standalone copies of vivaldi on USB
  // to spread their user id to all the computers they are run on.

  std::string user_id;
  bool is_new_user = false;
  if (!ReadUserIdFromOSProfile(&user_id) || !IsValidUserId(user_id)) {
    if (IsValidUserId(legacy_user_id)) {
      user_id = legacy_user_id;
    } else {
      uint64_t random = base::RandUint64();
      user_id = base::StringPrintf("%016" PRIX64, random);
      is_new_user = true;
    }
    WriteUserIdToOSProfile(user_id);
  }

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&UtilitiesGetUniqueUserIdFunction::RespondOnUIThread, this,
                     user_id, is_new_user));
}

void UtilitiesGetUniqueUserIdFunction::RespondOnUIThread(
    const std::string& user_id,
    bool is_new_user) {
  namespace Results = vivaldi::utilities::GetUniqueUserId::Results;

  g_browser_process->local_state()->SetString(
      vivaldiprefs::kVivaldiUniqueUserId, user_id);

  Respond(ArgumentList(Results::Create(user_id, is_new_user)));
}

ExtensionFunction::ResponseAction UtilitiesIsTabInLastSessionFunction::Run() {
  using vivaldi::utilities::IsTabInLastSession::Params;
  namespace Results = vivaldi::utilities::IsTabInLastSession::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tabId;
  if (!base::StringToInt(params->tab_id, &tabId)) {
    return RespondNow(Error("TabId is not an int - "+params->tab_id));
  }

  bool is_in_session = false;
  content::WebContents* contents;

  if (!ExtensionTabUtil::GetTabById(tabId, browser_context(), true, nullptr,
                                    nullptr, &contents, nullptr)) {
    return RespondNow(Error("TabId is found - "+params->tab_id));
  }
  // Both the profile and navigation entries are marked if they are
  // loaded from a session, so check both.
  if (Profile::FromBrowserContext(browser_context())->restored_last_session()) {
    content::NavigationEntry* entry =
        contents->GetController().GetVisibleEntry();
    is_in_session = entry && entry->IsRestored();
  }
  return RespondNow(ArgumentList(Results::Create(is_in_session)));
}

UtilitiesIsUrlValidFunction::UtilitiesIsUrlValidFunction() {}

UtilitiesIsUrlValidFunction::~UtilitiesIsUrlValidFunction() {}

// Based on OnDefaultProtocolClientWorkerFinished in
// external_protocol_handler.cc
void UtilitiesIsUrlValidFunction::OnDefaultProtocolClientWorkerFinished(
    shell_integration::DefaultWebClientState state) {
  namespace Results = vivaldi::utilities::IsUrlValid::Results;

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If we get here, either we are not the default or we cannot work out
  // what the default is, so we proceed.
  if (prompt_user_ && state == shell_integration::NOT_DEFAULT) {
    // Ask the user if they want to allow the protocol, but only
    // if the url is a valid formatted url.
    result_.external_handler = result_.url_valid;
  }
  Respond(ArgumentList(Results::Create(result_)));
}

ExtensionFunction::ResponseAction UtilitiesIsUrlValidFunction::Run() {
  using vivaldi::utilities::IsUrlValid::Params;
  namespace Results = vivaldi::utilities::IsUrlValid::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url(params->url);

  result_.url_valid = !params->url.empty() && url.is_valid();
  result_.scheme_valid =
      URLPattern::IsValidSchemeForExtensions(url.scheme()) ||
      url.SchemeIs(url::kJavaScriptScheme) || url.SchemeIs(url::kDataScheme) ||
      url.SchemeIs(url::kMailToScheme) || url.spec() == url::kAboutBlankURL ||
      url.SchemeIs(content::kViewSourceScheme);
  result_.scheme_parsed = url.scheme();
  result_.normalized_url = url.is_valid() ? url.spec() : "";
  result_.external_handler = false;

  if (result_.url_valid && result_.scheme_valid) {
    // We have valid schemes and url, no need to check for external handler.
    return RespondNow(ArgumentList(Results::Create(result_)));
  }
  // We have an invalid scheme, let's see if it's an external handler.
  // The worker creates tasks with references to itself and puts them into
  // message loops.
  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState(
          url.scheme(), Profile::FromBrowserContext(browser_context()));
  prompt_user_ = block_state == ExternalProtocolHandler::UNKNOWN;

  auto default_protocol_worker = base::MakeRefCounted<
      shell_integration::DefaultProtocolClientWorker>(
      base::BindRepeating(
          &UtilitiesIsUrlValidFunction::OnDefaultProtocolClientWorkerFinished,
          this),
      url.scheme());

  // StartCheckIsDefault takes ownership and releases everything once all
  // background activities finishes
  default_protocol_worker->StartCheckIsDefault();
  return RespondLater();
}

ExtensionFunction::ResponseAction UtilitiesGetSelectedTextFunction::Run() {
  using vivaldi::utilities::GetSelectedText::Params;
  namespace Results = vivaldi::utilities::GetSelectedText::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tabId;
  if (!base::StringToInt(params->tab_id, &tabId))
    return RespondNow(Error("TabId is not a string - "+params->tab_id));

  std::string text;
  content::WebContents* web_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tabId, browser_context());
  if (web_contents) {
    content::RenderWidgetHostView* rwhv =
        web_contents->GetRenderWidgetHostView();
    if (rwhv) {
      text = base::UTF16ToUTF8(rwhv->GetSelectedText());
    }
  }

  return RespondNow(ArgumentList(Results::Create(text)));
}

ExtensionFunction::ResponseAction UtilitiesCreateUrlMappingFunction::Run() {
  using vivaldi::utilities::CreateUrlMapping::Params;
  namespace Results = vivaldi::utilities::CreateUrlMapping::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiDataSourcesAPI* api =
      VivaldiDataSourcesAPI::GetFactoryInstance()->Get(browser_context());

  // PathExists() triggers IO restriction.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  std::string file = params->local_path;
  std::string guid = base::GenerateGUID();
  base::FilePath path = base::FilePath::FromUTF8Unsafe(file);
  if (!base::PathExists(path)) {
    return RespondNow(Error("File does not exists: " + file));
  }
  if (!api->AddMapping(guid, path)) {
    return RespondNow(Error("Mapping for file failed: " + file));
  }
  std::string retval = kBaseFileMappingUrl + guid;
  return RespondNow(ArgumentList(Results::Create(retval)));
}

ExtensionFunction::ResponseAction UtilitiesRemoveUrlMappingFunction::Run() {
  using vivaldi::utilities::RemoveUrlMapping::Params;
  namespace Results = vivaldi::utilities::RemoveUrlMapping::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiDataSourcesAPI* api =
    VivaldiDataSourcesAPI::GetFactoryInstance()->Get(browser_context());

  // Extract the ID from the given url first.
  GURL url(params->url);
  std::string id = url.ExtractFileName();

  bool success = api->RemoveMapping(id);

  return RespondNow(ArgumentList(Results::Create(success)));
}

namespace {
// Converts file extensions to a ui::SelectFileDialog::FileTypeInfo.
ui::SelectFileDialog::FileTypeInfo ConvertExtensionsToFileTypeInfo(
    const std::vector<vivaldi::utilities::FileExtension>&
        extensions) {
  ui::SelectFileDialog::FileTypeInfo file_type_info;

  for (const auto& item : extensions) {
    base::FilePath::StringType allowed_extension =
      base::FilePath::FromUTF8Unsafe(item.ext).value();

    // FileTypeInfo takes a nested vector like [["htm", "html"], ["txt"]] to
    // group equivalent extensions, but we don't use this feature here.
    std::vector<base::FilePath::StringType> inner_vector;
    inner_vector.push_back(allowed_extension);
    file_type_info.extensions.push_back(inner_vector);
  }
  return file_type_info;
}
}  // namespace

UtilitiesSelectFileFunction::UtilitiesSelectFileFunction() {}

UtilitiesSelectFileFunction::~UtilitiesSelectFileFunction() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

ExtensionFunction::ResponseAction UtilitiesSelectFileFunction::Run() {
  using vivaldi::utilities::SelectFile::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::string16 title;

  if (params->title.get()) {
    title = base::UTF8ToUTF16(*params->title.get());
  }

  ui::SelectFileDialog::FileTypeInfo file_type_info;

  if (params->accepts.get()) {
    file_type_info = ConvertExtensionsToFileTypeInfo(*params->accepts.get());
  }
  file_type_info.include_all_files = true;

  AddRef();

  select_file_dialog_ = ui::SelectFileDialog::Create(this, nullptr);

  content::WebContents* web_contents = dispatcher()->GetAssociatedWebContents();
  gfx::NativeWindow window =
      web_contents ? platform_util::GetTopLevel(web_contents->GetNativeView())
                   : nullptr;

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, title,
      base::FilePath(), &file_type_info, 0, base::FilePath::StringType(),
      window, nullptr);

  return RespondLater();
}

void UtilitiesSelectFileFunction::FileSelected(const base::FilePath& path,
                                               int index,
                                               void* params) {
  namespace Results = vivaldi::utilities::SelectFile::Results;

  Respond(ArgumentList(Results::Create(path.AsUTF8Unsafe())));
  Release();
}

void UtilitiesSelectFileFunction::FileSelectionCanceled(void* params) {
  Respond(Error("File selection aborted."));
  Release();
}

ExtensionFunction::ResponseAction UtilitiesGetVersionFunction::Run() {
  namespace Results = vivaldi::utilities::GetVersion::Results;

  return RespondNow(ArgumentList(Results::Create(
      ::vivaldi::GetVivaldiVersionString(), version_info::GetVersionNumber())));
}

ExtensionFunction::ResponseAction UtilitiesSetSharedDataFunction::Run() {
  using vivaldi::utilities::SetSharedData::Params;
  namespace Results = vivaldi::utilities::SetSharedData::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiUtilitiesAPI* api =
      VivaldiUtilitiesAPI::GetFactoryInstance()->Get(browser_context());

  bool found = api->SetSharedData(params->key_value_pair.key,
                                  params->key_value_pair.value.get());
  // Respond before sending an event
  Respond(ArgumentList(Results::Create(found)));

  ::vivaldi::BroadcastEvent(
      vivaldi::utilities::OnSharedDataUpdated::kEventName,
      vivaldi::utilities::OnSharedDataUpdated::Create(
          params->key_value_pair.key, *params->key_value_pair.value),
      browser_context());
  return AlreadyResponded();
}

ExtensionFunction::ResponseAction UtilitiesGetSharedDataFunction::Run() {
  using vivaldi::utilities::GetSharedData::Params;
  namespace Results = vivaldi::utilities::GetSharedData::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiUtilitiesAPI* api =
      VivaldiUtilitiesAPI::GetFactoryInstance()->Get(browser_context());

  const base::Value* value = api->GetSharedData(params->key_value_pair.key);
  return RespondNow(ArgumentList(
      Results::Create(value ? *value : *params->key_value_pair.value.get())));
}

ExtensionFunction::ResponseAction UtilitiesGetSystemDateFormatFunction::Run() {
  namespace Results = vivaldi::utilities::GetSystemDateFormat::Results;

  vivaldi::utilities::DateFormats date_formats;
  if (!ReadDateFormats(&date_formats)) {
    return RespondNow(Error(
        "Error reading date formats or not implemented on mac/linux yet"));
  } else {
    return RespondNow(ArgumentList(Results::Create(date_formats)));
  }
}

ExtensionFunction::ResponseAction UtilitiesSetLanguageFunction::Run() {
  using vivaldi::utilities::SetLanguage::Params;
  namespace Results = vivaldi::utilities::SetLanguage::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string& language_code = params->locale;

  DCHECK(!language_code.empty());
  if (language_code.empty()) {
    return RespondNow(Error("Empty language code."));
  }
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetString(language::prefs::kApplicationLocale, language_code);

  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction UtilitiesGetLanguageFunction::Run() {
  namespace Results = vivaldi::utilities::GetLanguage::Results;

  PrefService* pref_service = g_browser_process->local_state();
  std::string language_code =
      pref_service->GetString(language::prefs::kApplicationLocale);
  if (language_code.empty()) {
    // NOTE(igor@vivaldi.com) The user has never set the language
    // explicitly, so use one deduced from the system default settings.
    language_code = g_browser_process->GetApplicationLocale();
  }

  DCHECK(!language_code.empty());
  if (language_code.empty()) {
    return RespondNow(Error("Empty language code."));
  }
  return RespondNow(ArgumentList(Results::Create(language_code)));
}

UtilitiesSetVivaldiAsDefaultBrowserFunction::
    ~UtilitiesSetVivaldiAsDefaultBrowserFunction() {
  if (!did_respond()) {
    Respond(Error("no reply"));
  }
}

void UtilitiesSetVivaldiAsDefaultBrowserFunction::
    OnDefaultBrowserWorkerFinished(
        shell_integration::DefaultWebClientState state) {
  namespace Results = vivaldi::utilities::SetVivaldiAsDefaultBrowser::Results;

  if (state == shell_integration::IS_DEFAULT) {
    Respond(ArgumentList(Results::Create(true)));
  } else if (state == shell_integration::NOT_DEFAULT) {
    if (shell_integration::CanSetAsDefaultBrowser() ==
        shell_integration::SET_DEFAULT_NOT_ALLOWED) {
    } else {
      // Not default browser
      Respond(ArgumentList(Results::Create(false)));
    }
  } else if (state == shell_integration::UNKNOWN_DEFAULT) {
  } else {
    return;  // Still processing.
  }
}

ExtensionFunction::ResponseAction
UtilitiesSetVivaldiAsDefaultBrowserFunction::Run() {
  auto default_browser_worker =
      base::MakeRefCounted<shell_integration::DefaultBrowserWorker>(
          base::BindRepeating(&UtilitiesSetVivaldiAsDefaultBrowserFunction::
                                  OnDefaultBrowserWorkerFinished,
                              this));
  // StartSetAsDefault takes ownership and releases everything once all
  // background activities finishes.
  default_browser_worker->StartSetAsDefault();
  return RespondLater();
}

UtilitiesIsVivaldiDefaultBrowserFunction::
    ~UtilitiesIsVivaldiDefaultBrowserFunction() {
  if (!did_respond()) {
    Respond(Error("no reply"));
  }
}

ExtensionFunction::ResponseAction
UtilitiesIsVivaldiDefaultBrowserFunction::Run() {
  auto default_browser_worker =
      base::MakeRefCounted<shell_integration::DefaultBrowserWorker>(
          base::BindRepeating(&UtilitiesIsVivaldiDefaultBrowserFunction::
                                  OnDefaultBrowserWorkerFinished,
                              this));
  // StartCheckIsDefault takes ownership and releases everything once all
  // background activities finishes.
  default_browser_worker->StartCheckIsDefault();
  return RespondLater();
}

// shell_integration::DefaultWebClientObserver implementation.
void UtilitiesIsVivaldiDefaultBrowserFunction::OnDefaultBrowserWorkerFinished(
    shell_integration::DefaultWebClientState state) {
  namespace Results = vivaldi::utilities::IsVivaldiDefaultBrowser::Results;

  if (state == shell_integration::IS_DEFAULT) {
    Respond(ArgumentList(Results::Create(true)));
  } else if (state == shell_integration::NOT_DEFAULT) {
    if (shell_integration::CanSetAsDefaultBrowser() ==
        shell_integration::SET_DEFAULT_NOT_ALLOWED) {
    } else {
      // Not default browser
      Respond(ArgumentList(Results::Create(false)));
    }
  } else if (state == shell_integration::UNKNOWN_DEFAULT) {
  } else {
    return;  // Still processing.
  }
}

ExtensionFunction::ResponseAction
UtilitiesLaunchNetworkSettingsFunction::Run() {
  namespace Results = vivaldi::utilities::LaunchNetworkSettings::Results;

  settings_utils::ShowNetworkProxySettings(NULL);
  return RespondNow(ArgumentList(Results::Create("")));
}

ExtensionFunction::ResponseAction UtilitiesSavePageFunction::Run() {
  namespace Results = vivaldi::utilities::SavePage::Results;

  Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
      dispatcher()->GetAssociatedWebContents());

  content::WebContents* current_tab =
      browser->tab_strip_model()->GetActiveWebContents();
  current_tab->OnSavePage();
  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction UtilitiesOpenPageFunction::Run() {
  namespace Results = vivaldi::utilities::OpenPage::Results;

  Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
      dispatcher()->GetAssociatedWebContents());
  browser->OpenFile();
  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction
UtilitiesSetDefaultContentSettingsFunction::Run() {
  using vivaldi::utilities::SetDefaultContentSettings::Params;
  namespace Results = vivaldi::utilities::SetDefaultContentSettings::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string& content_settings = params->content_setting;
  std::string& value = params->value;

  ContentSetting default_setting = vivContentSettingFromString(value);
  //
  ContentSettingsType content_type =
      vivContentSettingsTypeFromGroupName(content_settings);

  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  map->SetDefaultContentSetting(content_type, default_setting);

  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction
UtilitiesGetDefaultContentSettingsFunction::Run() {
  using vivaldi::utilities::GetDefaultContentSettings::Params;
  namespace Results = vivaldi::utilities::GetDefaultContentSettings::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string& content_settings = params->content_setting;
  std::string def_provider = "";
  ContentSettingsType content_type =
      vivContentSettingsTypeFromGroupName(content_settings);
  ContentSetting default_setting;
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();

  default_setting = HostContentSettingsMapFactory::GetForProfile(profile)
                        ->GetDefaultContentSetting(content_type, &def_provider);

  std::string setting = vivContentSettingToString(default_setting);

  return RespondNow(ArgumentList(Results::Create(setting)));
}

ExtensionFunction::ResponseAction
UtilitiesSetBlockThirdPartyCookiesFunction::Run() {
  using vivaldi::utilities::SetBlockThirdPartyCookies::Params;
  namespace Results = vivaldi::utilities::SetBlockThirdPartyCookies::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool block3rdparty = params->block3rd_party;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* service = profile->GetOriginalProfile()->GetPrefs();
  service->SetBoolean(prefs::kBlockThirdPartyCookies, block3rdparty);

  bool blockThirdParty = service->GetBoolean(prefs::kBlockThirdPartyCookies);

  return RespondNow(ArgumentList(Results::Create(blockThirdParty)));
}

ExtensionFunction::ResponseAction
UtilitiesGetBlockThirdPartyCookiesFunction::Run() {
  namespace Results = vivaldi::utilities::GetBlockThirdPartyCookies::Results;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* service = profile->GetOriginalProfile()->GetPrefs();
  bool blockThirdParty = service->GetBoolean(prefs::kBlockThirdPartyCookies);

  return RespondNow(ArgumentList(Results::Create(blockThirdParty)));
}

ExtensionFunction::ResponseAction UtilitiesOpenTaskManagerFunction::Run() {
  namespace Results = vivaldi::utilities::OpenTaskManager::Results;

  Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
      dispatcher()->GetAssociatedWebContents());

  chrome::OpenTaskManager(browser);
  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction UtilitiesGetStartupActionFunction::Run() {
  namespace Results = vivaldi::utilities::GetStartupAction::Results;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const SessionStartupPref startup_pref =
      SessionStartupPref::GetStartupPref(profile->GetPrefs());

  std::string startupRes;
  switch (startup_pref.type) {
    default:
    case SessionStartupPref::LAST:
      startupRes = "last";
      break;
    case SessionStartupPref::VIVALDI_HOMEPAGE:
      startupRes = "homepage";
      break;
    case SessionStartupPref::DEFAULT:
      startupRes = "speeddial";
      break;
    case SessionStartupPref::URLS:
      startupRes = "urls";
      break;
  }
  return RespondNow(ArgumentList(Results::Create(startupRes)));
}

ExtensionFunction::ResponseAction UtilitiesSetStartupActionFunction::Run() {
  using vivaldi::utilities::SetStartupAction::Params;
  namespace Results = vivaldi::utilities::SetStartupAction::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string& content_settings = params->startup;
  SessionStartupPref startup_pref(SessionStartupPref::LAST);

  if (content_settings == "last") {
    startup_pref.type = SessionStartupPref::LAST;
  } else if (content_settings == "homepage") {
    startup_pref.type = SessionStartupPref::VIVALDI_HOMEPAGE;
  } else if (content_settings == "speeddial") {
    startup_pref.type = SessionStartupPref::DEFAULT;
  } else if (content_settings == "urls") {
    startup_pref.type = SessionStartupPref::URLS;
  }

  // SessionStartupPref will erase existing urls regardless of applied type so
  // we need to specify the list for the "url" type every time.
  for (size_t i = 0; i < params->urls.size(); ++i) {
    startup_pref.urls.push_back(GURL(params->urls[i]));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* prefs = profile->GetPrefs();

  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  return RespondNow(ArgumentList(Results::Create(content_settings)));
}

ExtensionFunction::ResponseAction UtilitiesCanShowWelcomePageFunction::Run() {
  namespace Results = vivaldi::utilities::CanShowWelcomePage::Results;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  bool force_first_run = command_line->HasSwitch(switches::kForceFirstRun);
  bool no_first_run = command_line->HasSwitch(switches::kNoFirstRun);
  bool can_show_welcome = (force_first_run || !no_first_run);

  return RespondNow(ArgumentList(Results::Create(can_show_welcome)));
}

ExtensionFunction::ResponseAction UtilitiesCanShowWhatsNewPageFunction::Run() {
  namespace Results = vivaldi::utilities::CanShowWhatsNewPage::Results;

  bool can_show_whats_new = false;

  // Show new features tab only for official final builds.
#if defined(OFFICIAL_BUILD) && \
   (BUILD_VERSION(VIVALDI_RELEASE) == VIVALDI_BUILD_PUBLIC_RELEASE)
  Profile* profile = Profile::FromBrowserContext(browser_context());
  bool version_changed = false;
  std::string version = ::vivaldi::GetVivaldiVersionString();
  std::string last_seen_version =
    profile->GetPrefs()->GetString(vivaldiprefs::kStartupLastSeenVersion);

  std::vector<std::string> version_array = base::SplitString(
      version, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<std::string> last_seen_array = base::SplitString(
      last_seen_version, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (version_array.size() != 4 || last_seen_array.size() != 4 ||
      (version_array[0] > last_seen_array[0]) /* major */ ||
      ((version_array[1] > last_seen_array[1]) /* minor */ &&
       (version_array[0] >= last_seen_array[0]))) {
    version_changed = true;
    profile->GetPrefs()->SetString(vivaldiprefs::kStartupLastSeenVersion,
                                   version);
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  bool force_first_run = command_line->HasSwitch(switches::kForceFirstRun);
  bool no_first_run = command_line->HasSwitch(switches::kNoFirstRun);
  can_show_whats_new = (version_changed || force_first_run) &&
      !no_first_run;
#endif

  return RespondNow(ArgumentList(Results::Create(can_show_whats_new)));
}

ExtensionFunction::ResponseAction UtilitiesSetDialogPositionFunction::Run() {
  using vivaldi::utilities::SetDialogPosition::Params;
  namespace Results = vivaldi::utilities::SetDialogPosition::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  gfx::Rect rect(params->position.left, params->position.top,
                 params->position.width, params->position.height);

  VivaldiUtilitiesAPI* api =
    VivaldiUtilitiesAPI::GetFactoryInstance()->Get(browser_context());

  bool found = api->SetDialogPostion(
      params->window_id, vivaldi::utilities::ToString(params->dialog_name),
      rect, vivaldi::utilities::ToString(params->flow_direction));

  return RespondNow(ArgumentList(Results::Create(found)));
}

ExtensionFunction::ResponseAction
UtilitiesIsRazerChromaAvailableFunction::Run() {
  namespace Results = vivaldi::utilities::IsRazerChromaAvailable::Results;

  VivaldiUtilitiesAPI* api =
      VivaldiUtilitiesAPI::GetFactoryInstance()->Get(browser_context());

  bool available = api->IsRazerChromaAvailable();
  return RespondNow(ArgumentList(Results::Create(available)));
}

ExtensionFunction::ResponseAction
UtilitiesIsRazerChromaReadyFunction::Run() {
  namespace Results = vivaldi::utilities::IsRazerChromaReady::Results;

  VivaldiUtilitiesAPI* api =
    VivaldiUtilitiesAPI::GetFactoryInstance()->Get(browser_context());

  bool available = api->IsRazerChromaReady();
  return RespondNow(ArgumentList(Results::Create(available)));
}

ExtensionFunction::ResponseAction UtilitiesSetRazerChromaColorFunction::Run() {
  using vivaldi::utilities::SetRazerChromaColor::Params;
  namespace Results = vivaldi::utilities::SetRazerChromaColor::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiUtilitiesAPI* api =
    VivaldiUtilitiesAPI::GetFactoryInstance()->Get(browser_context());

  RazerChromaColors colors;
  for (size_t i = 0; i < params->colors.size(); i++) {
    colors.push_back(SkColorSetRGB(params->colors[i].red,
                                   params->colors[i].green,
                                   params->colors[i].blue));
  }
  bool success = api->SetRazerChromaColors(colors);
  return RespondNow(ArgumentList(Results::Create(success)));
}

ExtensionFunction::ResponseAction
UtilitiesIsDownloadManagerReadyFunction::Run() {
  namespace Results = vivaldi::utilities::IsDownloadManagerReady::Results;

  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(
          Profile::FromBrowserContext(browser_context())->GetOriginalProfile());

  bool initialized = manager->IsManagerInitialized();
  return RespondNow(ArgumentList(Results::Create(initialized)));
}

}  // namespace extensions
