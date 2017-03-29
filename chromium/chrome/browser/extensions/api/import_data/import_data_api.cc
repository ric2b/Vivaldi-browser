// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/import_data/import_data_api.h"

#include <string>

#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/options/advanced_options_utils.h"
#include "chrome/browser/ui/webui/options/content_settings_handler.h"
#include "chrome/common/extensions/api/import_data.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/pref_names.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/url_pattern_set.h"

#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/app/chrome_command_ids.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_commands.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/prefs/session_startup_pref.h"

#include "content/public/browser/render_view_host.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/extension_messages.h"

class Browser;


namespace extensions {
namespace GetProfiles = api::import_data::GetProfiles;
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

ProfileSingletonFactory::ProfileSingletonFactory(){
  api_importer_list_.reset(new ImporterList());
  profilesRequested = false;
}

ProfileSingletonFactory::~ProfileSingletonFactory(){
  instanceFlag = false;
  profilesRequested = false;
}

ImporterList *ProfileSingletonFactory::getImporterList(){
  return single->api_importer_list_.get();
}

void ProfileSingletonFactory::setProfileRequested(bool profileReq){
  profilesRequested = profileReq;
}

bool ProfileSingletonFactory::getProfileRequested(){
  return profilesRequested;
}

ImporterApiFunction::ImporterApiFunction(){

}

ImporterApiFunction::~ImporterApiFunction(){

}

void ImporterApiFunction::SendResponseToCallback() {
  SendResponse(true);
  Release();  // Balanced in RunAsync().
}

bool ImporterApiFunction::RunAsync() {
  AddRef();  // Balanced in SendAysncRepose() and below.
  bool retval = RunAsyncImpl();
  if (false == retval)
    Release();
  return retval;
}

void ImporterApiFunction::SendAsyncResponse() {
  base::MessageLoop::current()->PostTask(
    FROM_HERE,
    base::Bind(&ImporterApiFunction::SendResponseToCallback, this));
}

void ImporterApiFunction::Finished() {
  std::vector<linked_ptr<api::import_data::ProfileItem>> nodes;

  for (size_t i = 0; i < api_importer_list->count(); ++i) {
    const importer::SourceProfile& source_profile =
        api_importer_list->GetSourceProfileAt(i);
    api::import_data::ProfileItem* profile = new api::import_data::ProfileItem;

    uint16 browser_services = source_profile.services_supported;

    profile->name = base::UTF16ToUTF8(source_profile.importer_name);
    profile->index = i;

    profile->history = ((browser_services & importer::HISTORY) != 0);
    profile->favorites = ((browser_services & importer::FAVORITES) != 0);
    profile->passwords = ((browser_services & importer::PASSWORDS) != 0);
    profile->search = ((browser_services & importer::SEARCH_ENGINES) != 0);
    profile->notes = ((browser_services & importer::NOTES) != 0);

    std::vector<linked_ptr<api::import_data::UserProfileItem>> profileItems;

    for (size_t i = 0; i < source_profile.user_profile_names.size(); ++i) {
      api::import_data::UserProfileItem* profItem =
          new api::import_data::UserProfileItem;

      profItem->profile_display_name = base::UTF16ToUTF8(
          source_profile.user_profile_names.at(i).profileDisplayName);
      profItem->profile_name =
          source_profile.user_profile_names.at(i).profileName;

      linked_ptr<api::import_data::UserProfileItem> new_prof_node(profItem);

      profileItems.push_back(new_prof_node);
    }

    profile->user_profiles = profileItems;

    linked_ptr<api::import_data::ProfileItem> new_node(profile);
    nodes.push_back(new_node);
  }

  results_ = api::import_data::GetProfiles::Results::Create(nodes);
  SendAsyncResponse();
}

ImportDataGetProfilesFunction::ImportDataGetProfilesFunction() {
}

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

ImportDataGetProfilesFunction::~ImportDataGetProfilesFunction() {
}

ImportDataStartImportFunction::ImportDataStartImportFunction()
    : importer_host_(NULL), import_did_succeed_(false) {
}

// ExtensionFunction:
bool ImportDataStartImportFunction::RunAsyncImpl() {
  api_importer_list =
      ProfileSingletonFactory::getInstance()->getInstance()->getImporterList();

  scoped_ptr<api::import_data::StartImport::Params> params(
      api::import_data::StartImport::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string string_value;

  std::vector<std::string>& ids = params->items_to_import;
  size_t count = ids.size();
  EXTENSION_FUNCTION_VALIDATE(count > 0);

  int browser_index;
  if (!base::StringToInt(ids[0], &browser_index)) {
    NOTREACHED();
    return false;
  }

  importer::SourceProfile& source_profile =
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

  int imported_items = (selected_items & supported_items);

  if (source_profile.importer_type == importer::TYPE_BOOKMARKS_FILE) {
    base::ListValue* args = new base::ListValue;
    args->Append(new base::StringValue("html"));
    HandleChooseBookmarksFile(args);
    return true;
  } else if (source_profile.importer_type ==
             importer::TYPE_OPERA_BOOKMARK_FILE) {
    base::ListValue* args = new base::ListValue;
    args->Append(new base::StringValue("adr"));
    HandleChooseBookmarksFile(args);
    return true;
  } else if (source_profile.importer_type == importer::TYPE_OPERA &&
             ids[6] == "false") {
    base::ListValue* args = new base::ListValue;
    args->Append(new base::StringValue("ini"));
    args->Append(new base::FundamentalValue(imported_items));
    HandleChooseBookmarksFile(args);
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

void ImportDataStartImportFunction::HandleChooseBookmarksFile(
    const base::ListValue* args) {
  std::string string_value;
  int imported_items = 0;

  if (!args->GetString(0, &string_value)) {
    NOTREACHED();
    return;
  }
  // Only set for .ini file import.
  args->GetInteger(1, &imported_items);

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);

  file_type_info.extensions[0].push_back(
      base::FilePath::FromUTF8Unsafe(string_value).value());

  if (!dispatcher())
    return;  // Extension was unloaded.

  AddRef();

  WebContents* web_contents = dispatcher()->GetAssociatedWebContents();

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_contents));

  gfx::NativeWindow window =
      web_contents ? platform_util::GetTopLevel(web_contents->GetNativeView())
                   : NULL;

  DialogParams* params = new DialogParams();
  params->imported_items = imported_items;
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, base::string16(),
      base::FilePath(), &file_type_info, 0, base::FilePath::StringType(),
      window, reinterpret_cast<void *>(params));
}

void ImportDataStartImportFunction::FileSelectionCanceled(void* params) {
  delete reinterpret_cast<DialogParams*>(params);
  results_ = api::import_data::StartImport::Results::Create("Cancel");
  SendAsyncResponse();

  Release();  // Balanced in HandleChooseBookmarksFile
}

void ImportDataStartImportFunction::FileSelected(const base::FilePath& path,
                                                 int /*index*/,
                                                 void* params) {
  importer::SourceProfile source_profile;
  source_profile.source_path = path;
  DialogParams* dlgparams = reinterpret_cast<DialogParams*>(params);
  int imported_items = dlgparams->imported_items;
  delete dlgparams;

  if (path.MatchesExtension(FILE_PATH_LITERAL(".html"))) {
    source_profile.importer_type = importer::TYPE_BOOKMARKS_FILE;
  } else if (path.MatchesExtension(FILE_PATH_LITERAL(".ini"))) {
    source_profile.importer_type = importer::TYPE_OPERA;
  } else {
    source_profile.importer_type = importer::TYPE_OPERA_BOOKMARK_FILE;
  }
  if (imported_items && source_profile.importer_type == importer::TYPE_OPERA) {
    StartImport(source_profile, imported_items);
  } else {
    StartImport(source_profile, importer::FAVORITES);
  }
  Release();  // Balanced in HandleChooseBookmarksFile()
}

ImportDataStartImportFunction::~ImportDataStartImportFunction() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void ImportDataStartImportFunction::StartImport(
    const importer::SourceProfile& source_profile,
    uint16 imported_items) {
  if (!imported_items)
    return;

  // AddRef(); // balanced from ImportEnded()
  // If another import is already ongoing, let it finish silently.
  if (importer_host_)
    importer_host_->set_observer(NULL);

  base::FundamentalValue importing(true);

  import_did_succeed_ = false;

  importer_host_ = new ExternalProcessImporterHost();
  importer_host_->set_observer(this);

  Profile* profile = GetProfile();

  importer_host_->StartImportSettings(source_profile, profile, imported_items,
                                      new ProfileWriter(profile));
}

void ImportDataStartImportFunction::ImportStarted() {
}

void ImportDataStartImportFunction::ImportItemStarted(
    importer::ImportItem item) {
}

void ImportDataStartImportFunction::ImportItemEnded(importer::ImportItem item) {
  import_did_succeed_ = true;
}

void ImportDataStartImportFunction::ImportEnded() {
  importer_host_->set_observer(NULL);
  importer_host_ = NULL;

  if (import_did_succeed_) {
    results_ = api::import_data::StartImport::Results::Create("Success");
  } else {
    results_ = api::import_data::StartImport::Results::Create("Failure");
  }
  // SendResponse(true);
  // Release(); // balanced from StartImport()

  SendAsyncResponse();
}

ImportDataSetVivaldiAsDefaultBrowserFunction::
    ImportDataSetVivaldiAsDefaultBrowserFunction() {
  default_browser_worker_ = new ShellIntegration::DefaultBrowserWorker(this);
}

ImportDataSetVivaldiAsDefaultBrowserFunction::
    ~ImportDataSetVivaldiAsDefaultBrowserFunction() {
  if (default_browser_worker_.get())
    default_browser_worker_->ObserverDestroyed();
}

// ShellIntegration::DefaultWebClientObserver implementation.
void ImportDataSetVivaldiAsDefaultBrowserFunction::SetDefaultWebClientUIState(
    ShellIntegration::DefaultWebClientUIState state) {
  if (state == ShellIntegration::STATE_IS_DEFAULT) {
    results_ =
        api::import_data::SetVivaldiAsDefaultBrowser::Results::Create("true");
    SendResponse(true);  // Already default
    Release();
  } else if (state == ShellIntegration::STATE_NOT_DEFAULT) {
    if (ShellIntegration::CanSetAsDefaultBrowser() ==
        ShellIntegration::SET_DEFAULT_NOT_ALLOWED) {
    } else {
      // Not default browser
      results_ = api::import_data::SetVivaldiAsDefaultBrowser::Results::Create(
          "false");
      SendResponse(true);
      Release();
    }
  } else if (state == ShellIntegration::STATE_UNKNOWN) {
  } else {
    return;  // Still processing.
  }
}

bool ImportDataSetVivaldiAsDefaultBrowserFunction::
    IsInteractiveSetDefaultPermitted() {
  return true;
}

bool ImportDataSetVivaldiAsDefaultBrowserFunction::RunAsync() {
  AddRef();  // balanced from SetDefaultWebClientUIState()
  default_browser_worker_->StartSetAsDefault();
  // SendResponse(true);
  return true;
}

ImportDataIsVivaldiDefaultBrowserFunction::
    ImportDataIsVivaldiDefaultBrowserFunction() {
  default_browser_worker_ = new ShellIntegration::DefaultBrowserWorker(this);
}

ImportDataIsVivaldiDefaultBrowserFunction::
    ~ImportDataIsVivaldiDefaultBrowserFunction() {
  if (default_browser_worker_.get())
    default_browser_worker_->ObserverDestroyed();
}

bool ImportDataIsVivaldiDefaultBrowserFunction::RunAsync() {
  AddRef();  // balanced from SetDefaultWebClientUIState()
  default_browser_worker_->StartCheckIsDefault();
  return true;
}

// ShellIntegration::DefaultWebClientObserver implementation.
void ImportDataIsVivaldiDefaultBrowserFunction::SetDefaultWebClientUIState(
    ShellIntegration::DefaultWebClientUIState state) {
  if (state == ShellIntegration::STATE_IS_DEFAULT) {
    results_ =
        api::import_data::IsVivaldiDefaultBrowser::Results::Create("true");
    SendResponse(true);  // Already default
    Release();
  } else if (state == ShellIntegration::STATE_NOT_DEFAULT) {
    if (ShellIntegration::CanSetAsDefaultBrowser() ==
        ShellIntegration::SET_DEFAULT_NOT_ALLOWED) {
    } else {
      // Not default browser
      results_ =
          api::import_data::IsVivaldiDefaultBrowser::Results::Create("false");
      SendResponse(true);
      Release();
    }
  } else if (state == ShellIntegration::STATE_UNKNOWN) {
  } else {
    return;  // Still processing.
  }
}
bool ImportDataIsVivaldiDefaultBrowserFunction::
    IsInteractiveSetDefaultPermitted() {
  return true;
}

ImportDataLaunchNetworkSettingsFunction::
    ImportDataLaunchNetworkSettingsFunction() {
}

ImportDataLaunchNetworkSettingsFunction::
    ~ImportDataLaunchNetworkSettingsFunction() {
}

bool ImportDataLaunchNetworkSettingsFunction::RunAsync() {
  options::AdvancedOptionsUtilities::ShowNetworkProxySettings(NULL);
  SendResponse(true);
  return true;
}

ImportDataSavePageFunction::ImportDataSavePageFunction() {
}

ImportDataSavePageFunction::~ImportDataSavePageFunction() {
}

bool ImportDataSavePageFunction::RunAsync() {
  Browser* browser =
      chrome::FindLastActiveWithHostDesktopType(chrome::GetActiveDesktop());

  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  current_tab->OnSavePage();
  SendResponse(true);
  return true;
}

ImportDataOpenPageFunction::ImportDataOpenPageFunction() {
}

ImportDataOpenPageFunction::~ImportDataOpenPageFunction() {
}

bool ImportDataOpenPageFunction::RunAsync() {
  Browser* browser =
      chrome::FindLastActiveWithHostDesktopType(chrome::GetActiveDesktop());

  browser->OpenFile();
  SendResponse(true);
  return true;
}

ImportDataSetVivaldiLanguageFunction::ImportDataSetVivaldiLanguageFunction() {
}

ImportDataSetVivaldiLanguageFunction::~ImportDataSetVivaldiLanguageFunction() {
}

bool ImportDataSetVivaldiLanguageFunction::RunAsync() {
  scoped_ptr<api::import_data::SetVivaldiLanguage::Params> params(
      api::import_data::SetVivaldiLanguage::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());
  std::string& language_code = params->locale;

  CHECK(!language_code.empty());

  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetString(prefs::kApplicationLocale, language_code);
  SendResponse(true);
  return true;
}

ImportDataSetDefaultContentSettingsFunction::
    ImportDataSetDefaultContentSettingsFunction() {
}

ImportDataSetDefaultContentSettingsFunction::
    ~ImportDataSetDefaultContentSettingsFunction() {
}

bool ImportDataSetDefaultContentSettingsFunction::RunAsync() {
  scoped_ptr<api::import_data::SetDefaultContentSettings::Params> params(
      api::import_data::SetDefaultContentSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string& content_settings = params->content_setting;
  std::string& value = params->value;

  ContentSetting default_setting = vivContentSettingFromString(value);
  //
  ContentSettingsType content_type =
      vivContentSettingsTypeFromGroupName(content_settings);

  Profile* profile = GetProfile();

  HostContentSettingsMap* map = profile->GetHostContentSettingsMap();
  map->SetDefaultContentSetting(content_type, default_setting);

  results_ =
      api::import_data::SetDefaultContentSettings::Results::Create("success");
  SendResponse(true);
  return true;
}

ImportDataGetDefaultContentSettingsFunction::
    ImportDataGetDefaultContentSettingsFunction() {
}

ImportDataGetDefaultContentSettingsFunction::
    ~ImportDataGetDefaultContentSettingsFunction() {
}

bool ImportDataGetDefaultContentSettingsFunction::RunAsync() {
  scoped_ptr<api::import_data::GetDefaultContentSettings::Params> params(
      api::import_data::GetDefaultContentSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  std::string& content_settings = params->content_setting;
  std::string def_provider = "";
  ContentSettingsType content_type =
      vivContentSettingsTypeFromGroupName(content_settings);
  ContentSetting default_setting;
  Profile* profile = GetProfile();

  default_setting =
      profile->GetHostContentSettingsMap()->GetDefaultContentSetting(
          content_type, &def_provider);

  std::string setting = vivContentSettingToString(default_setting);

  results_ =
      api::import_data::GetDefaultContentSettings::Results::Create(setting);
  SendResponse(true);
  return true;
}

ImportDataSetBlockThirdPartyCookiesFunction::
    ImportDataSetBlockThirdPartyCookiesFunction() {
}

ImportDataSetBlockThirdPartyCookiesFunction::
    ~ImportDataSetBlockThirdPartyCookiesFunction() {
}

bool ImportDataSetBlockThirdPartyCookiesFunction::RunAsync() {
  scoped_ptr<api::import_data::SetBlockThirdPartyCookies::Params> params(
      api::import_data::SetBlockThirdPartyCookies::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());
  bool block3rdparty = params->block3rd_party;

  PrefService* pref_service = GetProfile()->GetPrefs();
  pref_service->SetBoolean(prefs::kBlockThirdPartyCookies, block3rdparty);

  PrefService* service = GetProfile()->GetPrefs();
  bool blockThirdParty = service->GetBoolean(prefs::kBlockThirdPartyCookies);

  results_ = api::import_data::SetBlockThirdPartyCookies::Results::Create(
      blockThirdParty);
  SendResponse(true);
  return true;
}

ImportDataGetBlockThirdPartyCookiesFunction::
    ImportDataGetBlockThirdPartyCookiesFunction() {
}

ImportDataGetBlockThirdPartyCookiesFunction::
    ~ImportDataGetBlockThirdPartyCookiesFunction() {
}

bool ImportDataGetBlockThirdPartyCookiesFunction::RunAsync() {
  PrefService* service = GetProfile()->GetPrefs();
  bool blockThirdParty = service->GetBoolean(prefs::kBlockThirdPartyCookies);

  results_ = api::import_data::GetBlockThirdPartyCookies::Results::Create(
      blockThirdParty);
  SendResponse(true);
  return true;
}

ImportDataOpenTaskManagerFunction::~ImportDataOpenTaskManagerFunction() {
}

bool ImportDataOpenTaskManagerFunction::RunAsync() {
  Browser* browser =
      chrome::FindLastActiveWithHostDesktopType(chrome::GetActiveDesktop());

  chrome::OpenTaskManager(browser);

  SendResponse(true);
  return true;
}

ImportDataOpenTaskManagerFunction::ImportDataOpenTaskManagerFunction() {
}

ImportDataShowDevToolsFunction::ImportDataShowDevToolsFunction() {
}

ImportDataShowDevToolsFunction::~ImportDataShowDevToolsFunction() {
}

bool ImportDataShowDevToolsFunction::RunAsync() {
  Browser* browser =
      chrome::FindLastActiveWithHostDesktopType(chrome::GetActiveDesktop());

  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  DevToolsWindow::InspectElement(current_tab, 0, 0);

  SendResponse(true);
  return true;
}

ImportDataGetStartupActionFunction::ImportDataGetStartupActionFunction() {
}

ImportDataGetStartupActionFunction::~ImportDataGetStartupActionFunction() {
}

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
  results_ = api::import_data::GetStartupAction::Results::Create(startupRes);

  SendResponse(true);
  return true;
}

ImportDataSetStartupActionFunction::ImportDataSetStartupActionFunction(){
}

ImportDataSetStartupActionFunction::~ImportDataSetStartupActionFunction(){
}

bool ImportDataSetStartupActionFunction::RunAsync() {
  scoped_ptr<api::import_data::SetStartupAction::Params> params(
      api::import_data::SetStartupAction::Params::Create(*args_));
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

  Profile* profile = GetProfile();
  PrefService* prefs = profile->GetPrefs();

  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  results_ =
      api::import_data::GetStartupAction::Results::Create(content_settings);

  SendResponse(true);
  return true;
}

}  // namespace extensions
