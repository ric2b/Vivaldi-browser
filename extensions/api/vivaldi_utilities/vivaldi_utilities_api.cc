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
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "prefs/vivaldi_pref_names.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/vivaldi_ui_utils.h"
#include "url/url_constants.h"

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

}

VivaldiUtilitiesEventRouter::VivaldiUtilitiesEventRouter(Profile* profile)
    : browser_context_(profile) {}

VivaldiUtilitiesEventRouter::~VivaldiUtilitiesEventRouter() {}

void VivaldiUtilitiesEventRouter::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

VivaldiUtilitiesAPI::VivaldiUtilitiesAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this,
                                 vivaldi::utilities::OnScroll::kEventName);

  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor)
    power_monitor->AddObserver(this);
}

VivaldiUtilitiesAPI::~VivaldiUtilitiesAPI() {}

void VivaldiUtilitiesAPI::Shutdown() {
  for (auto it : key_to_values_map_) {
    // Get rid of the allocated items
    delete it.second;
  }
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor)
    power_monitor->RemoveObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI> >::
    DestructorAtExit g_factory_utils = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI>*
VivaldiUtilitiesAPI::GetFactoryInstance() {
  return g_factory_utils.Pointer();
}

void VivaldiUtilitiesAPI::ScrollType(int scrollType) {
  if (event_router_) {
    event_router_->DispatchEvent(
        vivaldi::utilities::OnScroll::kEventName,
        vivaldi::utilities::OnScroll::Create(scrollType));
  }
}

void VivaldiUtilitiesAPI::OnListenerAdded(const EventListenerInfo& details) {
  event_router_.reset(new VivaldiUtilitiesEventRouter(
      Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
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

void VivaldiUtilitiesAPI::OnPowerStateChange(bool on_battery_power) {
  // Implement if needed
}

void VivaldiUtilitiesAPI::OnSuspend() {
  if (event_router_) {
    event_router_->DispatchEvent(vivaldi::utilities::OnSuspend::kEventName,
                                 vivaldi::utilities::OnSuspend::Create());
  }
}

void VivaldiUtilitiesAPI::OnResume() {
  if (event_router_) {
    event_router_->DispatchEvent(vivaldi::utilities::OnResume::kEventName,
                                 vivaldi::utilities::OnResume::Create());
  }
}

namespace ClearAllRecentlyClosedSessions =
    vivaldi::utilities::ClearAllRecentlyClosedSessions;

UtilitiesBasicPrintFunction::UtilitiesBasicPrintFunction() {}
UtilitiesBasicPrintFunction::~UtilitiesBasicPrintFunction() {}

bool UtilitiesBasicPrintFunction::RunAsync() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  Browser* browser = chrome::FindAnyBrowser(profile, true);
  chrome::BasicPrint(browser);

  SendResponse(true);
  return true;
}

UtilitiesClearAllRecentlyClosedSessionsFunction::
    ~UtilitiesClearAllRecentlyClosedSessionsFunction() {}

bool UtilitiesClearAllRecentlyClosedSessionsFunction::RunAsync() {
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(GetProfile());
  bool result = false;
  if (tab_restore_service) {
    result = true;
    tab_restore_service->ClearEntries();
  }
  results_ = ClearAllRecentlyClosedSessions::Results::Create(result);
  SendResponse(result);
  return result;
}

UtilitiesGetUniqueUserIdFunction::~UtilitiesGetUniqueUserIdFunction() {}

bool UtilitiesGetUniqueUserIdFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::GetUniqueUserId::Params> params(
      vivaldi::utilities::GetUniqueUserId::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string user_id = g_browser_process->local_state()->GetString(
      vivaldiprefs::kVivaldiUniqueUserId);

  if (!IsValidUserId(user_id)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(
            &UtilitiesGetUniqueUserIdFunction::GetUniqueUserIdOnFileThread,
            this, params->legacy_user_id));
  } else {
    RespondOnUIThread(user_id, false);
  }

  return true;
}

void UtilitiesGetUniqueUserIdFunction::GetUniqueUserIdOnFileThread(
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

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&UtilitiesGetUniqueUserIdFunction::RespondOnUIThread, this,
                 user_id, is_new_user));
}

void UtilitiesGetUniqueUserIdFunction::RespondOnUIThread(
    const std::string& user_id,
    bool is_new_user) {
  g_browser_process->local_state()->SetString(
      vivaldiprefs::kVivaldiUniqueUserId, user_id);

  results_ = vivaldi::utilities::GetUniqueUserId::Results::Create(user_id,
                                                                  is_new_user);
  SendResponse(true);
}

bool UtilitiesIsTabInLastSessionFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::IsTabInLastSession::Params> params(
      vivaldi::utilities::IsTabInLastSession::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tabId;
  if (!base::StringToInt(params->tab_id, &tabId))
    return false;

  bool is_in_session = false;
  content::WebContents* contents;

  if (!ExtensionTabUtil::GetTabById(tabId, GetProfile(), true, nullptr, nullptr,
                                    &contents, nullptr)) {
    error_ = "TabId not found.";
    return false;
  }
  // Both the profile and navigation entries are marked if they are
  // loaded from a session, so check both.
  if (GetProfile()->restored_last_session()) {
    content::NavigationEntry* entry =
        contents->GetController().GetVisibleEntry();
    is_in_session = entry && entry->IsRestored();
  }
  results_ =
      vivaldi::utilities::IsTabInLastSession::Results::Create(is_in_session);

  SendResponse(true);
  return true;
}

UtilitiesIsTabInLastSessionFunction::UtilitiesIsTabInLastSessionFunction() {}

UtilitiesIsTabInLastSessionFunction::~UtilitiesIsTabInLastSessionFunction() {}

bool UtilitiesIsUrlValidFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::IsUrlValid::Params> params(
      vivaldi::utilities::IsUrlValid::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url(params->url);

  vivaldi::utilities::UrlValidResults result;
  result.url_valid = !params->url.empty() && url.is_valid();
  result.scheme_valid =
      URLPattern::IsValidSchemeForExtensions(url.scheme()) ||
      url.SchemeIs(url::kJavaScriptScheme) || url.SchemeIs(url::kDataScheme) ||
      url.SchemeIs(url::kMailToScheme) || url.spec() == url::kAboutBlankURL;

  results_ = vivaldi::utilities::IsUrlValid::Results::Create(result);

  SendResponse(true);
  return true;
}

UtilitiesGetSelectedTextFunction::UtilitiesGetSelectedTextFunction() {}

UtilitiesGetSelectedTextFunction::~UtilitiesGetSelectedTextFunction() {}

bool UtilitiesGetSelectedTextFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::GetSelectedText::Params> params(
      vivaldi::utilities::GetSelectedText::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tabId;
  if (!base::StringToInt(params->tab_id, &tabId))
    return false;

  std::string text;
  content::WebContents* web_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tabId, GetProfile());
  if (web_contents) {
    content::RenderWidgetHostView* rwhv =
        web_contents->GetRenderWidgetHostView();
    if (rwhv) {
      text = base::UTF16ToUTF8(rwhv->GetSelectedText());
    }
  }

  results_ = vivaldi::utilities::GetSelectedText::Results::Create(text);

  SendResponse(true);
  return true;
}

UtilitiesIsUrlValidFunction::UtilitiesIsUrlValidFunction() {}

UtilitiesIsUrlValidFunction::~UtilitiesIsUrlValidFunction() {}

bool UtilitiesCreateUrlMappingFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::CreateUrlMapping::Params>
    params(vivaldi::utilities::CreateUrlMapping::Params::Create(
      *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiDataSourcesAPI* api =
    VivaldiDataSourcesAPI::GetFactoryInstance()->Get(GetProfile());

  // PathExists() triggers IO restriction.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  std::string file = params->local_path;
  std::string guid = base::GenerateGUID();
  base::FilePath path = base::FilePath::FromUTF8Unsafe(file);
  if (!base::PathExists(path)) {
    error_ = "File does not exists: " + file;
    return false;
  }
  if (!api->AddMapping(guid, path)) {
    error_ = "Mapping for file failed: " + file;
    return false;
  }
  std::string retval = kBaseFileMappingUrl + guid;
  results_ = vivaldi::utilities::CreateUrlMapping::Results::Create(retval);
  SendResponse(true);
  return true;
}

bool UtilitiesRemoveUrlMappingFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::RemoveUrlMapping::Params>
    params(vivaldi::utilities::RemoveUrlMapping::Params::Create(
      *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiDataSourcesAPI* api =
    VivaldiDataSourcesAPI::GetFactoryInstance()->Get(GetProfile());

  // Extract the ID from the given url first.
  GURL url(params->url);
  std::string id = url.ExtractFileName();

  bool success = api->RemoveMapping(id);

  results_ = vivaldi::utilities::RemoveUrlMapping::Results::Create(success);
  SendResponse(true);
  return true;
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

UtilitiesSelectFileFunction::UtilitiesSelectFileFunction() {
}
UtilitiesSelectFileFunction::~UtilitiesSelectFileFunction() {
}

bool UtilitiesSelectFileFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::SelectFile::Params>
    params(vivaldi::utilities::SelectFile::Params::Create(
      *args_));
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

  content::WebContents* web_contents = dispatcher()->GetAssociatedWebContents();

  select_file_dialog_ = ui::SelectFileDialog::Create(this, nullptr);

  gfx::NativeWindow window =
    web_contents ? platform_util::GetTopLevel(web_contents->GetNativeView())
    : NULL;

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, title,
      base::FilePath(), &file_type_info, 0, base::FilePath::StringType(),
      window, nullptr);

  return true;
}

void UtilitiesSelectFileFunction::FileSelected(const base::FilePath& path,
                                               int index,
                                               void* params) {
  results_ =
      vivaldi::utilities::SelectFile::Results::Create(path.AsUTF8Unsafe());
  SendResponse(true);
  Release();
}

void UtilitiesSelectFileFunction::FileSelectionCanceled(void* params) {
  error_ = "File selection aborted.";
  SendResponse(false);
  Release();
}

bool UtilitiesGetVersionFunction::RunAsync() {
  results_ = vivaldi::utilities::GetVersion::Results::Create(
      ::vivaldi::GetVivaldiVersionString(), version_info::GetVersionNumber());

  SendResponse(true);
  return true;
}

bool UtilitiesSetSharedDataFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::SetSharedData::Params> params(
      vivaldi::utilities::SetSharedData::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiUtilitiesAPI* api =
      VivaldiUtilitiesAPI::GetFactoryInstance()->Get(GetProfile());

  bool found = api->SetSharedData(params->key_value_pair.key,
                                  params->key_value_pair.value.get());
  results_ = vivaldi::utilities::SetSharedData::Results::Create(found);

  SendResponse(true);

  EventRouter* event_router = EventRouter::Get(GetProfile());
  if (event_router) {
    std::unique_ptr<base::ListValue> event_args(
        vivaldi::utilities::OnSharedDataUpdated::Create(
            params->key_value_pair.key, *params->key_value_pair.value));

    event_router->BroadcastEvent(base::WrapUnique(new extensions::Event(
        extensions::events::VIVALDI_EXTENSION_EVENT,
        vivaldi::utilities::OnSharedDataUpdated::kEventName,
        std::move(event_args))));
  }
  return true;
}

bool UtilitiesGetSharedDataFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::GetSharedData::Params> params(
    vivaldi::utilities::GetSharedData::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiUtilitiesAPI* api =
      VivaldiUtilitiesAPI::GetFactoryInstance()->Get(GetProfile());

  const base::Value* value = api->GetSharedData(params->key_value_pair.key);
  results_ = vivaldi::utilities::GetSharedData::Results::Create(
      value ? *value : *params->key_value_pair.value.get());

  SendResponse(true);
  return true;
}

ExtensionFunction::ResponseAction UtilitiesGetSystemDateFormatFunction::Run() {
  vivaldi::utilities::DateFormats date_formats;

  if (!ReadDateFormats(&date_formats)) {
    return RespondNow(Error(
        "Error reading date formats or not implemented on mac/linux yet"));
  } else {
    return RespondNow(
        ArgumentList(vivaldi::utilities::GetSystemDateFormat::Results::Create(
            date_formats)));
  }
}

bool UtilitiesSetLanguageFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::SetLanguage::Params> params(
      vivaldi::utilities::SetLanguage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string& language_code = params->locale;

  DCHECK(!language_code.empty());
  if (language_code.empty()) {
    error_ = "Empty language code.";
    results_ = vivaldi::utilities::SetLanguage::Results::Create(false);
    SendResponse(false);
    return true;
  }
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetString(language::prefs::kApplicationLocale, language_code);

  results_ = vivaldi::utilities::SetLanguage::Results::Create(true);
  SendResponse(true);
  return true;
}

bool UtilitiesGetLanguageFunction::RunAsync() {
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
    error_ = "Empty language code.";
    results_ = vivaldi::utilities::GetLanguage::Results::Create(language_code);
    SendResponse(false);
    return true;
  }
  // JS side expects lower case language codes, so convert it here.
  std::transform(language_code.begin(), language_code.end(),
                 language_code.begin(), ::tolower);
  results_ = vivaldi::utilities::GetLanguage::Results::Create(language_code);
  SendResponse(true);
  return true;
}

UtilitiesSetVivaldiAsDefaultBrowserFunction::
    UtilitiesSetVivaldiAsDefaultBrowserFunction()
    : weak_ptr_factory_(this) {
  default_browser_worker_ = new shell_integration::DefaultBrowserWorker(
      base::Bind(&UtilitiesSetVivaldiAsDefaultBrowserFunction::
                     OnDefaultBrowserWorkerFinished,
                 weak_ptr_factory_.GetWeakPtr()));
}

UtilitiesSetVivaldiAsDefaultBrowserFunction::
    ~UtilitiesSetVivaldiAsDefaultBrowserFunction() {
  Respond(ArgumentList(std::move(results_)));
}

void UtilitiesSetVivaldiAsDefaultBrowserFunction::
    OnDefaultBrowserWorkerFinished(
        shell_integration::DefaultWebClientState state) {
  if (state == shell_integration::IS_DEFAULT) {
    results_ =
        vivaldi::utilities::SetVivaldiAsDefaultBrowser::Results::Create(
            "true");
    Release();
  } else if (state == shell_integration::NOT_DEFAULT) {
    if (shell_integration::CanSetAsDefaultBrowser() ==
        shell_integration::SET_DEFAULT_NOT_ALLOWED) {
    } else {
      // Not default browser
      results_ =
          vivaldi::utilities::SetVivaldiAsDefaultBrowser::Results::Create(
              "false");
      Release();
    }
  } else if (state == shell_integration::UNKNOWN_DEFAULT) {
  } else {
    return;  // Still processing.
  }
}

bool UtilitiesSetVivaldiAsDefaultBrowserFunction::RunAsync() {
  AddRef();  // balanced from SetDefaultWebClientUIState()
  default_browser_worker_->StartSetAsDefault();
  return true;
}

UtilitiesIsVivaldiDefaultBrowserFunction::
    UtilitiesIsVivaldiDefaultBrowserFunction()
    : weak_ptr_factory_(this) {
  default_browser_worker_ = new shell_integration::DefaultBrowserWorker(
      base::Bind(&UtilitiesIsVivaldiDefaultBrowserFunction::
                     OnDefaultBrowserWorkerFinished,
                 weak_ptr_factory_.GetWeakPtr()));
}

UtilitiesIsVivaldiDefaultBrowserFunction::
    ~UtilitiesIsVivaldiDefaultBrowserFunction() {
  Respond(ArgumentList(std::move(results_)));
}

bool UtilitiesIsVivaldiDefaultBrowserFunction::RunAsync() {
  AddRef();  // balanced from SetDefaultWebClientUIState()
  default_browser_worker_->StartCheckIsDefault();
  return true;
}

// shell_integration::DefaultWebClientObserver implementation.
void UtilitiesIsVivaldiDefaultBrowserFunction::OnDefaultBrowserWorkerFinished(
    shell_integration::DefaultWebClientState state) {
  if (state == shell_integration::IS_DEFAULT) {
    results_ =
        vivaldi::utilities::IsVivaldiDefaultBrowser::Results::Create("true");
    Release();
  } else if (state == shell_integration::NOT_DEFAULT) {
    if (shell_integration::CanSetAsDefaultBrowser() ==
        shell_integration::SET_DEFAULT_NOT_ALLOWED) {
    } else {
      // Not default browser
      results_ = vivaldi::utilities::IsVivaldiDefaultBrowser::Results::Create(
          "false");
      Release();
    }
  } else if (state == shell_integration::UNKNOWN_DEFAULT) {
  } else {
    return;  // Still processing.
  }
}

UtilitiesLaunchNetworkSettingsFunction::
    UtilitiesLaunchNetworkSettingsFunction() {}

UtilitiesLaunchNetworkSettingsFunction::
    ~UtilitiesLaunchNetworkSettingsFunction() {}

bool UtilitiesLaunchNetworkSettingsFunction::RunAsync() {
  settings_utils::ShowNetworkProxySettings(NULL);
  SendResponse(true);
  return true;
}

UtilitiesSavePageFunction::UtilitiesSavePageFunction() {}

UtilitiesSavePageFunction::~UtilitiesSavePageFunction() {}

bool UtilitiesSavePageFunction::RunAsync() {
  Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
      dispatcher()->GetAssociatedWebContents());

  content::WebContents* current_tab =
      browser->tab_strip_model()->GetActiveWebContents();
  current_tab->OnSavePage();
  SendResponse(true);
  return true;
}

UtilitiesOpenPageFunction::UtilitiesOpenPageFunction() {}

UtilitiesOpenPageFunction::~UtilitiesOpenPageFunction() {}

bool UtilitiesOpenPageFunction::RunAsync() {
  Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
      dispatcher()->GetAssociatedWebContents());
  browser->OpenFile();
  SendResponse(true);
  return true;
}

UtilitiesSetDefaultContentSettingsFunction::
    UtilitiesSetDefaultContentSettingsFunction() {}

UtilitiesSetDefaultContentSettingsFunction::
    ~UtilitiesSetDefaultContentSettingsFunction() {}

bool UtilitiesSetDefaultContentSettingsFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::SetDefaultContentSettings::Params>
      params(vivaldi::utilities::SetDefaultContentSettings::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string& content_settings = params->content_setting;
  std::string& value = params->value;

  ContentSetting default_setting = vivContentSettingFromString(value);
  //
  ContentSettingsType content_type =
      vivContentSettingsTypeFromGroupName(content_settings);

  Profile* profile = GetProfile()->GetOriginalProfile();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  map->SetDefaultContentSetting(content_type, default_setting);

  results_ = vivaldi::utilities::SetDefaultContentSettings::Results::Create(
      "success");
  SendResponse(true);
  return true;
}

UtilitiesGetDefaultContentSettingsFunction::
    UtilitiesGetDefaultContentSettingsFunction() {}

UtilitiesGetDefaultContentSettingsFunction::
    ~UtilitiesGetDefaultContentSettingsFunction() {}

bool UtilitiesGetDefaultContentSettingsFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::GetDefaultContentSettings::Params>
      params(vivaldi::utilities::GetDefaultContentSettings::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  std::string& content_settings = params->content_setting;
  std::string def_provider = "";
  ContentSettingsType content_type =
      vivContentSettingsTypeFromGroupName(content_settings);
  ContentSetting default_setting;
  Profile* profile = GetProfile()->GetOriginalProfile();

  default_setting = HostContentSettingsMapFactory::GetForProfile(profile)
                        ->GetDefaultContentSetting(content_type, &def_provider);

  std::string setting = vivContentSettingToString(default_setting);

  results_ =
      vivaldi::utilities::GetDefaultContentSettings::Results::Create(setting);
  SendResponse(true);
  return true;
}

UtilitiesSetBlockThirdPartyCookiesFunction::
    UtilitiesSetBlockThirdPartyCookiesFunction() {}

UtilitiesSetBlockThirdPartyCookiesFunction::
    ~UtilitiesSetBlockThirdPartyCookiesFunction() {}

bool UtilitiesSetBlockThirdPartyCookiesFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::SetBlockThirdPartyCookies::Params>
      params(vivaldi::utilities::SetBlockThirdPartyCookies::Params::Create(
          *args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());
  bool block3rdparty = params->block3rd_party;

  PrefService* pref_service = GetProfile()->GetOriginalProfile()->GetPrefs();
  pref_service->SetBoolean(prefs::kBlockThirdPartyCookies, block3rdparty);

  PrefService* service = GetProfile()->GetOriginalProfile()->GetPrefs();
  bool blockThirdParty = service->GetBoolean(prefs::kBlockThirdPartyCookies);

  results_ = vivaldi::utilities::SetBlockThirdPartyCookies::Results::Create(
      blockThirdParty);
  SendResponse(true);
  return true;
}

UtilitiesGetBlockThirdPartyCookiesFunction::
    UtilitiesGetBlockThirdPartyCookiesFunction() {}

UtilitiesGetBlockThirdPartyCookiesFunction::
    ~UtilitiesGetBlockThirdPartyCookiesFunction() {}

bool UtilitiesGetBlockThirdPartyCookiesFunction::RunAsync() {
  PrefService* service = GetProfile()->GetOriginalProfile()->GetPrefs();
  bool blockThirdParty = service->GetBoolean(prefs::kBlockThirdPartyCookies);

  results_ = vivaldi::utilities::GetBlockThirdPartyCookies::Results::Create(
      blockThirdParty);
  SendResponse(true);
  return true;
}

UtilitiesOpenTaskManagerFunction::~UtilitiesOpenTaskManagerFunction() {}

bool UtilitiesOpenTaskManagerFunction::RunAsync() {
  Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
      dispatcher()->GetAssociatedWebContents());

  chrome::OpenTaskManager(browser);

  SendResponse(true);
  return true;
}

UtilitiesOpenTaskManagerFunction::UtilitiesOpenTaskManagerFunction() {}


UtilitiesGetStartupActionFunction::UtilitiesGetStartupActionFunction() {}

UtilitiesGetStartupActionFunction::~UtilitiesGetStartupActionFunction() {}

bool UtilitiesGetStartupActionFunction::RunAsync() {
  Profile* profile = GetProfile();
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
  results_ =
      vivaldi::utilities::GetStartupAction::Results::Create(startupRes);

  SendResponse(true);
  return true;
}

UtilitiesSetStartupActionFunction::UtilitiesSetStartupActionFunction() {}

UtilitiesSetStartupActionFunction::~UtilitiesSetStartupActionFunction() {}

bool UtilitiesSetStartupActionFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::SetStartupAction::Params> params(
      vivaldi::utilities::SetStartupAction::Params::Create(*args_));
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

  Profile* profile = GetProfile();
  PrefService* prefs = profile->GetPrefs();

  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  results_ =
      vivaldi::utilities::GetStartupAction::Results::Create(content_settings);

  SendResponse(true);
  return true;
}

bool UtilitiesCanShowWelcomePageFunction::RunAsync() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  bool force_first_run = command_line->HasSwitch(switches::kForceFirstRun);
  bool no_first_run = command_line->HasSwitch(switches::kNoFirstRun);
  bool can_show_welcome = (force_first_run || !no_first_run);

  results_ =
      vivaldi::utilities::CanShowWelcomePage::Results::Create(can_show_welcome);

  SendResponse(true);
  return true;
}

}  // namespace extensions
