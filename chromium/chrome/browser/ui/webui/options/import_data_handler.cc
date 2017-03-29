// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/import_data_handler.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_uma.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"

#include "content/public/browser/web_contents.h"
#include "content/browser/web_contents/web_contents_view.h"

using content::BrowserThread;

namespace options {

ImportDataHandler::ImportDataHandler()
    : importer_host_(NULL),
      import_did_succeed_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ImportDataHandler::~ImportDataHandler() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (importer_host_)
    importer_host_->set_observer(NULL);

  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void ImportDataHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    {"importFromLabel", IDS_IMPORT_FROM_LABEL},
    {"importLoading", IDS_IMPORT_LOADING_PROFILES},
    {"importDescription", IDS_IMPORT_ITEMS_LABEL},
    {"importHistory", IDS_IMPORT_HISTORY_CHKBOX},
    {"importFavorites", IDS_IMPORT_FAVORITES_CHKBOX},
    {"importSearch", IDS_IMPORT_SEARCH_ENGINES_CHKBOX},
    {"importPasswords", IDS_IMPORT_PASSWORDS_CHKBOX},
    {"importAutofillFormData", IDS_IMPORT_AUTOFILL_FORM_DATA_CHKBOX},
    {"importChooseFile", IDS_IMPORT_CHOOSE_FILE},
	{"importNotes", IDS_IMPORT_NOTES_CHKBOX},
    {"importCommit", IDS_IMPORT_COMMIT},
    {"noProfileFound", IDS_IMPORT_NO_PROFILE_FOUND},
    {"importSucceeded", IDS_IMPORT_SUCCEEDED},
    {"findYourImportedBookmarks", IDS_IMPORT_FIND_YOUR_BOOKMARKS},
#if defined(OS_MACOSX)
    {"macPasswordKeychain", IDS_IMPORT_PASSWORD_KEYCHAIN_WARNING},
#endif
	{ "useOperaDefaultLocation", IDS_USE_OPERA_DEFAULT_LOCATION },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "importDataOverlay",
                IDS_IMPORT_SETTINGS_TITLE);
}

void ImportDataHandler::InitializeHandler() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  importer_list_.reset(new ImporterList());
  importer_list_->DetectSourceProfiles(
      g_browser_process->GetApplicationLocale(),
      true,  // include_interactive_profiles?
      base::Bind(&ImportDataHandler::InitializePage, base::Unretained(this)));
}

void ImportDataHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "importData",
      base::Bind(&ImportDataHandler::ImportData, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "chooseBookmarksFile",
      base::Bind(&ImportDataHandler::HandleChooseBookmarksFile,
                 base::Unretained(this)));
}

void ImportDataHandler::StartImport(
    const importer::SourceProfile& source_profile,
    int16 imported_items) {
  importer::ImportConfig import_config;

  import_config.imported_items = imported_items;

  StartImport(source_profile, import_config);
}

void ImportDataHandler::StartImport(
    const importer::SourceProfile& source_profile,
    const importer::ImportConfig &imported_items) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!imported_items.imported_items)
    return;

  // If another import is already ongoing, let it finish silently.
  if (importer_host_)
    importer_host_->set_observer(NULL);

  base::FundamentalValue importing(true);
  web_ui()->CallJavascriptFunction("ImportDataOverlay.setImportingState",
                                   importing);
  import_did_succeed_ = false;

  importer_host_ = new ExternalProcessImporterHost();
  importer_host_->set_observer(this);
  Profile* profile = Profile::FromWebUI(web_ui());
  importer_host_->StartImportSettings(source_profile, profile,
                                      imported_items,
                                      new ProfileWriter(profile));

  importer::LogImporterUseToMetrics("ImportDataHandler",
                                    source_profile.importer_type);
}

void ImportDataHandler::ImportData(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string string_value;

  int browser_index;
  if (!args->GetString(0, &string_value) ||
      !base::StringToInt(string_value, &browser_index)) {
    NOTREACHED();
    return;
  }

   const importer::SourceProfile& source_profile =
      importer_list_->GetSourceProfileAt(browser_index);
  uint16 supported_items = source_profile.services_supported;
  importer::ImportConfig import_config;

  uint16 selected_items = importer::NONE;
  if (args->GetString(1, &string_value) && string_value == "true") {
    selected_items |= importer::HISTORY;
  }
  if (args->GetString(2, &string_value) && string_value == "true") {
    selected_items |= importer::FAVORITES;
  }
  if (args->GetString(3, &string_value) && string_value == "true") {
    selected_items |= importer::PASSWORDS;
  }
  if (args->GetString(4, &string_value) && string_value == "true") {
    selected_items |= importer::SEARCH_ENGINES;
  }
  if (args->GetString(5, &string_value) && string_value == "true") {
    selected_items |= importer::AUTOFILL_FORM_DATA;
  }
  if (args->GetString(6, &string_value) && string_value == "true") {
    selected_items |= importer::NOTES;
  }
  
  if (source_profile.importer_type==  importer::TYPE_OPERA){
    if (args->GetString(7, &string_value) && string_value == "false") {
		base::ListValue* args = new base::ListValue;
		
		//Set browser index to 6. (Browse for Opera ini file)
		args->Append( new base::StringValue("6"));

		HandleChooseBookmarksFile(args);
		return;
    }

    if ((selected_items & importer::PASSWORDS) != 0 &&
      (supported_items & importer::MASTER_PASSWORD) != 0 &&
      args->GetString(7, &string_value) && 
      !string_value.empty())
    {
      // Master password
      import_config.arguments.push_back(base::UTF8ToUTF16(string_value));
    }
  }

  uint16 imported_items = (selected_items & supported_items);
  if (imported_items) {
    StartImport(source_profile, imported_items);
  } else {
    LOG(WARNING) << "There were no settings to import from '"
        << source_profile.importer_name << "'.";
  }
}

void ImportDataHandler::InitializePage() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  bool operaProfileFound = false;

  base::ListValue browser_profiles;
  for (size_t i = 0; i < importer_list_->count(); ++i) {
    const importer::SourceProfile& source_profile =
        importer_list_->GetSourceProfileAt(i);
    uint16 browser_services = source_profile.services_supported;

    base::DictionaryValue* browser_profile = new base::DictionaryValue();
    browser_profile->SetString("name", source_profile.importer_name);
    browser_profile->SetInteger("index", i);
    browser_profile->SetBoolean("history",
        (browser_services & importer::HISTORY) != 0);
    browser_profile->SetBoolean("favorites",
        (browser_services & importer::FAVORITES) != 0);
    browser_profile->SetBoolean("passwords",
        (browser_services & importer::PASSWORDS) != 0);
    browser_profile->SetBoolean("search",
        (browser_services & importer::SEARCH_ENGINES) != 0);
    browser_profile->SetBoolean("autofill-form-data",
        (browser_services & importer::AUTOFILL_FORM_DATA) != 0);
    browser_profile->SetBoolean("notes",
        (browser_services & importer::NOTES) != 0);

    browser_profile->SetBoolean("show_bottom_bar",
#if defined(OS_MACOSX)
        source_profile.importer_type == importer::TYPE_SAFARI);
#else
        false);
#endif

    browser_profiles.Append(browser_profile);

	if(source_profile.importer_type==  importer::TYPE_OPERA)
	{	
#if defined(OS_WIN)
		if(base::EndsWith(source_profile.source_path.value(), L"Opera", true))
#else
		if(base::EndsWith(source_profile.source_path.value(), "Opera", true))
#endif
		{
			operaProfileFound  = true;
		}
	}
  }

  web_ui()->CallJavascriptFunction("ImportDataOverlay.updateSupportedBrowsers",
                                   browser_profiles);
    
  web_ui()->CallJavascriptFunction("ImportDataOverlay.operaProfile", base::FundamentalValue(operaProfileFound));


}

void ImportDataHandler::ImportStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ImportDataHandler::ImportItemStarted(importer::ImportItem item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(csilv): show progress detail in the web view.
}

void ImportDataHandler::ImportItemEnded(importer::ImportItem item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(csilv): show progress detail in the web view.
  import_did_succeed_ = true;
}

void ImportDataHandler::ImportEnded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  importer_host_->set_observer(NULL);
  importer_host_ = NULL;

  if (import_did_succeed_) {
    web_ui()->CallJavascriptFunction("ImportDataOverlay.confirmSuccess");
  } else {
    base::FundamentalValue state(false);
    web_ui()->CallJavascriptFunction("ImportDataOverlay.setImportingState",
                                     state);
    web_ui()->CallJavascriptFunction("ImportDataOverlay.dismiss");
  }
}

void ImportDataHandler::FileSelected(const base::FilePath& path,
                                     int /*index*/,
                                     void* /*params*/) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  importer::SourceProfile source_profile;
  source_profile.source_path = path;

  if (path.MatchesExtension(FILE_PATH_LITERAL(".html")))
  {
    source_profile.importer_type = importer::TYPE_BOOKMARKS_FILE;
  }
  if (path.MatchesExtension(FILE_PATH_LITERAL(".ini")))
  {
    source_profile.importer_type = importer::TYPE_OPERA;
  }
  else
  {
    source_profile.importer_type = importer::TYPE_OPERA_BOOKMARK_FILE;
  }
  StartImport(source_profile, importer::FAVORITES);
}

void ImportDataHandler::HandleChooseBookmarksFile(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string string_value;

  int browser_index;
  if (!args->GetString(0, &string_value) || !base::StringToInt(string_value, &browser_index)) {
    NOTREACHED();
    return;
  }
  
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);

  if(browser_index == 4)
	file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("adr"));
  else if (browser_index == 6)
	  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("ini"));
  else
	file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("html"));

  /*Arnar@vivaldi. Currently no browser object exists for vivaldi.
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());*/

  gfx::NativeWindow window =
      (web_ui()->GetWebContents()->GetTopLevelNativeWindow());



  select_file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                  base::string16(),
                                  base::FilePath(),
                                  &file_type_info,
                                  0,
                                  base::FilePath::StringType(),
                                  window,
                                  NULL);
}

}  // namespace options
