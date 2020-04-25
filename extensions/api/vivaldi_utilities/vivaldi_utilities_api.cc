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
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/power_monitor/power_monitor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "base/vivaldi_switches.h"
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
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
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

namespace extensions {

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

  base::PowerMonitor::AddObserver(this);

  if (VivaldiRuntimeFeatures::IsEnabled(context, "razer_chroma_support")) {
    razer_chroma_handler_.reset(
        new RazerChromaHandler(Profile::FromBrowserContext(context)));
  }
}

void VivaldiUtilitiesAPI::PostProfileSetup() {
  // This call requires that ProfileKey::GetProtoDatabaseProvider() has
  // been initialized. That does not happen until *after*
  // the constructor of this object has been called.
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(
          profile->GetOriginalProfile());
  manager->AddObserver(this);
}

VivaldiUtilitiesAPI::~VivaldiUtilitiesAPI() {}

void VivaldiUtilitiesAPI::Shutdown() {
  for (auto it : key_to_values_map_) {
    // Get rid of the allocated items
    delete it.second;
  }

  base::PowerMonitor::RemoveObserver(this);

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

// static
bool VivaldiUtilitiesAPI::AuthenticateUser(
    content::BrowserContext* browser_context,
    content::WebContents* web_contents) {
  VivaldiUtilitiesAPI* api = GetFactoryInstance()->Get(browser_context);
  DCHECK(api);
  if (!api)
    return false;

  api->native_window_ =
      web_contents ? platform_util::GetTopLevel(web_contents->GetNativeView())
                   : nullptr;

  bool success = api->password_access_authenticator_.EnsureUserIsAuthenticated(
    password_manager::ReauthPurpose::VIEW_PASSWORD);

  api->native_window_ = nullptr;

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
  Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
                      dispatcher()->GetAssociatedWebContents());
  chrome::ManagePasswordsForPage(browser);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction UtilitiesPrintFunction::Run() {
  using vivaldi::utilities::Print::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string error;
  content::WebContents* tabstrip_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
          params->tab_id, browser_context(), &error);
  if (!tabstrip_contents)
    return RespondNow(Error(error));

  Browser* browser = chrome::FindBrowserWithWebContents(tabstrip_contents);
  if (browser) {
    chrome::Print(browser);
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

ExtensionFunction::ResponseAction UtilitiesIsTabInLastSessionFunction::Run() {
  using vivaldi::utilities::IsTabInLastSession::Params;
  namespace Results = vivaldi::utilities::IsTabInLastSession::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

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

UtilitiesIsUrlValidFunction::UtilitiesIsUrlValidFunction() {}

UtilitiesIsUrlValidFunction::~UtilitiesIsUrlValidFunction() {}

// Based on OnDefaultProtocolClientWorkerFinished in
// external_protocol_handler.cc
void UtilitiesIsUrlValidFunction::OnDefaultProtocolClientWorkerFinished(
    shell_integration::DefaultWebClientState state) {
  namespace Results = vivaldi::utilities::IsUrlValid::Results;

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If we get here, either Vivaldi is not the default or we cannot work out
  // what the default is, so we proceed.
  if (prompt_user_ && state == shell_integration::NOT_DEFAULT) {
    // Ask the user if they want to allow the protocol, but only
    // if the url is a valid formatted url with a registered
    // program to handle it.
    base::string16 application_name =
      shell_integration::GetApplicationNameForProtocol(url_);

    result_.external_handler = result_.url_valid && !application_name.empty();
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
  // GURL::spec() can only be called when url is valid.
  std::string spec = url.is_valid() ? url.spec() : base::EmptyString();
  result_.scheme_valid =
      URLPattern::IsValidSchemeForExtensions(url.scheme()) ||
      url.SchemeIs(url::kJavaScriptScheme) || url.SchemeIs(url::kDataScheme) ||
      url.SchemeIs(url::kMailToScheme) || spec == url::kAboutBlankURL ||
      url.SchemeIs(content::kViewSourceScheme);
  result_.scheme_parsed = url.scheme();
  result_.normalized_url = spec;
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
  url_ = url;

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

  std::string error;
  content::WebContents* web_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
          params->tab_id, browser_context(), &error);
  if (!web_contents)
    return RespondNow(Error(error));

  std::string text;
  content::RenderWidgetHostView* rwhv =
      web_contents->GetRenderWidgetHostView();
  if (rwhv) {
    text = base::UTF16ToUTF8(rwhv->GetSelectedText());
  }

  return RespondNow(ArgumentList(Results::Create(text)));
}

namespace {

// Helpers to simply the usage of ui::SelectFileDialog when implementing
// extension functions.

struct FileSelectionOptions {
  void SetTitle(base::StringPiece str);

  base::string16 title;
  ui::SelectFileDialog::Type type = ui::SelectFileDialog::SELECT_OPEN_FILE;
  ui::SelectFileDialog::FileTypeInfo file_type_info;
};

class FileSelectionRunner : private ui::SelectFileDialog::Listener {
 public:
  using ResultCallback = base::OnceCallback<void(base::FilePath file_path)>;

  static void Start(content::WebContents* web_contents,
                    const FileSelectionOptions& options,
                    ResultCallback callback);
 private:
  FileSelectionRunner(ResultCallback callback);
  ~FileSelectionRunner() override;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  ResultCallback callback_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(FileSelectionRunner);
};

void FileSelectionOptions::SetTitle(base::StringPiece str) {
  title = base::UTF8ToUTF16(str);
}

FileSelectionRunner::FileSelectionRunner(ResultCallback callback)
    : callback_(std::move(callback)),
      select_file_dialog_(ui::SelectFileDialog::Create(this, nullptr)) {}

FileSelectionRunner::~FileSelectionRunner() {
  select_file_dialog_->ListenerDestroyed();
}

// static
void FileSelectionRunner::Start(content::WebContents* web_contents,
                                const FileSelectionOptions& options,
                                ResultCallback callback) {
  gfx::NativeWindow window =
      web_contents ? platform_util::GetTopLevel(web_contents->GetNativeView())
                   : nullptr;

  FileSelectionRunner* runner = new FileSelectionRunner(std::move(callback));
  runner->select_file_dialog_->SelectFile(
      options.type, options.title, base::FilePath(), &options.file_type_info, 0,
      base::FilePath::StringType(), window, nullptr);
}

void FileSelectionRunner::FileSelected(const base::FilePath& path,
                                       int index,
                                       void* params) {
  DCHECK(!params);
  ResultCallback callback = std::move(callback_);
  base::FilePath path_copy = path;
  delete this;
  std::move(callback).Run(std::move(path_copy));
}

void FileSelectionRunner::FileSelectionCanceled(void* params) {
  DCHECK(!params);
  ResultCallback callback = std::move(callback_);
  delete this;
  std::move(callback).Run(base::FilePath());
}

// Converts file extensions to a ui::SelectFileDialog::FileTypeInfo.
void ConvertExtensionsToFileTypeInfo(
    const std::vector<vivaldi::utilities::FileExtension>&
        extensions, ui::SelectFileDialog::FileTypeInfo* file_type_info) {
  for (const auto& item : extensions) {
    base::FilePath::StringType allowed_extension =
      base::FilePath::FromUTF8Unsafe(item.ext).value();

    // FileTypeInfo takes a nested vector like [["htm", "html"], ["txt"]] to
    // group equivalent extensions, but we don't use this feature here.
    std::vector<base::FilePath::StringType> inner_vector;
    inner_vector.push_back(allowed_extension);
    file_type_info->extensions.push_back(std::move(inner_vector));
  }
}

}  // namespace

ExtensionFunction::ResponseAction UtilitiesSelectFileFunction::Run() {
  using vivaldi::utilities::SelectFile::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  FileSelectionOptions options;
  if (params->title) {
    options.SetTitle(*params->title);
  }
  options.type =
      params->type == vivaldi::utilities::SELECT_FILE_DIALOG_TYPE_FOLDER
          ? ui::SelectFileDialog::SELECT_EXISTING_FOLDER
          : ui::SelectFileDialog::SELECT_OPEN_FILE;

  if (params->accepts) {
    ConvertExtensionsToFileTypeInfo(*params->accepts, &options.file_type_info);
  }
  options.file_type_info.include_all_files = true;

  content::WebContents* web_contents = dispatcher()->GetAssociatedWebContents();
  FileSelectionRunner::Start(
      web_contents, options,
      base::BindOnce(&UtilitiesSelectFileFunction::OnFileSelected, this));

  return RespondLater();
}

void UtilitiesSelectFileFunction::OnFileSelected(base::FilePath path) {
  namespace Results = vivaldi::utilities::SelectFile::Results;

  if (path.empty()) {
    Respond(Error("File selection aborted."));
  } else {
    Respond(ArgumentList(Results::Create(path.AsUTF8Unsafe())));
  }
}

ExtensionFunction::ResponseAction UtilitiesSelectLocalImageFunction::Run() {
  using vivaldi::utilities::SelectLocalImage::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int64_t bookmark_id = 0;
  int preference_index = -1;
  if (!!params->params.thumbnail_bookmark_id ==
      !!params->params.preference_path) {
    return RespondNow(Error(
        "Exactly one of preferencePath and thumbnailBookmarkId must be given"));
  }
  if (params->params.thumbnail_bookmark_id) {
    if (!base::StringToInt64(*params->params.thumbnail_bookmark_id,
                             &bookmark_id) ||
        bookmark_id <= 0) {
      return RespondNow(
          Error("thumbnailBookmarkId is not a valid positive integer - " +
                *params->params.thumbnail_bookmark_id));
    }
  } else {
    preference_index = VivaldiDataSourcesAPI::FindMappingPreference(
        *params->params.preference_path);
    if (preference_index < 0) {
      return RespondNow(
          Error("preference " + *params->params.preference_path +
                " is not a preference that stores data mapping URL"));
    }
  }
  FileSelectionOptions options;
  options.SetTitle(params->params.title);
  options.type = ui::SelectFileDialog::SELECT_OPEN_FILE;
  options.file_type_info.include_all_files = true;

  content::WebContents* web_contents = dispatcher()->GetAssociatedWebContents();
  FileSelectionRunner::Start(
      web_contents, options,
      base::BindOnce(&UtilitiesSelectLocalImageFunction::OnFileSelected, this,
                     bookmark_id, preference_index));

  return RespondLater();
}

void UtilitiesSelectLocalImageFunction::OnFileSelected(
    int64_t bookmark_id,
    int preference_index,
    base::FilePath path) {
  namespace Results = vivaldi::utilities::SelectLocalImage::Results;

  std::string result;
  if (path.empty()) {
    SendResult(false);
    return;
  }

  VivaldiDataSourcesAPI::UpdateMapping(
      browser_context(), bookmark_id, preference_index, std::move(path),
      base::BindOnce(
          &UtilitiesSelectLocalImageFunction::SendResult, this));
}

void UtilitiesSelectLocalImageFunction::SendResult(bool success) {
  namespace Results = vivaldi::utilities::SelectLocalImage::Results;

  Respond(ArgumentList(Results::Create(success)));
}

ExtensionFunction::ResponseAction UtilitiesGetVersionFunction::Run() {
  namespace Results = vivaldi::utilities::GetVersion::Results;

  return RespondNow(ArgumentList(Results::Create(
      ::vivaldi::GetVivaldiVersionString(), version_info::GetVersionNumber())));
}

ExtensionFunction::ResponseAction UtilitiesGetFFMPEGStateFunction::Run() {
  namespace Results = vivaldi::utilities::GetFFMPEGState::Results;
#if defined(OS_LINUX)
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

ExtensionFunction::ResponseAction UtilitiesGetSystemCountryFunction::Run() {
  namespace Results = vivaldi::utilities::GetSystemCountry::Results;

  std::string value;
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (!env->GetVar("VIVALDI_COUNTRY", &value) || value.empty()) {
    value = ReadCountry();
  }
  return RespondNow(ArgumentList(Results::Create(value)));
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
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  namespace Results = vivaldi::utilities::IsVivaldiDefaultBrowser::Results;
  if (command_line.HasSwitch(switches::kNoDefaultBrowserCheck)) {
    // Pretend we are default already which will surpress the dialog
    // on startup.
    return RespondNow(ArgumentList(Results::Create(true)));
  }

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

  settings_utils::ShowNetworkProxySettings(
      dispatcher()->GetAssociatedWebContents());
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

ExtensionFunction::ResponseAction UtilitiesSetContentSettingsFunction::Run() {
  using vivaldi::utilities::SetContentSettings::Params;
  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

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
    if (!profile->HasOffTheRecordProfile()) {
      return RespondNow(NoArguments());
    }
    profile = profile->GetOffTheRecordProfile();
  }

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString(primary_pattern_string);
  ContentSettingsPattern secondary_pattern =
      secondary_pattern_string.empty()
          ? ContentSettingsPattern::Wildcard()
          : ContentSettingsPattern::FromString(secondary_pattern_string);

  map->SetContentSettingCustomScope(primary_pattern, secondary_pattern,
                                    content_type, "", setting);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction UtilitiesIsDialogOpenFunction::Run() {
  using vivaldi::utilities::IsDialogOpen::Params;
  namespace Results = vivaldi::utilities::IsDialogOpen::Results;
  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool visible = false;

  switch (params->dialog_name) {
    case vivaldi::utilities::DialogName::DIALOG_NAME_PASSWORD :
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
  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

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

}  // namespace extensions
