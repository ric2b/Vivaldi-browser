//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_version_info.h"
#include "base/guid.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/datasource/vivaldi_data_source_api.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
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
}

VivaldiUtilitiesAPI::~VivaldiUtilitiesAPI() {}

void VivaldiUtilitiesAPI::Shutdown() {
  for (auto it : key_to_values_map_) {
    // Get rid of the allocated items
    delete it.second;
  }
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI> >::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI>*
VivaldiUtilitiesAPI::GetFactoryInstance() {
  return g_factory.Pointer();
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
        content::BrowserThread::FILE, FROM_HERE,
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
}

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

}  // namespace extensions
