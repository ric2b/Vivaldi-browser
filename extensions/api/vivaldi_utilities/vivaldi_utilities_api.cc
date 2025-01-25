//
// Copyright (c) 2015-2018 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

#include <algorithm>
#include <cstddef>
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
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
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
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/crash/core/app/crashpad.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/history/core/browser/top_sites.h"
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
#include "extensions/browser/extension_system.h"
#include "extensions/schema/vivaldi_utilities.h"
#include "net/base/data_url.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/display/screen.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "app/vivaldi_version_info.h"
#include "base/vivaldi_switches.h"
#include "browser/translate/vivaldi_translate_server_request.h"
#include "browser/vivaldi_browser_finder.h"
#include "browser/vivaldi_version_utils.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "components/datasource/vivaldi_data_url_utils.h"
#include "components/datasource/vivaldi_image_store.h"
#include "components/direct_match/direct_match_service_factory.h"
#include "components/locale/locale_kit.h"
#include "extensions/api/runtime/runtime_api.h"
#include "extensions/api/vivaldi_utilities/drag_download_items.h"
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

#include "chrome/browser/net/proxy_service_factory.h"
#include "chromium/net/proxy_resolution/proxy_config_service.h"
#include "components/proxy_config/pref_proxy_config_tracker.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"

#include "components/qr_code_generator/qr_code_generator.h"
#include "components/qr_code_generator/bitmap_generator.h"

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
#include "base/mac/mac_util.h"

#endif

using content_settings::CookieControlsMode;

namespace extensions {

namespace {
constexpr char kMutexNameKey[] = "name";
constexpr char kMutexReleaseTokenKey[] = "release_token";

ContentSetting vivContentSettingFromString(const std::string& name) {
  ContentSetting setting;
  content_settings::ContentSettingFromString(name, &setting);
  return setting;
}

}  // namespace

VivaldiUtilitiesAPI::VivaldiUtilitiesAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this,
                                 vivaldi::utilities::OnScroll::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::utilities::OnDownloadManagerReady::kEventName);

  auto* power_monitor = base::PowerMonitor::GetInstance();
  power_monitor->AddPowerSuspendObserver(this);
  power_monitor->AddPowerStateObserver(this);

  razer_chroma_handler_.reset(
      new RazerChromaHandler(Profile::FromBrowserContext(context)));

  TopSitesFactory::GetForProfile(
      (Profile::FromBrowserContext(context)))->AddObserver(this);
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
  auto* power_monitor = base::PowerMonitor::GetInstance();
  power_monitor->RemovePowerStateObserver(this);
  power_monitor->RemovePowerSuspendObserver(this);

  if (razer_chroma_handler_ && razer_chroma_handler_->IsAvailable()) {
    razer_chroma_handler_->Shutdown();
  }

  TopSitesFactory::GetForProfile(
      (Profile::FromBrowserContext(browser_context_)))->RemoveObserver(this);
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

void VivaldiUtilitiesAPI::OnBatteryPowerStatusChange(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
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

void VivaldiUtilitiesAPI::OnDownloadCreated(content::DownloadManager* manager,
                                   download::DownloadItem* item) {
  if (item->GetState() == download::DownloadItem::IN_PROGRESS) {
    item->AddObserver(this);
  }
}

void ValidateInsecureDownload(download::DownloadItem* download) {
  download->ValidateInsecureDownload();
}

void VivaldiUtilitiesAPI::OnDownloadUpdated(download::DownloadItem* download) {

  if (download->GetInsecureDownloadStatus() != download::DownloadItem::UNKNOWN) {
    // The insecure state is determined and we do not want more notifications about this.
    download->RemoveObserver(this);
  }

  // For mixed content that gets block state we always show a download dialog so
  // this will always be a useraction. We return BLOCK from
  // GetInsecureDownloadStatusForDownload. VB-103844.
  if (download->GetInsecureDownloadStatus() == download::DownloadItem::BLOCK) {

    // We cannot update the downloaditem from the updateobserver so post a task
    // doing this later when the observers has been updated.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(ValidateInsecureDownload, download));
  }
}

void VivaldiUtilitiesAPI::OnPasswordIconStatusChanged(int window_id,
                                                      bool state) {
  ::vivaldi::BroadcastEvent(
      vivaldi::utilities::OnPasswordIconStatusChanged::kEventName,
      vivaldi::utilities::OnPasswordIconStatusChanged::Create(window_id, state),
      browser_context_);
}

void VivaldiUtilitiesAPI::TopSitesLoaded(history::TopSites* top_sites) {}

void VivaldiUtilitiesAPI::TopSitesChanged(
    history::TopSites* top_sites,
    history::TopSitesObserver::ChangeReason change_reason) {
  ::vivaldi::BroadcastEvent(vivaldi::utilities::OnTopSitesChanged::kEventName,
                            vivaldi::utilities::OnTopSitesChanged::Create(),
                            browser_context_);
}

void VivaldiUtilitiesAPI::OnSessionRecoveryStart() {
  on_session_recovery_done_subscription_ =
      SessionRestore::RegisterOnSessionRestoredCallback(base::BindRepeating(
          &VivaldiUtilitiesAPI::OnSessionRecoveryDone, base::Unretained(this)));

  ::vivaldi::BroadcastEvent(vivaldi::utilities::OnSessionRecoveryStart::kEventName,
                            vivaldi::utilities::OnSessionRecoveryStart::Create(),
                            browser_context_);
}

void VivaldiUtilitiesAPI::OnSessionRecoveryDone(Profile* profile, int tabs) {
  on_session_recovery_done_subscription_ = {};

  ::vivaldi::BroadcastEvent(vivaldi::utilities::OnSessionRecoveryDone::kEventName,
                            vivaldi::utilities::OnSessionRecoveryDone::Create(),
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
  std::optional<Params> params = Params::Create(args());
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

  std::optional<Params> params = Params::Create(args());
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

ExtensionFunction::ResponseAction
UtilitiesClearRecentlyClosedTabsFunction::Run() {
  using vivaldi::utilities::ClearRecentlyClosedTabs::Params;
  namespace Results = vivaldi::utilities::ClearRecentlyClosedTabs::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  bool result = false;
  if (tab_restore_service) {
    result = true;
    for (std::string id : params->ids) {
      int num_removed =  tab_restore_service->VivaldiRemoveEntryById(
          SessionID::FromSerializedValue(std::stoi(id)));
      if (num_removed == 0) {
        result = false;
        break;
      }
      tab_restore_service->VivaldiRequestSave(num_removed);
    }
  }
  return RespondNow(ArgumentList(Results::Create(result)));
}

ExtensionFunction::ResponseAction UtilitiesIsTabInLastSessionFunction::Run() {
  using vivaldi::utilities::IsTabInLastSession::Params;
  namespace Results = vivaldi::utilities::IsTabInLastSession::Results;

  std::optional<Params> params = Params::Create(args());
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

  std::optional<Params> params = Params::Create(args());
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

  std::optional<Params> params = Params::Create(args());
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

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  FileSelectionOptions options(params->options.window_id);
  options.SetTitle(params->options.title);
  switch (params->options.type) {
    case vivaldi::utilities::SelectFileDialogType::kFolder:
      options.SetType(ui::SelectFileDialog::SELECT_EXISTING_FOLDER);
      break;
    case vivaldi::utilities::SelectFileDialogType::kFile:
      options.SetType(ui::SelectFileDialog::SELECT_OPEN_FILE);
      break;
    case vivaldi::utilities::SelectFileDialogType::kSaveFile:
      options.SetType(ui::SelectFileDialog::SELECT_SAVEAS_FILE);
      break;
    default:
      NOTREACHED();
  }

  if (params->options.type !=
      vivaldi::utilities::SelectFileDialogType::kFolder) {
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

  std::optional<Params> params = Params::Create(args());
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
  std::optional<VivaldiImageStore::ImageFormat> format =
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
    std::optional<std::vector<uint8_t>> content) {
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

UtilitiesCleanUnusedImagesFunction::UtilitiesCleanUnusedImagesFunction() =
    default;
UtilitiesCleanUnusedImagesFunction::~UtilitiesCleanUnusedImagesFunction() =
    default;

ExtensionFunction::ResponseAction UtilitiesCleanUnusedImagesFunction::Run() {
  using vivaldi::utilities::CleanUnusedImages::Params;
  int limit = 0;
  std::optional<Params> params = Params::Create(args());
  if (params) {
    limit = params->created_before;
  }
  VivaldiImageStore::ScheduleRemovalOfUnusedUrlData(browser_context(), limit);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction UtilitiesStoreImageFunction::Run() {
  using vivaldi::utilities::StoreImage::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  ParseImagePlaceParams(place_,
                        params->options.theme_id.value_or(std::string()),
                        std::string(), error);
  if (!error.empty())
    return RespondNow(Error(std::move(error)));

  uint32_t what_args = 0;
  constexpr uint32_t has_value = (1 << 0);
  constexpr uint32_t has_url = (1 << 1);

  if (params->options.data.has_value()) {
    what_args |= has_value;
  }

  if (params->options.url && !params->options.url->empty()) {
    what_args |= has_url;
  }

  if (what_args == has_value) {
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
    StoreImage(base::MakeRefCounted<base::RefCountedBytes>(
        std::move(params->options.data.value())));
  } else if (what_args == has_url) {
    GURL url(*params->options.url);
    if (!url.is_valid()) {
      return RespondNow(
          Error("url is not valid - " + url.possibly_invalid_spec()));
    }
    if (url.SchemeIs("data")) {
      std::string mime, charset, data;
      if (net::DataURL::Parse(url, &mime, &charset, &data)) {
        auto bytes = base::MakeRefCounted<base::RefCountedBytes>(
            base::span(reinterpret_cast<const uint8_t*>(data.c_str()), data.size()));
        image_format_ = VivaldiImageStore::FindFormatForMimeType(mime);
        if (!image_format_) {
          return RespondNow(Error("invalid DataURL - unsupported mime type"));
        }
        StoreImage(bytes);
      } else {
        return RespondNow(Error("invalid DataURL"));
      }
    } else {
      if (!url.SchemeIsFile()) {
        return RespondNow(Error("unsupported image source URL: " + url.spec()));
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
    }
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

ExtensionFunction::ResponseAction UtilitiesGetEnvVarsFunction::Run() {
  namespace Results = vivaldi::utilities::GetEnvVars::Results;
  using Params = vivaldi::utilities::GetEnvVars::Params;

  std::optional<Params> params = Params::Create(args());

  EXTENSION_FUNCTION_VALIDATE(params);

  std::unique_ptr<base::Environment> env(base::Environment::Create());

  vivaldi::utilities::GetEnvVarsResponse response;

  // read the environment variables into a object with additional properties
  for (const auto& key : params->keys) {
    std::string value;

    if (env->GetVar(key, &value)) {
      response.additional_properties.emplace(key, value);
    }
  }

  return RespondNow(ArgumentList(Results::Create(response)));
}

ExtensionFunction::ResponseAction UtilitiesSetSharedDataFunction::Run() {
  using vivaldi::utilities::SetSharedData::Params;
  namespace Results = vivaldi::utilities::SetSharedData::Results;

  std::optional<Params> params = Params::Create(args());
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

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiUtilitiesAPI* api =
      VivaldiUtilitiesAPI::GetFactoryInstance()->Get(browser_context());

  const base::Value* value = api->GetSharedData(params->key_value_pair.key);
  return RespondNow(ArgumentList(
      Results::Create(value ? *value : params->key_value_pair.value)));
}

ExtensionFunction::ResponseAction UtilitiesTakeMutexFunction::Run() {
  using vivaldi::utilities::TakeMutex::Params;

  std::optional<Params> params = Params::Create(args());
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

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string* name;
  std::optional<int> release_token;

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

  std::optional<Params> params = Params::Create(args());
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

  Respond(
      ArgumentList(Results::Create(state == shell_integration::IS_DEFAULT)));
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

  Respond(
      ArgumentList(Results::Create(state == shell_integration::IS_DEFAULT)));
}

ExtensionFunction::ResponseAction
UtilitiesLaunchNetworkSettingsFunction::Run() {
  using vivaldi::utilities::LaunchNetworkSettings::Params;
  namespace Results = vivaldi::utilities::LaunchNetworkSettings::Results;

  std::optional<Params> params = Params::Create(args());
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
  std::optional<Params> params = Params::Create(args());
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
  std::optional<Params> params = Params::Create(args());
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
  std::optional<Params> params = Params::Create(args());
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

  std::optional<Params> params = Params::Create(args());
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

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string& content_settings = params->content_setting;
  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(content_settings);
  ContentSetting default_setting;
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();

  default_setting = HostContentSettingsMapFactory::GetForProfile(profile)
                        ->GetDefaultContentSetting(content_type, nullptr);

  std::string setting =
      content_settings::ContentSettingToString(default_setting);

  return RespondNow(ArgumentList(Results::Create(setting)));
}

namespace {
CookieControlsMode ToCookieControlsMode(
    vivaldi::utilities::CookieMode cookie_mode) {
  switch (cookie_mode) {
    case vivaldi::utilities::CookieMode::kOff:
      return CookieControlsMode::kOff;
    case vivaldi::utilities::CookieMode::kBlockThirdParty:
      return CookieControlsMode::kBlockThirdParty;
    case vivaldi::utilities::CookieMode::kBlockThirdPartyIncognitoOnly:
      return CookieControlsMode::kIncognitoOnly;
    default:
      NOTREACHED() << "Incorrect cookie mode to the API";
      //return CookieControlsMode::kBlockThirdParty;
  }
}

vivaldi::utilities::CookieMode ToCookieMode(CookieControlsMode mode) {
  using vivaldi::utilities::CookieMode;

  switch (mode) {
    case CookieControlsMode::kOff:
      return CookieMode::kOff;
    case CookieControlsMode::kBlockThirdParty:
      return CookieMode::kBlockThirdParty;
    case CookieControlsMode::kIncognitoOnly:
      return CookieMode::kBlockThirdPartyIncognitoOnly;
    default:
      NOTREACHED() << "Incorrect cookie controls mode to the API";
      //return CookieMode::kBlockThirdParty;
  }
}

}  // namespace

ExtensionFunction::ResponseAction
UtilitiesSetBlockThirdPartyCookiesFunction::Run() {
  using vivaldi::utilities::SetBlockThirdPartyCookies::Params;
  namespace Results = vivaldi::utilities::SetBlockThirdPartyCookies::Results;

  std::optional<Params> params = Params::Create(args());
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

  std::optional<Params> params = Params::Create(args());
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

  std::optional<Params> params = Params::Create(args());
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

  std::optional<Params> params = Params::Create(args());
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

  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();

  std::string version = ::vivaldi::GetVivaldiVersionString();
  const bool version_changed =
      ::vivaldi::version::HasVersionChanged(profile->GetPrefs());
  if (version_changed) {
    profile->GetPrefs()->SetString(vivaldiprefs::kStartupLastSeenVersion,
                                   version);
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  bool force_first_run = command_line->HasSwitch(switches::kForceFirstRun);
  bool no_first_run = command_line->HasSwitch(switches::kNoFirstRun);
  // Show new features tab only for official final builds.
  results.show = (version_changed || force_first_run) && !no_first_run &&
                 ::vivaldi::ReleaseKind() >= ::vivaldi::Release::kBeta;

  return RespondNow(ArgumentList(Results::Create(results)));
}

ExtensionFunction::ResponseAction UtilitiesSetDialogPositionFunction::Run() {
  using vivaldi::utilities::SetDialogPosition::Params;
  namespace Results = vivaldi::utilities::SetDialogPosition::Results;

  std::optional<Params> params = Params::Create(args());
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

  std::optional<Params> params = Params::Create(args());
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
  std::optional<Params> params = Params::Create(args());
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
  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool visible = false;

  switch (params->dialog_name) {
    case vivaldi::utilities::DialogName::kPassword:
      if (PasswordBubbleViewBase::manage_password_bubble()) {
        visible =
            PasswordBubbleViewBase::manage_password_bubble()->GetVisible();
      }
      break;

    default:
    case vivaldi::utilities::DialogName::kChromecast:
      break;
  }
  return RespondNow(ArgumentList(Results::Create(visible)));
}

ExtensionFunction::ResponseAction UtilitiesFocusDialogFunction::Run() {
  using vivaldi::utilities::FocusDialog::Params;
  namespace Results = vivaldi::utilities::FocusDialog::Results;
  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool focused = false;

  switch (params->dialog_name) {
    case vivaldi::utilities::DialogName::kPassword:
      if (PasswordBubbleViewBase::manage_password_bubble()) {
        if (PasswordBubbleViewBase::manage_password_bubble()->CanActivate()) {
          PasswordBubbleViewBase::manage_password_bubble()->ActivateBubble();
          focused = true;
        }
      }
      break;

    default:
    case vivaldi::utilities::DialogName::kChromecast:
      break;
  }
  return RespondNow(ArgumentList(Results::Create(focused)));
}

ExtensionFunction::ResponseAction UtilitiesStartChromecastFunction::Run() {
  using vivaldi::utilities::StartChromecast::Params;
  std::optional<Params> params = Params::Create(args());
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
    if (const std::optional<OSVERSIONINFOEX> current_os_version =
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

UtilitiesGenerateQRCodeFunction::~UtilitiesGenerateQRCodeFunction() {}

UtilitiesGenerateQRCodeFunction::UtilitiesGenerateQRCodeFunction() {}

ExtensionFunction::ResponseAction UtilitiesGenerateQRCodeFunction::Run() {
  using vivaldi::utilities::GenerateQRCode::Params;
  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::expected<SkBitmap, qr_code_generator::Error> qr_code;
  qr_code = qr_code_generator::GenerateBitmap(
      base::as_byte_span(params->data),
      qr_code_generator::ModuleStyle::kCircles,
      qr_code_generator::LocatorStyle::kRounded,
      qr_code_generator::CenterImage::kNoCenterImage,
      qr_code_generator::QuietZone::kIncluded);
  if (!qr_code.has_value()) {
    RespondOnUiThread("");
  } else {
    switch (params->destination) {
      case vivaldi::utilities::CaptureQRDestination::kClipboard: {
        ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
        scw.Reset();
        scw.WriteImage(qr_code.value());
        RespondOnUiThread("");
        break;
      }
      case vivaldi::utilities::CaptureQRDestination::kFile: {
        base::FilePath path = DownloadPrefs::GetDefaultDownloadDirectory();
        if (path.empty()) {
          RespondOnUiThread("");
          break;
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
                           std::move(path), std::move(qr_code.value()),
                          ::vivaldi::skia_utils::ImageFormat::kPNG, 90),
            base::BindOnce(
                &UtilitiesGenerateQRCodeFunction::RespondOnUiThreadForFile,
                this));
        break;
      }
      case vivaldi::utilities::CaptureQRDestination::kDataurl:
      default: {
        std::string dataurl = ::vivaldi::skia_utils::EncodeBitmapAsDataUrl(
            qr_code.value(), ::vivaldi::skia_utils::ImageFormat::kPNG, 90);
        RespondOnUiThread(dataurl);
        break;
      }
    }
  }
  return AlreadyResponded();
}

void UtilitiesGenerateQRCodeFunction::RespondOnUiThreadForFile(
    base::FilePath path) {
  namespace Results = vivaldi::utilities::GenerateQRCode::Results;

  if (path.empty()) {
    return Respond(Error("Failed to save QR code to file"));
  } else {
    Profile* profile = Profile::FromBrowserContext(browser_context());
    platform_util::ShowItemInFolder(profile, path);
    Respond(ArgumentList(Results::Create(path.AsUTF8Unsafe())));
  }
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

ExtensionFunction::ResponseAction UtilitiesGetAOLOAuthClientIdFunction::Run() {
  namespace Results = vivaldi::utilities::GetAOLOAuthClientId::Results;

#ifdef VIVALDI_AOL_OAUTH_API_CLIENT_ID
  return RespondNow(
      ArgumentList(Results::Create(VIVALDI_AOL_OAUTH_API_CLIENT_ID)));
#else
  return RespondNow(Error("No AOL client id defined"));
#endif  // VIVALDI_AOL_OAUTH_API_CLIENT_ID
}

ExtensionFunction::ResponseAction
UtilitiesGetAOLOAuthClientSecretFunction::Run() {
  namespace Results = vivaldi::utilities::GetAOLOAuthClientSecret::Results;

#ifdef VIVALDI_AOL_OAUTH_API_CLIENT_SECRET
  return RespondNow(
      ArgumentList(Results::Create(VIVALDI_AOL_OAUTH_API_CLIENT_SECRET)));
#else
  return RespondNow(Error("No AOL client secret defined"));
#endif  // VIVALDI_AOL_OAUTH_API_CLIENT_SECRET
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

ExtensionFunction::ResponseAction
UtilitiesGetOSGeolocationStateFunction::Run() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  namespace Results = vivaldi::utilities::GetOSGeolocationState::Results;
  return RespondNow(ArgumentList(Results::Create(
      system_permission_settings::IsAllowed(
          ContentSettingsType::GEOLOCATION))));
#else
  return RespondNow(Error("System not supported"));
#endif
}

ExtensionFunction::ResponseAction
UtilitiesOpenOSGeolocationSettingsFunction::Run() {
#if BUILDFLAG(IS_MAC)
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_LocationServices,
      std::string());
  namespace Results = vivaldi::utilities::OpenOSGeolocationSettings::Results;
  return RespondNow(ArgumentList(Results::Create()));
#else
  return RespondNow(Error("System not supported"));
#endif
}

UtilitiesGetCommandLineValueFunction::~UtilitiesGetCommandLineValueFunction() {}

UtilitiesGetCommandLineValueFunction::UtilitiesGetCommandLineValueFunction() {}

ExtensionFunction::ResponseAction UtilitiesGetCommandLineValueFunction::Run() {
  namespace Results = vivaldi::utilities::GetCommandLineValue::Results;
  using vivaldi::utilities::GetCommandLineValue::Params;
  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string result;
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  result = cmd_line.GetSwitchValueASCII(params->value);

  return RespondNow(ArgumentList(Results::Create(result)));
}

UtilitiesOsCryptFunction::UtilitiesOsCryptFunction() {}
UtilitiesOsCryptFunction::~UtilitiesOsCryptFunction() {}

ExtensionFunction::ResponseAction UtilitiesOsCryptFunction::Run() {
  std::optional<vivaldi::utilities::OsCrypt::Params> params(
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
  encoded = base::Base64Encode(*encrypted);

  Respond(ArgumentList(vivaldi::utilities::OsCrypt::Results::Create(encoded)));
}

UtilitiesOsDecryptFunction::UtilitiesOsDecryptFunction() {}
UtilitiesOsDecryptFunction::~UtilitiesOsDecryptFunction() {}

ExtensionFunction::ResponseAction UtilitiesOsDecryptFunction::Run() {
  std::optional<vivaldi::utilities::OsDecrypt::Params> params(
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

  std::optional<Params> params(Params::Create(args()));
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
      return vivaldi::utilities::TranslateError::kNoError;
    case ::vivaldi::TranslateError::kNetwork:
      return vivaldi::utilities::TranslateError::kNetwork;
    case ::vivaldi::TranslateError::kUnknownLanguage:
      return vivaldi::utilities::TranslateError::kUnknownLanguage;
    case ::vivaldi::TranslateError::kUnsupportedLanguage:
      return vivaldi::utilities::TranslateError::kUnsupportedLanguage;
    case ::vivaldi::TranslateError::kTranslationError:
      return vivaldi::utilities::TranslateError::kError;
    case ::vivaldi::TranslateError::kTranslationTimeout:
      return vivaldi::utilities::TranslateError::kTimeout;
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

  std::optional<Params> params = Params::Create(args());
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
  std::optional<vivaldi::utilities::SetProtocolHandling::Params> params(
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

ExtensionFunction::ResponseAction UtilitiesBrowserWindowReadyFunction::Run() {
  namespace Results = vivaldi::utilities::BrowserWindowReady::Results;
  std::optional<vivaldi::utilities::BrowserWindowReady::Params> params(
      vivaldi::utilities::BrowserWindowReady::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  VivaldiBrowserWindow* window =
      VivaldiBrowserWindow::FromId(params->window_id);
  if (window) {
    window->OnUIReady();
    return RespondNow(ArgumentList(Results::Create(true)));
  } else {
    return RespondNow(ArgumentList(Results::Create(false)));
  }
}

bool UtilitiesReadImageFunction::ReadFileAndMimeType(base::FilePath file_path,
                                                     std::vector<uint8_t>* data,
                                                     std::string* mimeType) {
  if (!base::PathExists(file_path) || !data || !mimeType) {
    return false;
  }
  if (const auto bytes = base::ReadFileToBytes(file_path)) {
    *data = *bytes;
  }
  net::GetMimeTypeFromFile(file_path, mimeType);
  return !(data->empty() || mimeType->empty());
}

void UtilitiesReadImageFunction::SendResult(
    std::unique_ptr<std::vector<uint8_t>> data,
    std::unique_ptr<std::string> mimeType,
    bool result) {
  namespace Results = vivaldi::utilities::ReadImage::Results;
  if (!result || !data || !mimeType) {
    Respond(Error("Could not get the data."));
  } else {
    std::vector<int> transData;
    std::transform(data->begin(), data->end(), std::back_inserter(transData),
                   [](const uint8_t& it) { return static_cast<int>(it); });

    vivaldi::utilities::ReadImageData resp;
    resp.data = transData;
    resp.type = *mimeType;
    Respond(ArgumentList(Results::Create(resp)));
  }
}

ExtensionFunction::ResponseAction UtilitiesReadImageFunction::Run() {
  std::optional<vivaldi::utilities::ReadImage::Params> params(
      vivaldi::utilities::ReadImage::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL gurl(params->url);

  if (gurl.is_empty() || !gurl.is_valid()) {
    return RespondNow(Error("Empty or invalid url."));
  }
  base::FilePath filePath;
  if (!net::FileURLToFilePath(gurl, &filePath)) {
    return RespondNow(
        Error("URL does not refer to a valid file path - " + gurl.spec()));
  }

  auto fileBytes = std::make_unique<std::vector<uint8_t>>();
  // |fileBytes_ptr| is expected to be valid as long as |fileBytes| is valid,
  // which should be at least until SendResult is called. So, it should be
  // safe to use during ReadFileAndMimeType - the explicit variable is required
  // to work on Windows
  std::vector<uint8_t>* fileBytes_ptr = fileBytes.get();

  auto mimeType = std::make_unique<std::string>();
  // |mimeType_ptr| is expected to be valid as long as |mimeType| is valid,
  // which should be at least until SendResult is called. So, it should be
  // safe to use during ReadFileAndMimeType - the explicit variable is required
  // to work on Windows
  std::string* mimeType_ptr = mimeType.get();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&UtilitiesReadImageFunction::ReadFileAndMimeType, this,
                     filePath, fileBytes_ptr, mimeType_ptr),
      base::BindOnce(&UtilitiesReadImageFunction::SendResult, this,
                     std::move(fileBytes), std::move(mimeType)));
  return RespondLater();
}

ExtensionFunction::ResponseAction UtilitiesIsRTLFunction::Run() {
  return RespondNow(Error("Unexpected call to the browser process"));
}

ExtensionFunction::ResponseAction UtilitiesGetDirectMatchFunction::Run() {
  using vivaldi::utilities::GetDirectMatch::Params;
  namespace Results = vivaldi::utilities::GetDirectMatch::Results;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  auto* service = direct_match::DirectMatchServiceFactory::GetForBrowserContext(
      browser_context());
  auto [ unit_found, allowed_to_be_default_match ] =
      service->GetDirectMatch(params->query);
  if (unit_found) {
    vivaldi::utilities::DirectMatchItem item;
    item.name = unit_found->name;
    item.title = unit_found->title;
    item.image_url = unit_found->image_url;
    item.image_path = unit_found->image_path;
    item.category = unit_found->category;
    item.display_location_address_bar =
        unit_found->display_locations.address_bar;
    item.display_location_sd_dialog = unit_found->display_locations.sd_dialog;
    item.redirect_url = unit_found->redirect_url;
    item.allowed_to_be_default_match = allowed_to_be_default_match;
    return RespondNow(ArgumentList(Results::Create(item)));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
UtilitiesGetDirectMatchPopularSitesFunction::Run() {
  namespace Results = vivaldi::utilities::GetDirectMatchPopularSites::Results;
  auto* service = direct_match::DirectMatchServiceFactory::GetForBrowserContext(
      browser_context());
  const auto& units = service->GetPopularSites();
  std::vector<vivaldi::utilities::DirectMatchItem> items;
  if (units.size() > 0) {
    for (const auto& unit : units) {
      vivaldi::utilities::DirectMatchItem item;
      item.name = unit->name;
      item.title = unit->title;
      item.image_url = unit->image_url;
      item.image_path = unit->image_path;
      item.category = unit->category;
      item.display_location_address_bar = unit->display_locations.address_bar;
      item.display_location_sd_dialog = unit->display_locations.sd_dialog;
      item.redirect_url = unit->redirect_url;
      items.push_back(std::move(item));
    }
  }
  return RespondNow(ArgumentList(Results::Create(items)));
}

ExtensionFunction::ResponseAction
UtilitiesGetDirectMatchesForCategoryFunction::Run() {
  using vivaldi::utilities::GetDirectMatchesForCategory::Params;
  namespace Results = vivaldi::utilities::GetDirectMatchesForCategory::Results;
  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  auto* service = direct_match::DirectMatchServiceFactory::GetForBrowserContext(
      browser_context());
  const auto& units = service->GetDirectMatchesForCategory(params->category_id);
  std::vector<vivaldi::utilities::DirectMatchItem> items;
  if (units.size() > 0) {
    for (const auto& unit : units) {
      vivaldi::utilities::DirectMatchItem item;
      item.name = unit->name;
      item.title = unit->title;
      item.image_url = unit->image_url;
      item.image_path = unit->image_path;
      item.category = unit->category;
      item.display_location_address_bar = unit->display_locations.address_bar;
      item.display_location_sd_dialog = unit->display_locations.sd_dialog;
      item.redirect_url = unit->redirect_url;
      items.push_back(std::move(item));
    }
  }
  return RespondNow(ArgumentList(Results::Create(items)));
}

ExtensionFunction::ResponseAction UtilitiesEmulateUserInputFunction::Run() {
  using vivaldi::utilities::EmulateUserInput::Params;
  namespace Results = vivaldi::utilities::EmulateUserInput::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiBrowserWindow* window =
      VivaldiBrowserWindow::FromId(params->window_id);
  if (!window) {
    return RespondNow(Error("No such window"));
  }

  window->web_contents()->GetPrimaryMainFrame()->NotifyUserActivation(
    blink::mojom::UserActivationNotificationType::kInteraction);
  return RespondNow(ArgumentList(Results::Create(true)));
}

void UtilitiesIsVivaldiPinnedToLaunchBarFunction::SendResult(std::optional<bool> isPinned) {
  namespace Results = vivaldi::utilities::IsVivaldiPinnedToLaunchBar::Results;
  if (isPinned.has_value())
    Respond(ArgumentList(Results::Create(isPinned.value())));
  else
    Respond(Error("Vivaldi cannot be pinned in the current environment."));
}

ExtensionFunction::ResponseAction
UtilitiesIsVivaldiPinnedToLaunchBarFunction::Run() {
#if BUILDFLAG(IS_WIN)
  return RespondNow(
      Error("IsVivaldiPinnedToLaunchBar API is not implemented on "
            "windows yet"));
#else
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &UtilitiesIsVivaldiPinnedToLaunchBarFunction::CheckIsPinned, this),
      base::BindOnce(&UtilitiesIsVivaldiPinnedToLaunchBarFunction::SendResult,
                     this));

  return RespondLater();
#endif
}

void UtilitiesPinVivaldiToLaunchBarFunction::SendResult(bool success) {
  namespace Results = vivaldi::utilities::PinVivaldiToLaunchBar::Results;
  Respond(ArgumentList(Results::Create(success)));
  return;
}

ExtensionFunction::ResponseAction
UtilitiesPinVivaldiToLaunchBarFunction::Run() {
#if BUILDFLAG(IS_WIN)
  return RespondNow(Error(
      "PinVivaldiToLaunchBar API is not implemented on windows yet"));
#else
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&UtilitiesPinVivaldiToLaunchBarFunction::PinToLaunchBar,
                     this),
      base::BindOnce(&UtilitiesPinVivaldiToLaunchBarFunction::SendResult,
                     this));

  return RespondLater();
#endif
}

ExtensionFunction::ResponseAction UtilitiesDownloadsDragFunction::Run() {
  using vivaldi::utilities::DownloadsDrag::Params;
  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Browser* browser = ::vivaldi::FindBrowserByWindowId(params->window_id);
  if (!browser) {
    return RespondNow(Error("No Browser instance."));
  }
  Profile* profile = Profile::FromBrowserContext(browser_context());
  DCHECK(profile);

  content::DownloadManager* manager =
      profile->GetOriginalProfile()->GetDownloadManager();

  const display::Screen* const screen = display::Screen::GetScreen();
  DCHECK(screen);

  std::vector<extensions::DraggableDownloadItem> items;
  for (const auto& id : params->download_ids) {
    download::DownloadItem* download_item = manager->GetDownload(id);
    if (!download_item ||
        download_item->GetState() != download::DownloadItem::COMPLETE) {
      continue;
    }

    // Use scale for primary display as it's more likely that the icon is cached
    gfx::Image* icon =
        g_browser_process->icon_manager()->LookupIconFromFilepath(
            download_item->GetTargetFilePath(), IconLoader::NORMAL,
            screen->GetPrimaryDisplay().device_scale_factor());
    items.push_back({download_item, icon});
  }

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  gfx::NativeView view = web_contents->GetNativeView();
  {
    // Enable nested tasks during DnD, while |DragDownload()| blocks.
    base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
    DragDownloadItems(items, view);
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction UtilitiesHasCommandLineSwitchFunction::Run() {
  namespace Results = vivaldi::utilities::HasCommandLineSwitch::Results;
  using vivaldi::utilities::GetCommandLineValue::Params;
  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();

  return RespondNow(
      ArgumentList(Results::Create(cmd_line.HasSwitch(params->value))));
}

ExtensionFunction::ResponseAction
UtilitiesAcknowledgeCrashedSessionFunction::Run() {
  using vivaldi::utilities::AcknowledgeCrashedSession::Params;
  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Browser* browser = ::vivaldi::FindBrowserByWindowId(params->window_id);
  if (!browser) {
    return RespondNow(Error("No Browser instance."));
  }
  Profile* profile = Profile::FromBrowserContext(browser_context());
  DCHECK(profile);

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(extension_service);

  if (params->restore_session && !params->reenable_extensions) {
    // Disable all temporary blocked extensions, and unblock them (blocked
    // extensions are not visible to the user).
    extension_service->DisableUserExtensionsExcept(std::vector<std::string>());
  }

  extension_service->UnblockAllExtensions();

  {
    // When user doesn't want to restore we still need to take crash lock to ACK
    // the crash.
    std::unique_ptr<ExitTypeService::CrashedLock> crashed_lock_;
    if (ExitTypeService* exit_type_service =
            ExitTypeService::GetInstanceForProfile(profile)) {
      crashed_lock_ = exit_type_service->CreateCrashedLock();
    }

    std::unique_ptr<ExitTypeService::CrashedLock> lock =
        std::move(crashed_lock_);

    if (params->restore_session) {
      VivaldiUtilitiesAPI* api =
          VivaldiUtilitiesAPI::GetFactoryInstance()->Get(browser_context());
      api->OnSessionRecoveryStart();
      // OnSessionRecoveryDone is going to be called here.
      SessionRestore::RestoreSessionAfterCrash(browser);
    }
  }

  return RespondNow(NoArguments());
}

}  // namespace extensions
