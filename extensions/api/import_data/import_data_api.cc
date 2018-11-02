// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/import_data/import_data_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/vivaldi_resources.h"

#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/settings_utils.h"
#include "chrome/browser/ui/webui/site_settings_helper.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/pref_names.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/url_pattern_set.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_finder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"

#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include "chrome/browser/prefs/session_startup_pref.h"

#include "content/public/browser/render_view_host.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/extension_messages.h"

#include "extensions/schema/import_data.h"

#include "components/strings/grit/components_strings.h"
#include "ui/vivaldi_ui_utils.h"

class Browser;

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

}

namespace GetProfiles = vivaldi::import_data::GetProfiles;
using content::WebContents;

// ProfileSingletonFactory
bool ProfileSingletonFactory::instanceFlag = false;
ProfileSingletonFactory* ProfileSingletonFactory::single = NULL;

ProfileSingletonFactory* ProfileSingletonFactory::getInstance() {
  if (!instanceFlag) {
    single = new ProfileSingletonFactory();
    instanceFlag = true;
    return single;
  } else {
    return single;
  }
}

ProfileSingletonFactory::ProfileSingletonFactory() {
  api_importer_list_.reset(new ImporterList());
  profilesRequested = false;
}

ProfileSingletonFactory::~ProfileSingletonFactory() {
  instanceFlag = false;
  profilesRequested = false;
}

ImporterList* ProfileSingletonFactory::getImporterList() {
  return single->api_importer_list_.get();
}

void ProfileSingletonFactory::setProfileRequested(bool profileReq) {
  profilesRequested = profileReq;
}

bool ProfileSingletonFactory::getProfileRequested() {
  return profilesRequested;
}

namespace {

// These are really flags, but never sent as flags.
static struct ImportItemToStringMapping {
  importer::ImportItem item;
  const char* name;
} import_item_string_mapping[]{
    // clang-format off
    {importer::FAVORITES, "favorites"},
    {importer::PASSWORDS, "passwords"},
    {importer::HISTORY, "history"},
    {importer::COOKIES, "cookies"},
    {importer::SEARCH_ENGINES, "search"},
    {importer::NOTES, "notes"},
    {importer::SPEED_DIAL, "speeddial"},
    // clang-format on
};

const size_t kImportItemToStringMappingLength =
    arraysize(import_item_string_mapping);

const std::string ImportItemToString(importer::ImportItem item) {
  for (size_t i = 0; i < kImportItemToStringMappingLength; i++) {
    if (item == import_item_string_mapping[i].item) {
      return import_item_string_mapping[i].name;
    }
  }
  // Missing datatype in the array?
  NOTREACHED();
  return nullptr;
}
}  // namespace

ImportDataEventRouter::ImportDataEventRouter(Profile* profile)
    : browser_context_(profile) {}

ImportDataEventRouter::~ImportDataEventRouter() {}

// Helper to actually dispatch an event to extension listeners.
void ImportDataEventRouter::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

ImportDataAPI::ImportDataAPI(content::BrowserContext* context)
    : browser_context_(context),
      importer_host_(nullptr),
      import_succeeded_count_(0) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(
      this, vivaldi::import_data::OnImportStarted::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::import_data::OnImportEnded::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::import_data::OnImportItemStarted::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::import_data::OnImportItemEnded::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::import_data::OnImportItemFailed::kEventName);
}

ImportDataAPI::~ImportDataAPI() {}

void ImportDataAPI::StartImport(const importer::SourceProfile& source_profile,
                                uint16_t imported_items) {
  if (!imported_items)
    return;

  import_succeeded_count_ = 0;

  DCHECK(!importer_host_);

  // If another import is already ongoing, let it finish silently.
  if (importer_host_)
    importer_host_->set_observer(NULL);

  base::Value importing(true);

  importer_host_ = new ExternalProcessImporterHost();
  importer_host_->set_observer(this);

  Profile* profile = static_cast<Profile*>(browser_context_);

  importer_host_->StartImportSettings(source_profile, profile, imported_items,
                                      new ProfileWriter(profile));
}

void ImportDataAPI::ImportStarted() {
  event_router_->DispatchEvent(
      vivaldi::import_data::OnImportStarted::kEventName,
      vivaldi::import_data::OnImportStarted::Create());
}

void ImportDataAPI::ImportItemStarted(importer::ImportItem item) {
  import_succeeded_count_++;
  const std::string item_name = ImportItemToString(item);

  event_router_->DispatchEvent(
      vivaldi::import_data::OnImportItemStarted::kEventName,
      vivaldi::import_data::OnImportItemStarted::Create(item_name));
}

void ImportDataAPI::ImportItemEnded(importer::ImportItem item) {
  import_succeeded_count_--;
  const std::string item_name = ImportItemToString(item);

  event_router_->DispatchEvent(
      vivaldi::import_data::OnImportItemEnded::kEventName,
      vivaldi::import_data::OnImportItemEnded::Create(item_name));
}
void ImportDataAPI::ImportItemFailed(importer::ImportItem item,
                                     const std::string& error) {
  // Ensure we get an error at the end.
  import_succeeded_count_++;
  const std::string item_name = ImportItemToString(item);

  event_router_->DispatchEvent(
      vivaldi::import_data::OnImportItemFailed::kEventName,
      vivaldi::import_data::OnImportItemFailed::Create(item_name, error));
}

void ImportDataAPI::ImportEnded() {
  importer_host_->set_observer(NULL);
  importer_host_ = NULL;

  event_router_->DispatchEvent(
      vivaldi::import_data::OnImportEnded::kEventName,
      vivaldi::import_data::OnImportEnded::Create(import_succeeded_count_));
}

void ImportDataAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<ImportDataAPI> >::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ImportDataAPI>*
ImportDataAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void ImportDataAPI::OnListenerAdded(const EventListenerInfo& details) {
  event_router_.reset(
      new ImportDataEventRouter(Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

ImporterApiFunction::ImporterApiFunction() {}

ImporterApiFunction::~ImporterApiFunction() {}

void ImporterApiFunction::SendResponseToCallback() {
  SendResponse(true);
  Release();  // Balanced in RunAsync().
}

bool ImporterApiFunction::RunAsync() {
  AddRef();  // Balanced in SendAsyncResponse() and below.
  bool retval = RunAsyncImpl();
  if (false == retval)
    Release();
  return retval;
}

void ImporterApiFunction::SendAsyncResponse() {
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&ImporterApiFunction::SendResponseToCallback,
                            base::Unretained(this)));
}

void ImporterApiFunction::Finished() {
  std::vector<vivaldi::import_data::ProfileItem> nodes;

  for (size_t i = 0; i < api_importer_list->count(); ++i) {
    const importer::SourceProfile& source_profile =
        api_importer_list->GetSourceProfileAt(i);
    vivaldi::import_data::ProfileItem* profile =
        new vivaldi::import_data::ProfileItem;

    uint16_t browser_services = source_profile.services_supported;

    profile->name = base::UTF16ToUTF8(source_profile.importer_name);
    profile->index = i;

    profile->history = ((browser_services & importer::HISTORY) != 0);
    profile->favorites = ((browser_services & importer::FAVORITES) != 0);
    profile->passwords = ((browser_services & importer::PASSWORDS) != 0);
    profile->supports_master_password =
        ((browser_services & importer::MASTER_PASSWORD) != 0);
    profile->search = ((browser_services & importer::SEARCH_ENGINES) != 0);
    profile->notes = ((browser_services & importer::NOTES) != 0);
    profile->speeddial = ((browser_services & importer::SPEED_DIAL) != 0);

    profile->supports_standalone_import =
        (source_profile.importer_type == importer::TYPE_OPERA ||
         source_profile.importer_type == importer::TYPE_VIVALDI);

    profile->profile_path =
#if defined(OS_WIN)
        base::UTF16ToUTF8(source_profile.source_path.value());
#else
        source_profile.source_path.value();
#endif  // defined(OS_WIN)
    profile->has_default_install = !source_profile.source_path.empty();

    if (profile->supports_standalone_import) {
      profile->will_show_dialog_type = "folder";
    } else if (source_profile.importer_type ==
                   importer::TYPE_OPERA_BOOKMARK_FILE ||
               source_profile.importer_type == importer::TYPE_BOOKMARKS_FILE) {
      profile->will_show_dialog_type = "file";
    } else {
      profile->will_show_dialog_type = "none";
    }

    std::vector<vivaldi::import_data::UserProfileItem> profileItems;

    for (size_t i = 0; i < source_profile.user_profile_names.size(); ++i) {
      vivaldi::import_data::UserProfileItem* profItem =
          new vivaldi::import_data::UserProfileItem;

      profItem->profile_display_name = base::UTF16ToUTF8(
          source_profile.user_profile_names.at(i).profileDisplayName);
      profItem->profile_name =
          source_profile.user_profile_names.at(i).profileName;

      profileItems.push_back(std::move(*profItem));
    }

    profile->user_profiles = std::move(profileItems);

    nodes.push_back(std::move(*profile));
  }

  results_ = vivaldi::import_data::GetProfiles::Results::Create(nodes);
  SendAsyncResponse();
}

ImportDataGetProfilesFunction::ImportDataGetProfilesFunction() {}

bool ImportDataGetProfilesFunction::RunAsyncImpl() {
  ProfileSingletonFactory* singl = ProfileSingletonFactory::getInstance();
  api_importer_list = singl->getInstance()->getImporterList();

  // if (!singl->getProfileRequested() &&
  // !api_importer_list->count()){
  singl->setProfileRequested(true);
  // AddRef(); // Balanced in OnSourceProfilesLoaded
  api_importer_list->DetectSourceProfiles(
      g_browser_process->GetApplicationLocale(), true,
      base::Bind(&ImporterApiFunction::Finished, base::Unretained(this)));
  return true;
  // }
}

ImportDataGetProfilesFunction::~ImportDataGetProfilesFunction() {}

ImportDataStartImportFunction::ImportDataStartImportFunction() {}

// ExtensionFunction:
bool ImportDataStartImportFunction::RunAsyncImpl() {
  api_importer_list =
      ProfileSingletonFactory::getInstance()->getInstance()->getImporterList();

  std::unique_ptr<vivaldi::import_data::StartImport::Params> params(
      vivaldi::import_data::StartImport::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::string16 dialog_title;

  std::vector<std::string>& ids = params->items_to_import;
  size_t count = ids.size();
  EXTENSION_FUNCTION_VALIDATE(count > 0);

  int browser_index;
  if (!base::StringToInt(ids[0], &browser_index)) {
    NOTREACHED();
    return false;
  }

  importer::SourceProfile source_profile =
      api_importer_list->GetSourceProfileAt(browser_index);
  int supported_items = source_profile.services_supported;
  int selected_items = importer::NONE;

  if (ids[1] == "true") {
    selected_items |= importer::HISTORY;
  }
  if (ids[2] == "true") {
    selected_items |= importer::FAVORITES;
  }
  if (ids[3] == "true") {
    selected_items |= importer::PASSWORDS;
  }
  if (ids[4] == "true") {
    selected_items |= importer::SEARCH_ENGINES;
  }
  if (ids[5] == "true") {
    selected_items |= importer::NOTES;
  }
  source_profile.selected_profile_name = ids[7];
  if (params->master_password.get()) {
    source_profile.master_password = *params->master_password;
  }
  if (ids[8] == "true") {
    selected_items |= importer::SPEED_DIAL;
  }

  int imported_items = (selected_items & supported_items);

  base::FilePath path;
  if (source_profile.importer_type == importer::TYPE_BOOKMARKS_FILE) {
    dialog_title =
        l10n_util::GetStringUTF16(IDS_IMPORT_HTML_BOOKMARKS_FILE_TITLE);
    HandleChooseBookmarksFileOrFolder(dialog_title, "html", imported_items,
                                      source_profile.importer_type, path, true);
    return true;
  } else if (source_profile.importer_type ==
             importer::TYPE_OPERA_BOOKMARK_FILE) {
    dialog_title =
        l10n_util::GetStringUTF16(IDS_IMPORT_OPERA_BOOKMARKS_FILE_TITLE);
    path = path.AppendASCII("bookmarks.adr");
    HandleChooseBookmarksFileOrFolder(dialog_title, "adr", imported_items,
                                      source_profile.importer_type, path, true);
    return true;
  } else if ((source_profile.importer_type == importer::TYPE_OPERA ||
              source_profile.importer_type == importer::TYPE_VIVALDI) &&
             ids[6] == "false") {
    dialog_title = l10n_util::GetStringUTF16(
        source_profile.importer_type == importer::TYPE_VIVALDI
            ? IDS_IMPORT_VIVALDI_PROFILE_TITLE
            : IDS_IMPORT_OPERA_PROFILE_TITLE);
    HandleChooseBookmarksFileOrFolder(dialog_title, "ini", imported_items,
                                      source_profile.importer_type, path,
                                      false);
    return true;
  } else {
    if (imported_items) {
      StartImport(source_profile, imported_items);
    } else {
      LOG(WARNING) << "There were no settings to import from '"
                   << source_profile.importer_name << "'.";
    }
  }
  return true;
}

void ImportDataStartImportFunction::HandleChooseBookmarksFileOrFolder(
    const base::string16& title,
    const std::string& extension,
    int imported_items,
    importer::ImporterType importer_type,
    const base::FilePath& default_file,
    bool file_selection) {
  if (!dispatcher())
    return;  // Extension was unloaded.

  ui::SelectFileDialog::FileTypeInfo file_type_info;

  if (file_selection) {
    file_type_info.extensions.resize(1);

    file_type_info.extensions[0].push_back(
        base::FilePath::FromUTF8Unsafe(extension).value());
  }
  AddRef();

  WebContents* web_contents = dispatcher()->GetAssociatedWebContents();

  select_file_dialog_ = ui::SelectFileDialog::Create(this, nullptr);

  gfx::NativeWindow window =
      web_contents ? platform_util::GetTopLevel(web_contents->GetNativeView())
                   : NULL;

  DialogParams* params = new DialogParams();
  params->imported_items = imported_items;
  params->importer_type = importer_type;
  params->file_dialog = file_selection;

  select_file_dialog_->SelectFile(
      file_selection ? ui::SelectFileDialog::SELECT_OPEN_FILE
                     : ui::SelectFileDialog::SELECT_FOLDER,
      title, default_file, &file_type_info, 0, base::FilePath::StringType(),
      window, reinterpret_cast<void*>(params));
}

void ImportDataStartImportFunction::FileSelectionCanceled(void* params) {
  delete reinterpret_cast<DialogParams*>(params);
  results_ = vivaldi::import_data::StartImport::Results::Create("Cancel");
  SendAsyncResponse();

  Release();  // Balanced in HandleChooseBookmarksFileOrFolder()
}

void ImportDataStartImportFunction::FileSelected(const base::FilePath& path,
                                                 int /*index*/,
                                                 void* params) {
  DialogParams* dlgparams = reinterpret_cast<DialogParams*>(params);
  int imported_items = dlgparams->imported_items;
  importer::ImporterType importer_type = dlgparams->importer_type;
  delete dlgparams;

  importer::SourceProfile source_profile;
  source_profile.source_path = path;
  source_profile.importer_type = importer_type;

  StartImport(source_profile, imported_items);

  Release();  // Balanced in HandleChooseBookmarksFileOrFolder()
}

ImportDataStartImportFunction::~ImportDataStartImportFunction() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void ImportDataStartImportFunction::StartImport(
    const importer::SourceProfile& source_profile,
    uint16_t imported_items) {
  ImportDataAPI::GetFactoryInstance()
      ->Get(GetProfile())
      ->StartImport(source_profile, imported_items);
}

ImportDataSetVivaldiAsDefaultBrowserFunction::
    ImportDataSetVivaldiAsDefaultBrowserFunction()
    : weak_ptr_factory_(this) {
  default_browser_worker_ = new shell_integration::DefaultBrowserWorker(
      base::Bind(&ImportDataSetVivaldiAsDefaultBrowserFunction::
                     OnDefaultBrowserWorkerFinished,
                 weak_ptr_factory_.GetWeakPtr()));
}

ImportDataSetVivaldiAsDefaultBrowserFunction::
    ~ImportDataSetVivaldiAsDefaultBrowserFunction() {
  Respond(ArgumentList(std::move(results_)));
}

void ImportDataSetVivaldiAsDefaultBrowserFunction::
    OnDefaultBrowserWorkerFinished(
        shell_integration::DefaultWebClientState state) {
  if (state == shell_integration::IS_DEFAULT) {
    results_ =
        vivaldi::import_data::SetVivaldiAsDefaultBrowser::Results::Create(
            "true");
    Release();
  } else if (state == shell_integration::NOT_DEFAULT) {
    if (shell_integration::CanSetAsDefaultBrowser() ==
        shell_integration::SET_DEFAULT_NOT_ALLOWED) {
    } else {
      // Not default browser
      results_ =
          vivaldi::import_data::SetVivaldiAsDefaultBrowser::Results::Create(
              "false");
      Release();
    }
  } else if (state == shell_integration::UNKNOWN_DEFAULT) {
  } else {
    return;  // Still processing.
  }
}

bool ImportDataSetVivaldiAsDefaultBrowserFunction::RunAsync() {
  AddRef();  // balanced from SetDefaultWebClientUIState()
  default_browser_worker_->StartSetAsDefault();
  return true;
}

ImportDataIsVivaldiDefaultBrowserFunction::
    ImportDataIsVivaldiDefaultBrowserFunction()
    : weak_ptr_factory_(this) {
  default_browser_worker_ = new shell_integration::DefaultBrowserWorker(
      base::Bind(&ImportDataIsVivaldiDefaultBrowserFunction::
                     OnDefaultBrowserWorkerFinished,
                 weak_ptr_factory_.GetWeakPtr()));
}

ImportDataIsVivaldiDefaultBrowserFunction::
    ~ImportDataIsVivaldiDefaultBrowserFunction() {
  Respond(ArgumentList(std::move(results_)));
}

bool ImportDataIsVivaldiDefaultBrowserFunction::RunAsync() {
  AddRef();  // balanced from SetDefaultWebClientUIState()
  default_browser_worker_->StartCheckIsDefault();
  return true;
}

// shell_integration::DefaultWebClientObserver implementation.
void ImportDataIsVivaldiDefaultBrowserFunction::OnDefaultBrowserWorkerFinished(
    shell_integration::DefaultWebClientState state) {
  if (state == shell_integration::IS_DEFAULT) {
    results_ =
        vivaldi::import_data::IsVivaldiDefaultBrowser::Results::Create("true");
    Release();
  } else if (state == shell_integration::NOT_DEFAULT) {
    if (shell_integration::CanSetAsDefaultBrowser() ==
        shell_integration::SET_DEFAULT_NOT_ALLOWED) {
    } else {
      // Not default browser
      results_ = vivaldi::import_data::IsVivaldiDefaultBrowser::Results::Create(
          "false");
      Release();
    }
  } else if (state == shell_integration::UNKNOWN_DEFAULT) {
  } else {
    return;  // Still processing.
  }
}

ImportDataLaunchNetworkSettingsFunction::
    ImportDataLaunchNetworkSettingsFunction() {}

ImportDataLaunchNetworkSettingsFunction::
    ~ImportDataLaunchNetworkSettingsFunction() {}

bool ImportDataLaunchNetworkSettingsFunction::RunAsync() {
  settings_utils::ShowNetworkProxySettings(NULL);
  SendResponse(true);
  return true;
}

ImportDataSavePageFunction::ImportDataSavePageFunction() {}

ImportDataSavePageFunction::~ImportDataSavePageFunction() {}

bool ImportDataSavePageFunction::RunAsync() {
  Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
      dispatcher()->GetAssociatedWebContents());

  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  current_tab->OnSavePage();
  SendResponse(true);
  return true;
}

ImportDataOpenPageFunction::ImportDataOpenPageFunction() {}

ImportDataOpenPageFunction::~ImportDataOpenPageFunction() {}

bool ImportDataOpenPageFunction::RunAsync() {
  Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
      dispatcher()->GetAssociatedWebContents());
  browser->OpenFile();
  SendResponse(true);
  return true;
}

ImportDataSetVivaldiLanguageFunction::ImportDataSetVivaldiLanguageFunction() {}

ImportDataSetVivaldiLanguageFunction::~ImportDataSetVivaldiLanguageFunction() {}

bool ImportDataSetVivaldiLanguageFunction::RunAsync() {
  std::unique_ptr<vivaldi::import_data::SetVivaldiLanguage::Params> params(
      vivaldi::import_data::SetVivaldiLanguage::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());
  std::string& language_code = params->locale;

  CHECK(!language_code.empty());

  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetString(prefs::kApplicationLocale, language_code);
  SendResponse(true);
  return true;
}

ImportDataSetDefaultContentSettingsFunction::
    ImportDataSetDefaultContentSettingsFunction() {}

ImportDataSetDefaultContentSettingsFunction::
    ~ImportDataSetDefaultContentSettingsFunction() {}

bool ImportDataSetDefaultContentSettingsFunction::RunAsync() {
  std::unique_ptr<vivaldi::import_data::SetDefaultContentSettings::Params>
      params(vivaldi::import_data::SetDefaultContentSettings::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string& content_settings = params->content_setting;
  std::string& value = params->value;

  ContentSetting default_setting = vivContentSettingFromString(value);
  //
  ContentSettingsType content_type =
      vivContentSettingsTypeFromGroupName(content_settings);

  Profile* profile = GetProfile();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  map->SetDefaultContentSetting(content_type, default_setting);

  results_ = vivaldi::import_data::SetDefaultContentSettings::Results::Create(
      "success");
  SendResponse(true);
  return true;
}

ImportDataGetDefaultContentSettingsFunction::
    ImportDataGetDefaultContentSettingsFunction() {}

ImportDataGetDefaultContentSettingsFunction::
    ~ImportDataGetDefaultContentSettingsFunction() {}

bool ImportDataGetDefaultContentSettingsFunction::RunAsync() {
  std::unique_ptr<vivaldi::import_data::GetDefaultContentSettings::Params>
      params(vivaldi::import_data::GetDefaultContentSettings::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  std::string& content_settings = params->content_setting;
  std::string def_provider = "";
  ContentSettingsType content_type =
      vivContentSettingsTypeFromGroupName(content_settings);
  ContentSetting default_setting;
  Profile* profile = GetProfile();

  default_setting = HostContentSettingsMapFactory::GetForProfile(profile)
                        ->GetDefaultContentSetting(content_type, &def_provider);

  std::string setting = vivContentSettingToString(default_setting);

  results_ =
      vivaldi::import_data::GetDefaultContentSettings::Results::Create(setting);
  SendResponse(true);
  return true;
}

ImportDataSetBlockThirdPartyCookiesFunction::
    ImportDataSetBlockThirdPartyCookiesFunction() {}

ImportDataSetBlockThirdPartyCookiesFunction::
    ~ImportDataSetBlockThirdPartyCookiesFunction() {}

bool ImportDataSetBlockThirdPartyCookiesFunction::RunAsync() {
  std::unique_ptr<vivaldi::import_data::SetBlockThirdPartyCookies::Params>
      params(vivaldi::import_data::SetBlockThirdPartyCookies::Params::Create(
          *args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());
  bool block3rdparty = params->block3rd_party;

  PrefService* pref_service = GetProfile()->GetPrefs();
  pref_service->SetBoolean(prefs::kBlockThirdPartyCookies, block3rdparty);

  PrefService* service = GetProfile()->GetPrefs();
  bool blockThirdParty = service->GetBoolean(prefs::kBlockThirdPartyCookies);

  results_ = vivaldi::import_data::SetBlockThirdPartyCookies::Results::Create(
      blockThirdParty);
  SendResponse(true);
  return true;
}

ImportDataGetBlockThirdPartyCookiesFunction::
    ImportDataGetBlockThirdPartyCookiesFunction() {}

ImportDataGetBlockThirdPartyCookiesFunction::
    ~ImportDataGetBlockThirdPartyCookiesFunction() {}

bool ImportDataGetBlockThirdPartyCookiesFunction::RunAsync() {
  PrefService* service = GetProfile()->GetPrefs();
  bool blockThirdParty = service->GetBoolean(prefs::kBlockThirdPartyCookies);

  results_ = vivaldi::import_data::GetBlockThirdPartyCookies::Results::Create(
      blockThirdParty);
  SendResponse(true);
  return true;
}

ImportDataOpenTaskManagerFunction::~ImportDataOpenTaskManagerFunction() {}

bool ImportDataOpenTaskManagerFunction::RunAsync() {
  Browser* browser = ::vivaldi::FindBrowserForEmbedderWebContents(
      dispatcher()->GetAssociatedWebContents());

  chrome::OpenTaskManager(browser);

  SendResponse(true);
  return true;
}

ImportDataOpenTaskManagerFunction::ImportDataOpenTaskManagerFunction() {}

ImportDataGetStartupActionFunction::ImportDataGetStartupActionFunction() {}

ImportDataGetStartupActionFunction::~ImportDataGetStartupActionFunction() {}

bool ImportDataGetStartupActionFunction::RunAsync() {
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
      vivaldi::import_data::GetStartupAction::Results::Create(startupRes);

  SendResponse(true);
  return true;
}

ImportDataSetStartupActionFunction::ImportDataSetStartupActionFunction() {}

ImportDataSetStartupActionFunction::~ImportDataSetStartupActionFunction() {}

bool ImportDataSetStartupActionFunction::RunAsync() {
  std::unique_ptr<vivaldi::import_data::SetStartupAction::Params> params(
      vivaldi::import_data::SetStartupAction::Params::Create(*args_));
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
      vivaldi::import_data::GetStartupAction::Results::Create(content_settings);

  SendResponse(true);
  return true;
}

}  // namespace extensions
