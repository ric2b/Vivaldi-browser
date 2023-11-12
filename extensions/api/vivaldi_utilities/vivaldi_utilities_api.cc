//
// Copyright (c) 2015-2018 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/power_monitor/power_monitor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/generated_cookie_prefs.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/language/core/browser/pref_names.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_uma_util.h"
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
#include "net/base/filename_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "app/vivaldi_version_info.h"
#include "base/vivaldi_switches.h"
#include "browser/translate/vivaldi_translate_server_request.h"
#include "browser/vivaldi_browser_finder.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "components/datasource/vivaldi_data_url_utils.h"
#include "components/datasource/vivaldi_image_store.h"
#include "components/locale/locale_kit.h"
#include "extensions/api/runtime/runtime_api.h"
#include "extensions/helper/file_selection_options.h"
#include "extensions/tools/vivaldi_tools.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "prefs/vivaldi_pref_names.h"
#include "sync/file_sync/file_store.h"
#include "sync/file_sync/file_store_factory.h"
#include "ui/lights/razer_chroma_handler.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_skia_utils.h"
#include "ui/vivaldi_ui_utils.h"

#if defined(ENABLE_RELAY_PROXY)
#include "proxy/launcher.h"
#endif

#if BUILDFLAG(IS_WIN)

#include <Windows.h>

#include "chrome/updater/util/win_util.h"

#if !defined(NTDDI_WIN10_19H1)
#error Windows 10.0.18362.0 SDK or higher required.
#endif

#include <mfapi.h>
#include "base/win/windows_version.h"
#include "chrome/browser/password_manager/password_manager_util_win.h"

#elif BUILDFLAG(IS_MAC)

#include "chrome/browser/password_manager/password_manager_util_mac.h"

#endif

using content_settings::CookieControlsMode;

namespace extensions {

namespace {
constexpr char kMutexNameKey[] = "name";
constexpr char kMutexReleaseTokenKey[] = "release_token";

// Helper for version testing. It is assumed that each array hold at least two
// elements.
static bool HasVersionChanged(std::vector<std::string> version,
                              std::vector<std::string> last_seen) {
  int last_seen_major, version_major, last_seen_minor, version_minor;
  if (base::StringToInt(version[0], &version_major) &&
      base::StringToInt(last_seen[0], &last_seen_major) &&
      base::StringToInt(version[1], &version_minor) &&
      base::StringToInt(last_seen[1], &last_seen_minor)) {
    return (version_major > last_seen_major) ||
           ((version_minor > last_seen_minor) &&
            (version_major >= last_seen_major));
  } else {
    return false;
  }
}

ContentSetting vivContentSettingFromString(const std::string& name) {
  ContentSetting setting;
  content_settings::ContentSettingFromString(name, &setting);
  return setting;
}

}  // namespace

VivaldiUtilitiesAPI::VivaldiUtilitiesAPI(content::BrowserContext* context)
    : browser_context_(context) {
  password_access_authenticator_.Init(
      base::BindRepeating(&VivaldiUtilitiesAPI::OsReauthCall,
                          base::Unretained(this)),
      base::BindRepeating(&VivaldiUtilitiesAPI::TimeoutCall,
                          base::Unretained(this)));

  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this,
                                 vivaldi::utilities::OnScroll::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::utilities::OnDownloadManagerReady::kEventName);

  base::PowerMonitor::AddPowerSuspendObserver(this);
  base::PowerMonitor::AddPowerStateObserver(this);

  razer_chroma_handler_.reset(
      new RazerChromaHandler(Profile::FromBrowserContext(context)));
}

void VivaldiUtilitiesAPI::PostProfileSetup() {
  // This call requires that ProfileKey::GetProtoDatabaseProvider() has
  // been initialized. That does not happen until *after*
  // the constructor of this object has been called.
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  content::DownloadManager* manager =
      profile->GetOriginalProfile()->GetDownloadManager();
  manager->AddObserver(this);
}

VivaldiUtilitiesAPI::~VivaldiUtilitiesAPI() {}

void VivaldiUtilitiesAPI::Shutdown() {
  base::PowerMonitor::RemovePowerStateObserver(this);
  base::PowerMonitor::RemovePowerSuspendObserver(this);

  if (razer_chroma_handler_ && razer_chroma_handler_->IsAvailable()) {
    razer_chroma_handler_->Shutdown();
  }
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI>>::
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
        profile->GetOriginalProfile()->GetDownloadManager();
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
                                        base::Value value) {
  return key_to_values_map_.insert_or_assign(key, std::move(value)).second;
}

// Looks up an existing key/value pair, returns nullptr if the key does not
// exist.
const base::Value* VivaldiUtilitiesAPI::GetSharedData(const std::string& key) {
  auto it = key_to_values_map_.find(key);
  if (it != key_to_values_map_.end()) {
    return &it->second;
  }
  return nullptr;
}

VivaldiUtilitiesAPI::MutexData::MutexData(int release_token)
    : release_token(release_token) {}
VivaldiUtilitiesAPI::MutexData::MutexData(MutexData&&) = default;
VivaldiUtilitiesAPI::MutexData& VivaldiUtilitiesAPI::MutexData::operator=(
    MutexData&&) = default;
VivaldiUtilitiesAPI::MutexData::~MutexData() = default;

bool VivaldiUtilitiesAPI::TakeMutex(const std::string& name,
                                    bool wait,
                                    MutexAvailableCallback callback) {
  // These tokens are just a precaution to prevent accidental release due to
  // coding errors. They only need to be unique-ish over the lifetime of the
  // application.
  static int next_release_token = 1;
  auto mutex = mutexes_.find(name);
  if (mutex != mutexes_.end()) {
    if (!wait)
      return false;
    mutex->second.wait_list.emplace(
        std::make_pair(next_release_token++, std::move(callback)));
    return false;
  }

  int release_token = next_release_token++;
  mutexes_.emplace(name, MutexData(release_token));
  std::move(callback).Run(release_token);
  return true;
}

bool VivaldiUtilitiesAPI::ReleaseMutex(const std::string& name,
                                       int release_token) {
  auto mutex = mutexes_.find(name);
  if (mutex == mutexes_.end() || mutex->second.release_token != release_token)
    return false;

  if (mutex->second.wait_list.empty()) {
    mutexes_.erase(mutex);
    return true;
  }

  int next_release_token = mutex->second.wait_list.front().first;
  mutex->second.release_token = next_release_token;
  std::move(mutex->second.wait_list.front().second).Run(next_release_token);
  mutex->second.wait_list.pop();
  return true;
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

// static
bool VivaldiUtilitiesAPI::AuthenticateUser(
    content::WebContents* web_contents,
    password_manager::PasswordAccessAuthenticator::AuthResultCallback
        callback) {
  VivaldiUtilitiesAPI* api =
      GetFactoryInstance()->Get(web_contents->GetBrowserContext());
  DCHECK(api);
  if (!api)
    return false;

  api->native_window_ =
      platform_util::GetTopLevel(web_contents->GetNativeView());

  api->password_access_authenticator_.EnsureUserIsAuthenticated(
      password_manager::ReauthPurpose::VIEW_PASSWORD,
      base::BindOnce(std::move(callback)));
  api->native_window_ = nullptr;

  return false;
}

void VivaldiUtilitiesAPI::OsReauthCall(
    password_manager::ReauthPurpose purpose,
    password_manager::PasswordAccessAuthenticator::AuthResultCallback
        callback) {
#if BUILDFLAG(IS_WIN)
  bool result = password_manager_util_win::AuthenticateUser(
      native_window_,
      password_manager_util_win::GetMessageForLoginPrompt(purpose));
  std::move(callback).Run(result);
#elif BUILDFLAG(IS_MAC)
  bool result = password_manager_util_mac::AuthenticateUser(
      password_manager_util_mac::GetMessageForLoginPrompt(purpose));
  std::move(callback).Run(result);
#else
  std::move(callback).Run(true);
#endif
}

void VivaldiUtilitiesAPI::TimeoutCall() {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile);
  if (router)
    router->OnPasswordManagerAuthTimeout();
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

// DownloadManager::Observer implementation
void VivaldiUtilitiesAPI::ManagerGoingDown(content::DownloadManager* manager) {
  manager->RemoveObserver(this);
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
  using vivaldi::utilities::ShowPasswordDialog::Params;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  Browser* browser = ::vivaldi::FindBrowserByWindowId(params->window_id);
  if (browser) {
    chrome::ManagePasswordsForPage(browser);
    return RespondNow(NoArguments());
  }
  return RespondNow(Error("No Browser instance."));
}

ExtensionFunction::ResponseAction UtilitiesPrintFunction::Run() {
  using vivaldi::utilities::Print::Params;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Browser* browser = ::vivaldi::FindBrowserByWindowId(params->window_id);
  if (!browser) {
    return RespondNow(Error("No Browser instance."));
  }
  chrome::Print(browser);
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

ExtensionFunction::ResponseAction UtilitiesIsTabInLastSessionFunction::Run() {
  using vivaldi::utilities::IsTabInLastSession::Params;
  namespace Results = vivaldi::utilities::IsTabInLastSession::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  content::WebContents* web_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
          params->tab_id, browser_context(), &error);
  if (!web_contents)
    return RespondNow(Error(error));

  // Both the profile and navigation entries are marked if they are
  // loaded from a session, so check both.
  bool is_in_session = false;
  if (Profile::FromBrowserContext(browser_context())->restored_last_session()) {
    content::NavigationEntry* entry =
        web_contents->GetController().GetVisibleEntry();
    is_in_session = entry && entry->IsRestored();
  }
  return RespondNow(ArgumentList(Results::Create(is_in_session)));
}

ExtensionFunction::ResponseAction UtilitiesIsUrlValidFunction::Run() {
  return RespondNow(Error("Unexpected call to the browser process"));
}

ExtensionFunction::ResponseAction UtilitiesCanOpenUrlExternallyFunction::Run() {
  using vivaldi::utilities::CanOpenUrlExternally::Params;
  namespace Results = vivaldi::utilities::CanOpenUrlExternally::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL url(params->url);

  bool result = false;
  do {
    if (!url.is_valid())
      break;

    // Check first if the user already decided to show or block the URL. If the
    // user has blocked it, return false to treat the blocked scheme as unknown
    // and send the url to the search engine rather than showing it. If the user
    // has accepted it, then presume that the scheme shows something useful and
    // return true.
    ExternalProtocolHandler::BlockState block_state =
        ExternalProtocolHandler::GetBlockState(
            url.scheme(), nullptr,
            Profile::FromBrowserContext(browser_context()));
    if (block_state != ExternalProtocolHandler::UNKNOWN) {
      result = (block_state == ExternalProtocolHandler::DONT_BLOCK);
      break;
    }

    // Ask OS if something handles the url. On Linux this always return
    // xdg-open, so there we effectively treat URL with any scheme as openable
    // until the user blocks it. But on Mac and Windows the behavior is more
    // sensible.
    std::u16string application_name =
        shell_integration::GetApplicationNameForScheme(url);
    if (!application_name.empty()) {
      result = true;
      break;
    }

    // As the last resort check if the browser handles the protocol itself
    // perhaps via an installed extension or something.
    //
    // TODO(igor@vivaldi.com): Figure out if this check is really necessary
    // given the above GetApplicationNameForProtocol() check?
    auto default_protocol_worker =
        base::MakeRefCounted<shell_integration::DefaultSchemeClientWorker>(
            url.scheme());

    // StartCheckIsDefault takes ownership and releases everything once all
    // background activities finishes
    default_protocol_worker->StartCheckIsDefault(
        base::BindOnce(&UtilitiesCanOpenUrlExternallyFunction::
                           OnDefaultProtocolClientWorkerFinished,
                       this));
    return RespondLater();
  } while (false);

  return RespondNow(ArgumentList(Results::Create(result)));
}

// Based on OnDefaultProtocolClientWorkerFinished in
// external_protocol_handler.cc
void UtilitiesCanOpenUrlExternallyFunction::
    OnDefaultProtocolClientWorkerFinished(
        shell_integration::DefaultWebClientState state) {
  namespace Results = vivaldi::utilities::CanOpenUrlExternally::Results;

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool can_open_with_browser = (state == shell_integration::IS_DEFAULT);
  Respond(ArgumentList(Results::Create(can_open_with_browser)));
}

ExtensionFunction::ResponseAction UtilitiesGetUrlFragmentsFunction::Run() {
  return RespondNow(Error("Unexpected call to the browser process"));
}

ExtensionFunction::ResponseAction UtilitiesUrlToThumbnailTextFunction::Run() {
  return RespondNow(Error("Unexpected call to the browser process"));
}

ExtensionFunction::ResponseAction UtilitiesGetSelectedTextFunction::Run() {
  using vivaldi::utilities::GetSelectedText::Params;
  namespace Results = vivaldi::utilities::GetSelectedText::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  content::WebContents* web_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
          params->tab_id, browser_context(), &error);
  if (!web_contents)
    return RespondNow(Error(error));

  std::string text;
  content::RenderWidgetHostView* rwhv = web_contents->GetRenderWidgetHostView();
  if (rwhv) {
    text = base::UTF16ToUTF8(rwhv->GetSelectedText());
  }

  return RespondNow(ArgumentList(Results::Create(text)));
}

ExtensionFunction::ResponseAction UtilitiesSelectFileFunction::Run() {
  using vivaldi::utilities::SelectFile::Params;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  FileSelectionOptions options(params->options.window_id);
  options.SetTitle(params->options.title);
  switch (params->options.type) {
    case vivaldi::utilities::SELECT_FILE_DIALOG_TYPE_FOLDER:
      options.SetType(ui::SelectFileDialog::SELECT_EXISTING_FOLDER);
      break;
    case vivaldi::utilities::SELECT_FILE_DIALOG_TYPE_FILE:
      options.SetType(ui::SelectFileDialog::SELECT_OPEN_FILE);
      break;
    case vivaldi::utilities::SELECT_FILE_DIALOG_TYPE_SAVE_FILE:
      options.SetType(ui::SelectFileDialog::SELECT_SAVEAS_FILE);
      break;
    default:
      NOTREACHED();
  }

  if (params->options.type !=
      vivaldi::utilities::SELECT_FILE_DIALOG_TYPE_FOLDER) {
    if (params->options.accepts) {
      for (const auto& item : *params->options.accepts) {
        options.AddExtension(item.ext);
      }
    }
    options.SetIncludeAllFiles();
  }

  if (params->options.default_path) {
    options.SetDefaultPath(*params->options.default_path);
  }

  std::move(options).RunDialog(
      base::BindOnce(&UtilitiesSelectFileFunction::OnFileSelected, this));

  return RespondLater();
}

void UtilitiesSelectFileFunction::OnFileSelected(base::FilePath path,
                                                 bool cancelled) {
  namespace Results = vivaldi::utilities::SelectFile::Results;

  // Presently JS does not need to distinguish between cancelled and error,
  // so just return the path.
  DCHECK(!cancelled || path.empty());
  Respond(ArgumentList(Results::Create(path.AsUTF8Unsafe())));
}

namespace {

// Common utility to parse JS input into ImagePlace. Return number of set
// parameters. The return value more than one indicate an error as only one
// parameter should be set. Both `theme_id` and `thumbnail_bookmark_id` can be
// null to indicate absence of the correspong keys in JS parameter object.
int ParseImagePlaceParams(VivaldiImageStore::ImagePlace& place,
                          const std::string& theme_id,
                          const std::string& thumbnail_bookmark_id,
                          std::string& error) {
  int case_count = 0;
  if (!thumbnail_bookmark_id.empty()) {
    if (++case_count == 1) {
      int64_t bookmark_id = 0;
      if (!base::StringToInt64(thumbnail_bookmark_id, &bookmark_id) ||
          bookmark_id <= 0) {
        error = "thumbnailBookmarkId is not a valid positive integer - " +
                thumbnail_bookmark_id;
      } else {
        place.SetBookmarkId(bookmark_id);
      }
    }
  }
  if (!theme_id.empty()) {
    if (++case_count == 1) {
      place.SetThemeId(theme_id);
    }
  }
  return case_count;
}

}  // namespace
ExtensionFunction::ResponseAction UtilitiesSelectLocalImageFunction::Run() {
  using vivaldi::utilities::SelectLocalImage::Params;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiImageStore::ImagePlace place;
  bool profile_image = false;
  std::string error;
  int case_count = ParseImagePlaceParams(
      place, params->params.theme_id.value_or(std::string()),
      params->params.thumbnail_bookmark_id.value_or(std::string()), error);
  if (!error.empty()) {
    return RespondNow(Error(error));
  }
  if (params->params.profile_image && *params->params.profile_image) {
    if (++case_count == 1) {
      profile_image = true;
    }
  }
  if (case_count != 1) {
    return RespondNow(
        Error("Exactly one of profileImage, themeId, "
              "thumbnailBookmarkId must be given"));
  }

  FileSelectionOptions options(params->params.window_id);
  options.SetTitle(params->params.title);
  options.SetType(ui::SelectFileDialog::SELECT_OPEN_FILE);
  options.AddExtensions(VivaldiImageStore::GetAllowedImageExtensions());

  std::move(options).RunDialog(
      base::BindOnce(&UtilitiesSelectLocalImageFunction::OnFileSelected, this,
                     std::move(place), profile_image));

  return RespondLater();
}

void UtilitiesSelectLocalImageFunction::OnFileSelected(
    VivaldiImageStore::ImagePlace place,
    bool store_as_profile_image,
    base::FilePath path,
    bool cancelled) {
  // Presently JS does not need to distinguish between cancelled and error,
  // so just return false when the path is empty here.
  if (path.empty()) {
    SendResult(std::string());
    return;
  }
  absl::optional<VivaldiImageStore::ImageFormat> format =
      VivaldiImageStore::FindFormatForPath(path);
  if (!format) {
    LOG(ERROR) << "Unsupported image format - " << path;
    SendResult(std::string());
    return;
  }

  if (place.IsBookmarkId()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&base::ReadFileToBytes, path),
        base::BindOnce(&UtilitiesSelectLocalImageFunction::OnContentRead, this,
                       place.GetBookmarkId()));
  } else if (!place.IsEmpty()) {
    VivaldiImageStore::UpdateMapping(
        browser_context(), std::move(place), *format, std::move(path),
        base::BindOnce(&UtilitiesSelectLocalImageFunction::SendResult, this));
  } else if (store_as_profile_image) {
    Profile* profile =
        Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
    std::string data_url = path.AsUTF8Unsafe();
    ::vivaldi::SetImagePathForProfilePath(
        vivaldiprefs::kVivaldiProfileImagePath, data_url,
        profile->GetPath().AsUTF8Unsafe());
    SendResult(data_url);
    RuntimeAPI::OnProfileAvatarChanged(profile);
  } else {
    NOTREACHED();
  }
}

void UtilitiesSelectLocalImageFunction::OnContentRead(
    int64_t bookmark_id,
    absl::optional<std::vector<uint8_t>> content) {
  if (!content) {
    SendResult("");
    return;
  }

  auto* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser_context());
  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(bookmark_model, bookmark_id);
  if (!node) {
    SendResult("");
    return;
  }

  auto* synced_file_store =
      SyncedFileStoreFactory::GetForBrowserContext(browser_context());
  std::string checksum = synced_file_store->SetLocalFile(
      node->uuid(), syncer::BOOKMARKS, *content);
  vivaldi_bookmark_kit::SetBookmarkThumbnail(
      bookmark_model, bookmark_id,
      vivaldi_data_url_utils::MakeUrl(
          vivaldi_data_url_utils::PathType::kSyncedStore, checksum));
  SendResult(checksum);
}

void UtilitiesSelectLocalImageFunction::SendResult(std::string data_url) {
  namespace Results = vivaldi::utilities::SelectLocalImage::Results;

  Respond(ArgumentList(Results::Create(!data_url.empty())));
}

UtilitiesStoreImageFunction::UtilitiesStoreImageFunction() = default;
UtilitiesStoreImageFunction::~UtilitiesStoreImageFunction() = default;

ExtensionFunction::ResponseAction UtilitiesStoreImageFunction::Run() {
  using vivaldi::utilities::StoreImage::Params;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  ParseImagePlaceParams(place_,
                        params->options.theme_id.value_or(std::string()),
                        std::string(), error);
  if (!error.empty())
    return RespondNow(Error(std::move(error)));
  if (params->options.data.has_value()) {
    if (params->options.data->empty()) {
      return RespondNow(Error("blob option cannot be empty"));
    }
    if (!params->options.mime_type.has_value() ||
        params->options.mime_type->empty()) {
      return RespondNow(Error("mimeType must be given"));
    }

    image_format_ =
        VivaldiImageStore::FindFormatForMimeType(*params->options.mime_type);
    if (!image_format_) {
      return RespondNow(
          Error("unsupported mimeType - " + *params->options.mime_type));
    }
    StoreImage(
        base::RefCountedBytes::TakeVector(&params->options.data.value()));
  } else if (params->options.url && !params->options.url->empty()) {
    GURL url(*params->options.url);
    if (!url.is_valid()) {
      return RespondNow(
          Error("url is not valid - " + url.possibly_invalid_spec()));
    }
    if (!url.SchemeIsFile()) {
      return RespondNow(Error("only file: url is supported - " + url.spec()));
    }
    base::FilePath file_path;
    if (!net::FileURLToFilePath(url, &file_path)) {
      return RespondNow(
          Error("url does not refer to a valid file path - " + url.spec()));
    }
    image_format_ = VivaldiImageStore::FindFormatForPath(file_path);
    if (!image_format_) {
      return RespondNow(Error("Unsupported image format - " +
                              file_path.BaseName().AsUTF8Unsafe()));
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&vivaldi_data_url_utils::ReadFileOnBlockingThread,
                       std::move(file_path), /*log_not_found=*/true),
        base::BindOnce(&UtilitiesStoreImageFunction::StoreImage, this));
  } else {
    return RespondNow(Error("Exactly one of data, url must be given"));
  }

  if (did_respond())
    return AlreadyResponded();
  return RespondLater();
}

void UtilitiesStoreImageFunction::StoreImage(
    scoped_refptr<base::RefCountedMemory> data) {
  if (!data) {
    SendResult(std::string());
    return;
  }
  if (!data->size()) {
    LOG(ERROR) << "Empty image";
    SendResult(std::string());
    return;
  }
  VivaldiImageStore::StoreImage(
      browser_context(), std::move(place_), *image_format_, std::move(data),
      base::BindOnce(&UtilitiesStoreImageFunction::SendResult, this));
}

void UtilitiesStoreImageFunction::SendResult(std::string data_url) {
  namespace Results = vivaldi::utilities::StoreImage::Results;

  Respond(ArgumentList(Results::Create(data_url)));
}

ExtensionFunction::ResponseAction UtilitiesGetVersionFunction::Run() {
  return RespondNow(Error("Unexpected call to the browser process"));
}

ExtensionFunction::ResponseAction UtilitiesGetFFMPEGStateFunction::Run() {
  namespace Results = vivaldi::utilities::GetFFMPEGState::Results;
#if BUILDFLAG(IS_LINUX)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kAutoTestMode)) {
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    std::string value;
    bool is_set = env->GetVar("VIVALDI_FFMPEG_FOUND", &value) && value == "NO";
    return RespondNow(ArgumentList(Results::Create(is_set)));
  }
#endif
  return RespondNow(ArgumentList(Results::Create(false)));
}

ExtensionFunction::ResponseAction UtilitiesSetSharedDataFunction::Run() {
  using vivaldi::utilities::SetSharedData::Params;
  namespace Results = vivaldi::utilities::SetSharedData::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiUtilitiesAPI* api =
      VivaldiUtilitiesAPI::GetFactoryInstance()->Get(browser_context());

  bool added = api->SetSharedData(params->key_value_pair.key,
                                  std::move(params->key_value_pair.value));
  // Respond before sending an event
  Respond(ArgumentList(Results::Create(added)));

  // Fetch value back from api and use in reply
  const base::Value* value = api->GetSharedData(params->key_value_pair.key);
  ::vivaldi::BroadcastEvent(vivaldi::utilities::OnSharedDataUpdated::kEventName,
                            vivaldi::utilities::OnSharedDataUpdated::Create(
                                params->key_value_pair.key,
                                value ? *value : params->key_value_pair.value),
                            browser_context());
  return AlreadyResponded();
}

ExtensionFunction::ResponseAction UtilitiesGetSharedDataFunction::Run() {
  using vivaldi::utilities::GetSharedData::Params;
  namespace Results = vivaldi::utilities::GetSharedData::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiUtilitiesAPI* api =
      VivaldiUtilitiesAPI::GetFactoryInstance()->Get(browser_context());

  const base::Value* value = api->GetSharedData(params->key_value_pair.key);
  return RespondNow(ArgumentList(
      Results::Create(value ? *value : params->key_value_pair.value)));
}

ExtensionFunction::ResponseAction UtilitiesTakeMutexFunction::Run() {
  using vivaldi::utilities::TakeMutex::Params;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiUtilitiesAPI* api =
      VivaldiUtilitiesAPI::GetFactoryInstance()->Get(browser_context());

  bool wait = params->wait ? *params->wait : true;

  if (api->TakeMutex(
          params->name, wait,
          base::BindOnce(&UtilitiesTakeMutexFunction::OnMutexAcquired, this,
                         params->name)))
    return AlreadyResponded();
  if (!wait)
    return RespondNow(Error("Mutex already held"));
  return RespondLater();
}

void UtilitiesTakeMutexFunction::OnMutexAcquired(std::string name,
                                                 int release_token) {
  namespace Results = vivaldi::utilities::TakeMutex::Results;
  base::Value mutex_handle(base::Value::Type::DICT);
  mutex_handle.GetDict().Set(kMutexNameKey, std::move(name));
  mutex_handle.GetDict().Set(kMutexReleaseTokenKey, release_token);
  Respond(ArgumentList(Results::Create(std::move(mutex_handle))));
}

ExtensionFunction::ResponseAction UtilitiesReleaseMutexFunction::Run() {
  using vivaldi::utilities::ReleaseMutex::Params;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string* name;
  absl::optional<int> release_token;

  VivaldiUtilitiesAPI* api =
      VivaldiUtilitiesAPI::GetFactoryInstance()->Get(browser_context());

  auto is_valid_handle = [&name, &release_token](base::Value* handle) {
    if (!handle->is_dict())
      return false;
    name = handle->GetDict().FindString(kMutexNameKey);
    if (!name)
      return false;
    release_token = handle->GetDict().FindInt(kMutexReleaseTokenKey);
    if (!release_token)
      return false;
    return true;
  };

  if (!is_valid_handle(&params->handle) ||
      !api->ReleaseMutex(*name, *release_token)) {
    return RespondNow(Error("Invalid token"));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction UtilitiesGetSystemDateFormatFunction::Run() {
  namespace Results = vivaldi::utilities::GetSystemDateFormat::Results;

  vivaldi::utilities::DateFormats date_formats;
  if (!ReadDateFormats(&date_formats)) {
    return RespondNow(
        Error("Error reading date formats or not implemented on "
              "mac/linux yet"));
  } else {
    return RespondNow(ArgumentList(Results::Create(date_formats)));
  }
}

ExtensionFunction::ResponseAction UtilitiesGetSystemCountryFunction::Run() {
  namespace Results = vivaldi::utilities::GetSystemCountry::Results;

  std::string country = locale_kit::GetUserCountry();
  return RespondNow(ArgumentList(Results::Create(country)));
}

ExtensionFunction::ResponseAction UtilitiesSetLanguageFunction::Run() {
  using vivaldi::utilities::SetLanguage::Params;
  namespace Results = vivaldi::utilities::SetLanguage::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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
      base::MakeRefCounted<shell_integration::DefaultBrowserWorker>();
  // StartSetAsDefault takes ownership and releases everything once all
  // background activities finishes.
  default_browser_worker->StartSetAsDefault(
      base::BindOnce(&UtilitiesSetVivaldiAsDefaultBrowserFunction::
                         OnDefaultBrowserWorkerFinished,
                     this));
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
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  namespace Results = vivaldi::utilities::IsVivaldiDefaultBrowser::Results;
  if (command_line.HasSwitch(switches::kNoDefaultBrowserCheck)) {
    // Pretend we are default already which will surpress the dialog
    // on startup.
    return RespondNow(ArgumentList(Results::Create(true)));
  }

  auto default_browser_worker =
      base::MakeRefCounted<shell_integration::DefaultBrowserWorker>();
  // StartCheckIsDefault takes ownership and releases everything once all
  // background activities finishes.
  default_browser_worker->StartCheckIsDefault(base::BindOnce(
      &UtilitiesIsVivaldiDefaultBrowserFunction::OnDefaultBrowserWorkerFinished,
      this));
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
  using vivaldi::utilities::LaunchNetworkSettings::Params;
  namespace Results = vivaldi::utilities::LaunchNetworkSettings::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiBrowserWindow* window =
      VivaldiBrowserWindow::FromId(params->window_id);
  if (!window) {
    return RespondNow(Error("No such window"));
  }

  settings_utils::ShowNetworkProxySettings(window->web_contents());

  return RespondNow(ArgumentList(Results::Create("")));
}

ExtensionFunction::ResponseAction UtilitiesSavePageFunction::Run() {
  using vivaldi::utilities::SavePage::Params;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  content::WebContents* web_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
          params->tab_id, browser_context(), &error);
  if (!web_contents)
    return RespondNow(Error(error));

  web_contents->OnSavePage();

  namespace Results = vivaldi::utilities::SavePage::Results;
  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction UtilitiesOpenPageFunction::Run() {
  using vivaldi::utilities::OpenPage::Params;
  namespace Results = vivaldi::utilities::OpenPage::Results;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  Browser* browser = ::vivaldi::FindBrowserByWindowId(params->window_id);
  if (!browser) {
    return RespondNow(Error("No browser with the supplied ID."));
  }
  browser->OpenFile();
  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction UtilitiesBroadcastMessageFunction::Run() {
  using vivaldi::utilities::BroadcastMessage::Params;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ::vivaldi::BroadcastEvent(
      vivaldi::utilities::OnBroadcastMessage::kEventName,
      vivaldi::utilities::OnBroadcastMessage::Create(params->message),
      browser_context());

  namespace Results = vivaldi::utilities::BroadcastMessage::Results;
  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction
UtilitiesSetDefaultContentSettingsFunction::Run() {
  using vivaldi::utilities::SetDefaultContentSettings::Params;
  namespace Results = vivaldi::utilities::SetDefaultContentSettings::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string& content_settings = params->content_setting;
  std::string& value = params->value;

  ContentSetting default_setting = vivContentSettingFromString(value);
  //
  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(content_settings);

  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  const content_settings::ContentSettingsInfo* info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(
          content_type);

  bool is_valid_settings_value = info->IsDefaultSettingValid(default_setting);
  DCHECK(is_valid_settings_value);
  if (is_valid_settings_value) {
    map->SetDefaultContentSetting(content_type, default_setting);
  }

  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction
UtilitiesGetDefaultContentSettingsFunction::Run() {
  using vivaldi::utilities::GetDefaultContentSettings::Params;
  namespace Results = vivaldi::utilities::GetDefaultContentSettings::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string& content_settings = params->content_setting;
  std::string def_provider = "";
  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(content_settings);
  ContentSetting default_setting;
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();

  default_setting = HostContentSettingsMapFactory::GetForProfile(profile)
                        ->GetDefaultContentSetting(content_type, &def_provider);

  std::string setting =
      content_settings::ContentSettingToString(default_setting);

  return RespondNow(ArgumentList(Results::Create(setting)));
}

namespace {
CookieControlsMode ToCookieControlsMode(
    vivaldi::utilities::CookieMode cookie_mode) {
  switch (cookie_mode) {
    case vivaldi::utilities::CookieMode::COOKIE_MODE_OFF:
      return CookieControlsMode::kOff;
    case vivaldi::utilities::CookieMode::COOKIE_MODE_BLOCKTHIRDPARTY:
      return CookieControlsMode::kBlockThirdParty;
    case vivaldi::utilities::CookieMode::
        COOKIE_MODE_BLOCKTHIRDPARTYINCOGNITOONLY:
      return CookieControlsMode::kIncognitoOnly;
    default:
      NOTREACHED() << "Incorrect cookie mode to the API";
      return CookieControlsMode::kBlockThirdParty;
  }
}

vivaldi::utilities::CookieMode ToCookieMode(CookieControlsMode mode) {
  using vivaldi::utilities::CookieMode;

  switch (mode) {
    case CookieControlsMode::kOff:
      return CookieMode::COOKIE_MODE_OFF;
    case CookieControlsMode::kBlockThirdParty:
      return CookieMode::COOKIE_MODE_BLOCKTHIRDPARTY;
    case CookieControlsMode::kIncognitoOnly:
      return CookieMode::COOKIE_MODE_BLOCKTHIRDPARTYINCOGNITOONLY;
    default:
      NOTREACHED() << "Incorrect cookie controls mode to the API";
      return CookieMode::COOKIE_MODE_BLOCKTHIRDPARTY;
  }
}

}  // namespace

ExtensionFunction::ResponseAction
UtilitiesSetBlockThirdPartyCookiesFunction::Run() {
  using vivaldi::utilities::SetBlockThirdPartyCookies::Params;
  namespace Results = vivaldi::utilities::SetBlockThirdPartyCookies::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* service = profile->GetOriginalProfile()->GetPrefs();
  CookieControlsMode mode = ToCookieControlsMode(params->cookie_mode);

  service->SetInteger(prefs::kCookieControlsMode, static_cast<int>(mode));

  return RespondNow(ArgumentList(Results::Create(true)));
}

ExtensionFunction::ResponseAction
UtilitiesGetBlockThirdPartyCookiesFunction::Run() {
  namespace Results = vivaldi::utilities::GetBlockThirdPartyCookies::Results;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* service = profile->GetOriginalProfile()->GetPrefs();
  CookieControlsMode mode = static_cast<CookieControlsMode>(
      service->GetInteger(prefs::kCookieControlsMode));

  vivaldi::utilities::CookieMode cookie_mode = ToCookieMode(mode);

  return RespondNow(ArgumentList(Results::Create(cookie_mode)));
}

ExtensionFunction::ResponseAction UtilitiesOpenTaskManagerFunction::Run() {
  using vivaldi::utilities::OpenTaskManager::Params;
  namespace Results = vivaldi::utilities::OpenTaskManager::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiBrowserWindow* window =
      VivaldiBrowserWindow::FromId(params->window_id);
  if (!window) {
    return RespondNow(Error("No such window"));
  }

  chrome::OpenTaskManager(window->browser());
  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction UtilitiesCreateQRCodeFunction::Run() {
  using vivaldi::utilities::CreateQRCode::Params;
  namespace Results = vivaldi::utilities::CreateQRCode::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  content::WebContents* web_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
          params->id, browser_context(), &error);
  if (!web_contents)
    return RespondNow(Error(error));
  qrcode_generator::QRCodeGeneratorBubbleController* bubble_controller =
      qrcode_generator::QRCodeGeneratorBubbleController::Get(web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  bubble_controller->ShowBubble(entry->GetURL());

  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction UtilitiesGetStartupActionFunction::Run() {
  namespace Results = vivaldi::utilities::GetStartupAction::Results;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const SessionStartupPref startup_pref = SessionStartupPref::GetStartupPref(
      profile->GetOriginalProfile()->GetPrefs());

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

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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
  PrefService* prefs = profile->GetOriginalProfile()->GetPrefs();

  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  return RespondNow(ArgumentList(Results::Create(content_settings)));
}

ExtensionFunction::ResponseAction UtilitiesCanShowWhatsNewPageFunction::Run() {
  namespace Results = vivaldi::utilities::CanShowWhatsNewPage::Results;
  vivaldi::utilities::WhatsNewResults results;

  results.show = false;
  results.firstrun = false;

  // Show new features tab only for official final builds.
  if (::vivaldi::IsBetaOrFinal()) {
    Profile* profile =
        Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
    bool version_changed = false;
    std::string version = ::vivaldi::GetVivaldiVersionString();
    std::string last_seen_version =
        profile->GetPrefs()->GetString(vivaldiprefs::kStartupLastSeenVersion);

    std::vector<std::string> version_array = base::SplitString(
        version, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    std::vector<std::string> last_seen_array =
        base::SplitString(last_seen_version, ".", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    if (version_array.size() != 4 || last_seen_array.size() != 4) {
      version_changed = true;
      results.firstrun = last_seen_array.size() == 0;
      profile->GetPrefs()->SetString(vivaldiprefs::kStartupLastSeenVersion,
                                     version);
    } else {
      version_changed = HasVersionChanged(version_array, last_seen_array);
      if (version_changed) {
        profile->GetPrefs()->SetString(vivaldiprefs::kStartupLastSeenVersion,
                                       version);
      }
    }

    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    bool force_first_run = command_line->HasSwitch(switches::kForceFirstRun);
    bool no_first_run = command_line->HasSwitch(switches::kNoFirstRun);
    results.show = (version_changed || force_first_run) && !no_first_run;
  }

  return RespondNow(ArgumentList(Results::Create(results)));
}

ExtensionFunction::ResponseAction UtilitiesSetDialogPositionFunction::Run() {
  using vivaldi::utilities::SetDialogPosition::Params;
  namespace Results = vivaldi::utilities::SetDialogPosition::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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

ExtensionFunction::ResponseAction UtilitiesIsRazerChromaReadyFunction::Run() {
  namespace Results = vivaldi::utilities::IsRazerChromaReady::Results;

  VivaldiUtilitiesAPI* api =
      VivaldiUtilitiesAPI::GetFactoryInstance()->Get(browser_context());

  bool available = api->IsRazerChromaReady();
  return RespondNow(ArgumentList(Results::Create(available)));
}

ExtensionFunction::ResponseAction UtilitiesSetRazerChromaColorFunction::Run() {
  using vivaldi::utilities::SetRazerChromaColor::Params;
  namespace Results = vivaldi::utilities::SetRazerChromaColor::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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
      Profile::FromBrowserContext(browser_context())
          ->GetOriginalProfile()
          ->GetDownloadManager();

  bool initialized = manager->IsManagerInitialized();
  return RespondNow(ArgumentList(Results::Create(initialized)));
}

ExtensionFunction::ResponseAction UtilitiesSetContentSettingsFunction::Run() {
  using vivaldi::utilities::SetContentSettings::Params;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string primary_pattern_string = params->details.primary_pattern;
  std::string secondary_pattern_string;
  if (params->details.secondary_pattern) {
    secondary_pattern_string = *params->details.secondary_pattern;
  }
  std::string type = params->details.type;
  std::string value = params->details.value;
  bool incognito = false;
  if (params->details.incognito) {
    incognito = *params->details.incognito;
  }

  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(type);
  ContentSetting setting;
  CHECK(content_settings::ContentSettingFromString(value, &setting));

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (incognito) {
    if (!profile->HasOffTheRecordProfile(Profile::OTRProfileID::PrimaryID())) {
      return RespondNow(NoArguments());
    }
    profile = profile->GetOffTheRecordProfile(
        Profile::OTRProfileID::PrimaryID(), false);
  }

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString(primary_pattern_string);
  ContentSettingsPattern secondary_pattern =
      secondary_pattern_string.empty()
          ? ContentSettingsPattern::Wildcard()
          : ContentSettingsPattern::FromString(secondary_pattern_string);

  // Clear any existing embargo status if the new setting isn't block.
  if (setting != CONTENT_SETTING_BLOCK) {
    GURL url(primary_pattern.ToString());
    if (url.is_valid()) {
      PermissionDecisionAutoBlockerFactory::GetForProfile(profile)
          ->RemoveEmbargoAndResetCounts(url, content_type);
    }
  }

  permissions::PermissionUmaUtil::ScopedRevocationReporter
      scoped_revocation_reporter(
          profile, primary_pattern, secondary_pattern, content_type,
          permissions::PermissionSourceUI::SITE_SETTINGS);

  map->SetContentSettingCustomScope(primary_pattern, secondary_pattern,
                                    content_type, setting);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction UtilitiesIsDialogOpenFunction::Run() {
  using vivaldi::utilities::IsDialogOpen::Params;
  namespace Results = vivaldi::utilities::IsDialogOpen::Results;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool visible = false;

  switch (params->dialog_name) {
    case vivaldi::utilities::DialogName::DIALOG_NAME_PASSWORD:
      if (PasswordBubbleViewBase::manage_password_bubble()) {
        visible =
            PasswordBubbleViewBase::manage_password_bubble()->GetVisible();
      }
      break;

    default:
    case vivaldi::utilities::DialogName::DIALOG_NAME_CHROMECAST:
      break;
  }
  return RespondNow(ArgumentList(Results::Create(visible)));
}

ExtensionFunction::ResponseAction UtilitiesFocusDialogFunction::Run() {
  using vivaldi::utilities::FocusDialog::Params;
  namespace Results = vivaldi::utilities::FocusDialog::Results;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool focused = false;

  switch (params->dialog_name) {
    case vivaldi::utilities::DialogName::DIALOG_NAME_PASSWORD:
      if (PasswordBubbleViewBase::manage_password_bubble()) {
        if (PasswordBubbleViewBase::manage_password_bubble()->CanActivate()) {
          PasswordBubbleViewBase::manage_password_bubble()->ActivateBubble();
          focused = true;
        }
      }
      break;

    default:
    case vivaldi::utilities::DialogName::DIALOG_NAME_CHROMECAST:
      break;
  }
  return RespondNow(ArgumentList(Results::Create(focused)));
}

ExtensionFunction::ResponseAction UtilitiesStartChromecastFunction::Run() {
  using vivaldi::utilities::StartChromecast::Params;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (media_router::MediaRouterEnabled(browser_context())) {
    Browser* browser = ::vivaldi::FindBrowserByWindowId(params->window_id);
    if (!browser) {
      return RespondNow(Error("No Browser instance."));
    }
    content::WebContents* current_tab =
        browser->tab_strip_model()->GetActiveWebContents();
    media_router::MediaRouterDialogController* dialog_controller =
        media_router::MediaRouterDialogController::GetOrCreateForWebContents(
            current_tab);
    if (dialog_controller) {
      dialog_controller->ShowMediaRouterDialog(
          media_router::MediaRouterDialogActivationLocation::PAGE);
    }
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
UtilitiesGetMediaAvailableStateFunction::Run() {
  namespace Results = vivaldi::utilities::GetMediaAvailableState::Results;
  bool is_available = true;
#if BUILDFLAG(IS_WIN)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kAutoTestMode)) {
    if (const absl::optional<OSVERSIONINFOEX> current_os_version =
            updater::GetOSVersion();
        current_os_version) {
      DWORD os_type = 0;
      ::GetProductInfo(current_os_version->dwMajorVersion,
                       current_os_version->dwMinorVersion, 0, 0, &os_type);

      // Only present on Vista+. All these 'N' versions of Windows come
      // without a media player or codecs.
      switch (os_type) {
        case PRODUCT_HOME_BASIC_N:
        case PRODUCT_BUSINESS_N:
        case PRODUCT_ENTERPRISE_N:
        case PRODUCT_ENTERPRISE_N_EVALUATION:
        case PRODUCT_ENTERPRISE_SUBSCRIPTION_N:
        case PRODUCT_ENTERPRISE_S_N:
        case PRODUCT_ENTERPRISE_S_N_EVALUATION:
        case PRODUCT_EDUCATION_N:
        case PRODUCT_PRO_FOR_EDUCATION_N:
        case PRODUCT_HOME_PREMIUM_N:
        case PRODUCT_ULTIMATE_N:
        case PRODUCT_PROFESSIONAL_N:
        case PRODUCT_PROFESSIONAL_S_N:
        case PRODUCT_PROFESSIONAL_STUDENT_N:
        case PRODUCT_STARTER_N:
        case PRODUCT_CORE_N:
          is_available = false;
          break;
      }
      if (!is_available) {
        // MFStartup triggers a delayload which crashes on startup if the
        // dll is not available, so ensure the dll is present first.
        HMODULE dll =
            ::LoadLibraryExW(L"mfplat.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
        if (dll) {
          // Only check N versions for media framework, otherwise just
          // assume all is fine and proceed.
          HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
          if (SUCCEEDED(hr)) {
            is_available = true;
            MFShutdown();
          }
          ::FreeLibrary(dll);
        }
      }
    }
  }
#endif
  return RespondNow(ArgumentList(Results::Create(is_available)));
}

ExtensionFunction::ResponseAction UtilitiesIsFirstRunFunction::Run() {
  namespace Results = vivaldi::utilities::IsFirstRun::Results;
  return RespondNow(
      ArgumentList(Results::Create(first_run::IsChromeFirstRun())));
}

UtilitiesGenerateQRCodeFunction::~UtilitiesGenerateQRCodeFunction() {}

UtilitiesGenerateQRCodeFunction::UtilitiesGenerateQRCodeFunction() {}

ExtensionFunction::ResponseAction UtilitiesGenerateQRCodeFunction::Run() {
  using vivaldi::utilities::GenerateQRCode::Params;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  dest_ = params->destination;

  qrcode_generator::mojom::GenerateQRCodeRequestPtr request =
      qrcode_generator::mojom::GenerateQRCodeRequest::New();
  request->data = params->data;
  request->render_module_style =
      qrcode_generator::mojom::ModuleStyle::DEFAULT_SQUARES;
  request->render_locator_style =
      qrcode_generator::mojom::LocatorStyle::DEFAULT_SQUARE;

  auto callback = base::BindOnce(
      &UtilitiesGenerateQRCodeFunction::OnCodeGeneratorResponse, this);
  // qr_generator_ is preserved as used by another process.
  qr_generator_ = std::make_unique<qrcode_generator::QRImageGenerator>();
  qr_generator_->GenerateQRCode(std::move(request), std::move(callback));

  return RespondLater();
}

void UtilitiesGenerateQRCodeFunction::OnCodeGeneratorResponse(
    const qrcode_generator::mojom::GenerateQRCodeResponsePtr response) {
  if (response->error_code !=
      qrcode_generator::mojom::QRCodeGeneratorError::NONE) {
    RespondOnUiThread("");
    return;
  }
  if (dest_ == vivaldi::utilities::CAPTURE_QR_DESTINATION_CLIPBOARD) {
    // Ignore everything else, we copy it raw to the clipboard.
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.Reset();
    scw.WriteImage(response->bitmap);
    RespondOnUiThread("");
    return;
  } else if (dest_ == vivaldi::utilities::CAPTURE_QR_DESTINATION_FILE) {
    base::FilePath path = DownloadPrefs::GetDefaultDownloadDirectory();
    if (path.empty()) {
      RespondOnUiThread("");
      return;
    }
    Profile* profile = Profile::FromBrowserContext(browser_context());
    PrefService* service = profile->GetOriginalProfile()->GetPrefs();
    std::string save_file_pattern =
        service->GetString(vivaldiprefs::kWebpagesCaptureSaveFilePattern);

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&::vivaldi::skia_utils::EncodeBitmapToFile,
                       std::move(path), std::move(response->bitmap),
                       ::vivaldi::skia_utils::ImageFormat::kPNG, 90),
        base::BindOnce(
            &UtilitiesGenerateQRCodeFunction::RespondOnUiThreadForFile, this));
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&::vivaldi::skia_utils::EncodeBitmapAsDataUrl,
                       std::move(response->bitmap),
                       ::vivaldi::skia_utils::ImageFormat::kPNG, 90),
        base::BindOnce(&UtilitiesGenerateQRCodeFunction::RespondOnUiThread,
                       this));
  }
}

void UtilitiesGenerateQRCodeFunction::RespondOnUiThreadForFile(
    base::FilePath path) {
  namespace Results = vivaldi::utilities::GenerateQRCode::Results;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  platform_util::ShowItemInFolder(profile, path);

  Respond(ArgumentList(Results::Create(path.AsUTF8Unsafe())));
}

void UtilitiesGenerateQRCodeFunction::RespondOnUiThread(
    std::string image_data) {
  namespace Results = vivaldi::utilities::GenerateQRCode::Results;

  Respond(ArgumentList(Results::Create(image_data)));
}

ExtensionFunction::ResponseAction UtilitiesGetGAPIKeyFunction::Run() {
  namespace Results = vivaldi::utilities::GetGAPIKey::Results;

#ifdef VIVALDI_GOOGLE_TASKS_API_KEY
  return RespondNow(
      ArgumentList(Results::Create(VIVALDI_GOOGLE_TASKS_API_KEY)));
#else
  return RespondNow(Error("No G API key defined"));
#endif  // VIVALDI_GOOGLE_TASKS_API_KEY
}

ExtensionFunction::ResponseAction UtilitiesGetGOAuthClientIdFunction::Run() {
  namespace Results = vivaldi::utilities::GetGOAuthClientId::Results;

#ifdef VIVALDI_GOOGLE_OAUTH_API_CLIENT_ID
  return RespondNow(
      ArgumentList(Results::Create(VIVALDI_GOOGLE_OAUTH_API_CLIENT_ID)));
#else
  return RespondNow(Error("No G client id defined"));
#endif  // VIVALDI_GOOGLE_OAUTH_API_CLIENT_ID
}

ExtensionFunction::ResponseAction
UtilitiesGetGOAuthClientSecretFunction::Run() {
  namespace Results = vivaldi::utilities::GetGOAuthClientSecret::Results;

#ifdef VIVALDI_GOOGLE_OAUTH_API_CLIENT_SECRET
  return RespondNow(
      ArgumentList(Results::Create(VIVALDI_GOOGLE_OAUTH_API_CLIENT_SECRET)));
#else
  return RespondNow(Error("No G client secret defined"));
#endif  // VIVALDI_GOOGLE_OAUTH_API_CLIENT_SECRET
}

ExtensionFunction::ResponseAction UtilitiesGetMOAuthClientIdFunction::Run() {
  namespace Results = vivaldi::utilities::GetMOAuthClientId::Results;

#ifdef VIVALDI_MICROSOFT_OAUTH_API_CLIENT_ID
  return RespondNow(
      ArgumentList(Results::Create(VIVALDI_MICROSOFT_OAUTH_API_CLIENT_ID)));
#else
  return RespondNow(Error("No M client id defined"));
#endif  // VIVALDI_MICROSOFT_OAUTH_API_CLIENT_ID
}

ExtensionFunction::ResponseAction UtilitiesGetYOAuthClientIdFunction::Run() {
  namespace Results = vivaldi::utilities::GetYOAuthClientId::Results;

#ifdef VIVALDI_YAHOO_OAUTH_API_CLIENT_ID
  return RespondNow(
      ArgumentList(Results::Create(VIVALDI_YAHOO_OAUTH_API_CLIENT_ID)));
#else
  return RespondNow(Error("No Y client id defined"));
#endif  // VIVALDI_YAHOO_OAUTH_API_CLIENT_ID
}

ExtensionFunction::ResponseAction
UtilitiesGetYOAuthClientSecretFunction::Run() {
  namespace Results = vivaldi::utilities::GetYOAuthClientSecret::Results;

#ifdef VIVALDI_YAHOO_OAUTH_API_CLIENT_SECRET
  return RespondNow(
      ArgumentList(Results::Create(VIVALDI_YAHOO_OAUTH_API_CLIENT_SECRET)));
#else
  return RespondNow(Error("No Y client secret defined"));
#endif  // VIVALDI_YAHOO_OAUTH_API_CLIENT_SECRET
}

ExtensionFunction::ResponseAction
UtilitiesGetVivaldiNetOAuthClientSecretFunction::Run() {
  namespace Results =
      vivaldi::utilities::GetVivaldiNetOAuthClientSecret::Results;

#ifdef VIVALDI_NET_OAUTH_CLIENT_SECRET
  return RespondNow(
      ArgumentList(Results::Create(VIVALDI_NET_OAUTH_CLIENT_SECRET)));
#else
  return RespondNow(Error("No Vivaldi.net client secret defined"));
#endif  // VIVALDI_NET_OAUTH_CLIENT_SECRET
}

ExtensionFunction::ResponseAction
UtilitiesGetVivaldiNetOAuthClientIdFunction::Run() {
  namespace Results = vivaldi::utilities::GetVivaldiNetOAuthClientId::Results;

#ifdef VIVALDI_NET_OAUTH_CLIENT_ID
  return RespondNow(ArgumentList(Results::Create(VIVALDI_NET_OAUTH_CLIENT_ID)));
#else
  return RespondNow(Error("No Vivaldi.net client id defined"));
#endif  // VIVALDI_NET_OAUTH_CLIENT_ID
}

ExtensionFunction::ResponseAction UtilitiesGetFOAuthClientIdFunction::Run() {
  namespace Results = vivaldi::utilities::GetFOAuthClientId::Results;

#ifdef VIVALDI_FASTMAIL_OAUTH_CLIENT_ID
  return RespondNow(
      ArgumentList(Results::Create(VIVALDI_FASTMAIL_OAUTH_CLIENT_ID)));
#else
  return RespondNow(Error("No Fastmail client id defined"));
#endif  // VIVALDI_FASTMAIL_OAUTH_CLIENT_ID
}

UtilitiesGetCommandLineValueFunction::~UtilitiesGetCommandLineValueFunction() {}

UtilitiesGetCommandLineValueFunction::UtilitiesGetCommandLineValueFunction() {}

ExtensionFunction::ResponseAction UtilitiesGetCommandLineValueFunction::Run() {
  namespace Results = vivaldi::utilities::GetCommandLineValue::Results;
  using vivaldi::utilities::GetCommandLineValue::Params;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string result;
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  result = cmd_line.GetSwitchValueASCII(params->value);

  return RespondNow(ArgumentList(Results::Create(result)));
}

UtilitiesOsCryptFunction::UtilitiesOsCryptFunction() {}
UtilitiesOsCryptFunction::~UtilitiesOsCryptFunction() {}

ExtensionFunction::ResponseAction UtilitiesOsCryptFunction::Run() {
  absl::optional<vivaldi::utilities::OsCrypt::Params> params(
      vivaldi::utilities::OsCrypt::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto encrypted = std::make_unique<std::string>();
  // |encrypted_ptr| is expected to be valid as long as |encrypted| is valid,
  // which should be at least until OnEncryptDone is called. So, it should be
  // safe to use during EncryptString
  std::string* encrypted_ptr = encrypted.get();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&OSCrypt::EncryptString, params->plain, encrypted_ptr),
      base::BindOnce(&UtilitiesOsCryptFunction::OnEncryptDone, this,
                     std::move(encrypted)));

  return RespondLater();
}

void UtilitiesOsCryptFunction::OnEncryptDone(
    std::unique_ptr<std::string> encrypted,
    bool result) {
  if (!result) {
    Respond(Error("Encryption failed"));
    return;
  }

  std::string encoded;
  base::Base64Encode(*encrypted, &encoded);

  Respond(ArgumentList(vivaldi::utilities::OsCrypt::Results::Create(encoded)));
}

UtilitiesOsDecryptFunction::UtilitiesOsDecryptFunction() {}
UtilitiesOsDecryptFunction::~UtilitiesOsDecryptFunction() {}

ExtensionFunction::ResponseAction UtilitiesOsDecryptFunction::Run() {
  absl::optional<vivaldi::utilities::OsDecrypt::Params> params(
      vivaldi::utilities::OsDecrypt::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  std::string encrypted;
  if (!base::Base64Decode(params->encrypted, &encrypted)) {
    return RespondNow(Error("Invalid base64 input"));
  }

  auto decrypted = std::make_unique<std::string>();
  // |decrypted_ptr| is expected to be valid as long as |decrypted| is valid,
  // which should be at least until OnEncryptDone is called. So, it should be
  // safe to use during EncryptString
  std::string* decrypted_ptr = decrypted.get();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&OSCrypt::DecryptString, encrypted, decrypted_ptr),
      base::BindOnce(&UtilitiesOsDecryptFunction::OnDecryptDone, this,
                     std::move(decrypted)));

  return RespondLater();
}

void UtilitiesOsDecryptFunction::OnDecryptDone(
    std::unique_ptr<std::string> decrypted,
    bool result) {
  if (!result) {
    Respond(Error("Decryption failed"));
    return;
  }

  Respond(
      ArgumentList(vivaldi::utilities::OsCrypt::Results::Create(*decrypted)));
}

UtilitiesTranslateTextFunction::UtilitiesTranslateTextFunction() {}
UtilitiesTranslateTextFunction::~UtilitiesTranslateTextFunction() {}

ExtensionFunction::ResponseAction UtilitiesTranslateTextFunction::Run() {
  using vivaldi::utilities::TranslateText::Params;

  absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  request_ = std::make_unique<::vivaldi::VivaldiTranslateServerRequest>(
      Profile::FromBrowserContext(browser_context())->GetWeakPtr(),
      base::BindOnce(&UtilitiesTranslateTextFunction::OnTranslateFinished,
                     this));

  request_->StartRequest(params->source_text, params->source_language_code,
                         params->destination_language_code);

  return RespondLater();
}

namespace {
vivaldi::utilities::TranslateError ConvertTranslateErrorCodeToAPIErrorCode(
    ::vivaldi::TranslateError error) {
  switch (error) {
    case ::vivaldi::TranslateError::kNoError:
      return vivaldi::utilities::TRANSLATE_ERROR_NO_ERROR;
    case ::vivaldi::TranslateError::kNetwork:
      return vivaldi::utilities::TRANSLATE_ERROR_NETWORK;
    case ::vivaldi::TranslateError::kUnknownLanguage:
      return vivaldi::utilities::TRANSLATE_ERROR_UNKNOWN_LANGUAGE;
    case ::vivaldi::TranslateError::kUnsupportedLanguage:
      return vivaldi::utilities::TRANSLATE_ERROR_UNSUPPORTED_LANGUAGE;
    case ::vivaldi::TranslateError::kTranslationError:
      return vivaldi::utilities::TRANSLATE_ERROR_ERROR;
    case ::vivaldi::TranslateError::kTranslationTimeout:
      return vivaldi::utilities::TRANSLATE_ERROR_TIMEOUT;
  }
}
}  // namespace

void UtilitiesTranslateTextFunction::OnTranslateFinished(
    ::vivaldi::TranslateError error,
    std::string detected_source_language,
    std::vector<std::string> sourceText,
    std::vector<std::string> translatedText) {
  namespace Results = vivaldi::utilities::TranslateText::Results;

  vivaldi::utilities::TranslateTextResponse result;

  result.detected_source_language = detected_source_language;
  result.source_text = std::move(sourceText);
  result.translated_text = std::move(translatedText);
  result.error = ConvertTranslateErrorCodeToAPIErrorCode(error);

  Respond(ArgumentList(Results::Create(result)));
}

ExtensionFunction::ResponseAction
UtilitiesShowManageSSLCertificatesFunction::Run() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  using vivaldi::utilities::ShowManageSSLCertificates::Params;
  namespace Results = vivaldi::utilities::ShowManageSSLCertificates::Results;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiBrowserWindow* window =
      VivaldiBrowserWindow::FromId(params->window_id);
  if (!window) {
    return RespondNow(Error("No such window"));
  }
  settings_utils::ShowManageSSLCertificates(window->web_contents());

  return RespondNow(ArgumentList(Results::Create()));
#else
  return RespondNow(Error("API not available on this platform"));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}

ExtensionFunction::ResponseAction UtilitiesSetProtocolHandlingFunction::Run() {
  absl::optional<vivaldi::utilities::SetProtocolHandling::Params> params(
      vivaldi::utilities::SetProtocolHandling::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  bool enabled = params->enabled;
  Profile* profile = Profile::FromBrowserContext(browser_context());

  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(profile);

  if (enabled) {
    registry->Enable();
  } else {
    registry->Disable();
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction UtilitiesConnectProxyFunction::Run() {
#if defined (ENABLE_RELAY_PROXY)
  namespace Results = vivaldi::utilities::ConnectProxy::Results;
  absl::optional<vivaldi::utilities::ConnectProxy::Params> params(
      vivaldi::utilities::ConnectProxy::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  ::vivaldi::proxy::ConnectSettings settings;
  settings.local_port = std::to_string(
      static_cast<int>(params->options.local.port));
  settings.remote_host = params->options.remote.host;
  settings.remote_port = std::to_string(
      static_cast<int>(params->options.remote.port));
  settings.token = params->options.token;
  ::vivaldi::proxy::ConnectState state;

  vivaldi::utilities::ProxyConnectInfo info;
  info.success = ::vivaldi::proxy::connect(settings, state);
  info.pid = state.pid;
  info.message = state.message;

  return RespondNow(ArgumentList(Results::Create(info)));
#else
  return RespondNow(Error("Unexpected call to the browser process"));
#endif // ENABLE_RELAY_PROXY
}

ExtensionFunction::ResponseAction UtilitiesDisconnectProxyFunction::Run() {
#if defined (ENABLE_RELAY_PROXY)
  namespace Results = vivaldi::utilities::DisconnectProxy::Results;
  ::vivaldi::proxy::disconnect();
  return RespondNow(ArgumentList(Results::Create(true)));
#else
  return RespondNow(Error("Unexpected call to the browser process"));
#endif // ENABLE_RELAY_PROXY
}

ExtensionFunction::ResponseAction UtilitiesSupportsProxyFunction::Run() {
  return RespondNow(Error("Unexpected call to the browser process"));
}


}  // namespace extensions
