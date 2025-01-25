// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "importer/chromium_importer.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/ini_parser.h"
#include "chrome/grit/branded_strings.h"
#include "chromium/components/sessions/core/session_types.h"
#include "components/os_crypt/sync/key_storage_config_linux.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_switches.h"
#include "importer/chrome_importer_utils.h"
#include "importer/imported_tab_entry.h"
#include "sql/statement.h"
#include "ui/base/l10n/l10n_util.h"

#include "importer/chromium_extension_importer.h"
#include "importer/chromium_session_importer.h"

ChromiumImporter::ChromiumImporter() {}

ChromiumImporter::~ChromiumImporter() {}

void ChromiumImporter::StartImport(
    const importer::SourceProfile& source_profile,
    uint16_t items,
    ImporterBridge* bridge) {
  bridge_ = bridge;
  std::string name = source_profile.selected_profile_name;
  profile_dir_ = source_profile.source_path.AppendASCII(name);

  bridge_->NotifyStarted();

  if ((items & importer::HISTORY) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::HISTORY);
    ImportHistory();
    bridge_->NotifyItemEnded(importer::HISTORY);
  }

  if ((items & importer::FAVORITES) && !cancelled()) {
    base::FilePath bookmarkFilePath = profile_dir_.AppendASCII("Bookmarks");

    bookmarkfilename_ = bookmarkFilePath.value();
    if (!base::PathExists(bookmarkFilePath)) {
      // Just notify about start and end if the file doesn't exist, otherwise
      // the end of import detection won't work.
      bridge_->NotifyItemStarted(importer::FAVORITES);
      bridge_->NotifyItemEnded(importer::FAVORITES);
    } else {
      bridge_->NotifyItemStarted(importer::FAVORITES);
      ImportBookMarks();
      bridge_->NotifyItemEnded(importer::FAVORITES);
    }
  }

  if ((items & importer::PASSWORDS) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::PASSWORDS);
    ImportPasswords(source_profile.importer_type);
    bridge_->NotifyItemEnded(importer::PASSWORDS);
  }

  if ((items & importer::TABS) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::TABS);
    ImportTabs(source_profile.importer_type);
    bridge_->NotifyItemEnded(importer::TABS);
  }

  if ((items & importer::EXTENSIONS) && !cancelled()) {
    ImportExtensions();
  } else {
    // When importing extensions Vivaldi ProfileWriter
    // is responsible for reprorting that import has finished.
    bridge_->NotifyEnded();
  }
}

#if BUILDFLAG(IS_WIN)
std::string import_encryption_key;
#endif  // IS_WIN
void ChromiumImporter::ImportPasswords(importer::ImporterType importer_type) {
  // Initializes Chrome decryptor

  std::vector<importer::ImportedPasswordForm> forms;
  base::FilePath source_path = profile_dir_;

#if BUILDFLAG(IS_WIN)
  // Read encryption key from other browser local state
  base::FilePath local_state_file =
      profile_dir_.DirName().AppendASCII("Local State");
  if (!base::PathExists(local_state_file)) {
    LOG(ERROR) << "Unable to find Local State for import browser.";
    return;
  }

  std::string local_state_string;
  if (!ReadFileToString(local_state_file, &local_state_string)) {
    LOG(ERROR) << "Unable to read Local State from disk.";
    return;
  }

  std::optional<base::Value> local_state(
      base::JSONReader::Read(local_state_string));
  if (!local_state) {
    LOG(ERROR) << "Unable to parse JSON in Local State.";
    return;
  }

  if (local_state->is_dict()) {
    base::Value* os_crypt_dict = local_state->GetDict().Find("os_crypt");
    if (!os_crypt_dict) {
      LOG(ERROR) << "Unable to find 'os_cypt' entry for import browser.";
      return;
    }

    const std::string* base64_encoded_key =
        os_crypt_dict->GetDict().FindString("encrypted_key");
    if (!base64_encoded_key) {
      LOG(ERROR) << "Unable to find 'encrypted_key' entry for import browser.";
      return;
    }

    std::string encrypted_key_with_header;
    base::Base64Decode(*base64_encoded_key, &encrypted_key_with_header);

    // Key prefix for a key encrypted with DPAPI.
    const char kDPAPIKeyPrefix[] = "DPAPI";
    if (!base::StartsWith(encrypted_key_with_header, kDPAPIKeyPrefix,
                          base::CompareCase::SENSITIVE)) {
      LOG(ERROR) << "Key is not DPAPI key, unable to decrypt.";
      return;
    }

    std::string dpapi_encrypted_key =
        encrypted_key_with_header.substr(sizeof(kDPAPIKeyPrefix) - 1);

    // This DPAPI decryption can fail if the user's password has been reset
    // by an Administrator.
    if (!OSCrypt::DecryptString(dpapi_encrypted_key, &import_encryption_key)) {
      LOG(ERROR) << "Decryption key invalid.";
      return;
    }
  }
#endif  // IS_WIN

  base::FilePath file = source_path.AppendASCII("Login Data");
  if (base::PathExists(file)) {
    ReadAndParseSignons(file, &forms, importer_type);
  }
  if (!cancelled()) {
    for (size_t i = 0; i < forms.size(); ++i) {
      if (!forms[i].username_value.empty() ||
          !forms[i].password_value.empty()) {
        bridge_->SetPasswordForm(forms[i]);
      }
    }
  }
}

bool ChromiumImporter::ReadAndParseSignons(
    const base::FilePath& sqlite_file,
    std::vector<importer::ImportedPasswordForm>* forms,
    importer::ImporterType importer_type) {
  sql::Database db;
  if (!db.Open(sqlite_file))
    return false;

  const char query2[] =
      "SELECT origin_url, action_url, username_element, username_value, "
      "password_element, password_value, signon_realm "
      "FROM logins";

  sql::Statement s2(db.GetUniqueStatement(query2));
  if (!s2.is_valid())
    return false;

  while (s2.Step()) {
    importer::ImportedPasswordForm form;
    form.url = GURL(s2.ColumnString(0));
    form.action = GURL(s2.ColumnString(1));
    form.username_element = base::UTF8ToUTF16(s2.ColumnString(2));
    form.username_value = base::UTF8ToUTF16(s2.ColumnString(3));
    form.password_element = base::UTF8ToUTF16(s2.ColumnString(4));
    const std::string& cipher_text = s2.ColumnString(5);
    std::u16string plain_text;

#if BUILDFLAG(IS_MAC)
    std::string service_name = "Chrome Safe Storage";
    std::string account_name = "Chrome";
    if (importer_type == importer::TYPE_BRAVE) {
      service_name = "Brave Safe Storage";
      account_name = "Brave";
    }
    if (importer_type == importer::TYPE_EDGE_CHROMIUM) {
      service_name = "Microsoft Edge Safe Storage";
      account_name = "Microsoft Edge";
    }
    if (importer_type == importer::TYPE_OPERA_OPIUM) {
      service_name = "Opera Safe Storage";
      account_name = "Opera";
    }
    if (importer_type == importer::TYPE_VIVALDI) {
      service_name = "Vivaldi Safe Storage";
      account_name = "Vivaldi";
    }
    if (importer_type == importer::TYPE_YANDEX) {
      service_name = "Yandex Safe Storage";
      account_name = "Yandex";
    }
    if (importer_type == importer::TYPE_CHROMIUM) {
      service_name = "Chromium Safe Storage";
      account_name = "Chromium";
    }
    if (importer_type == importer::TYPE_ARC) {
      service_name = "Arc Safe Storage";
      account_name = "Arc";
    }
    if (importer_type == importer::TYPE_OPERA_GX) {
      service_name = "Opera GX Safe Storage";
      account_name = "Opera GX";
    }
    OSCryptImpl::GetInstance()->DecryptImportedString16(cipher_text, &plain_text, service_name,
                                     account_name);
#else
#if BUILDFLAG(IS_LINUX)
    // Set up crypt config.
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    std::unique_ptr<os_crypt::Config> config(new os_crypt::Config());
    config->store =
        command_line.GetSwitchValueASCII(password_manager::kPasswordStore);
    config->product_name = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
    config->should_use_preference =
        command_line.HasSwitch(password_manager::kEnableEncryptionSelection);
    chrome::GetDefaultUserDataDirectory(&config->user_data_path);
    OSCryptImpl::GetInstance()->SetConfig(std::move(config));
    OSCryptImpl::GetInstance()->DecryptString16(cipher_text, &plain_text);
#endif  // IS_LINUX
#if BUILDFLAG(IS_WIN)
    OSCryptImpl::GetInstance()->DecryptImportedString16(cipher_text, &plain_text,
                                     import_encryption_key);
#endif  // IS_WIN
#endif

    form.password_value = plain_text;
    form.signon_realm = s2.ColumnString(6);

    forms->push_back(form);
  }
#if BUILDFLAG(IS_MAC)
  OSCryptImpl::GetInstance()->ResetImportCache();
#endif

  return true;
}

void ChromiumImporter::ImportHistory() {
  std::vector<ImporterURLRow> historyRows;
  base::FilePath source_path = profile_dir_;

  base::FilePath file = source_path.AppendASCII("History");
  if (base::PathExists(file)) {
    ReadAndParseHistory(file, &historyRows);
  }

  if (!historyRows.empty() && !cancelled())
    bridge_->SetHistoryItems(historyRows,
                             importer::VISIT_SOURCE_CHROMIUM_IMPORTED);
}

bool ChromiumImporter::ReadAndParseHistory(const base::FilePath& sqlite_file,
                                           std::vector<ImporterURLRow>* forms) {
  sql::Database db;
  if (!db.Open(sqlite_file))
    return false;

  const char query2[] =
      "SELECT url, title, visit_count, hidden, typed_count, case when "
      "last_visit_time = 0 then 1 else last_visit_time end as last_visit_time "
      "FROM urls";

  sql::Statement s2(db.GetUniqueStatement(query2));
  if (!s2.is_valid())
    return false;

  while (s2.Step()) {
    ImporterURLRow row(GURL(s2.ColumnString(0)));
    row.title = s2.ColumnString16(1);
    row.visit_count = s2.ColumnInt(2);
    row.hidden = s2.ColumnInt(3) == 1;
    row.typed_count = s2.ColumnInt(4);

    base::Time t = base::Time::FromInternalValue(s2.ColumnInt64(5));
    row.last_visit = t;
    forms->push_back(row);
  }
  return true;
}

void ChromiumImporter::ImportExtensions() {
  bridge_->NotifyItemStarted(importer::EXTENSIONS);

  const auto extensions =
      extension_importer::ChromiumExtensionsImporter::GetImportableExtensions(
          profile_dir_);
  if (!extensions.empty() && !cancelled()) {
    bridge_->AddExtensions(extensions);
  } else {
    bridge_->NotifyItemEnded(importer::EXTENSIONS);
    bridge_->NotifyEnded();
  }
}

void ChromiumImporter::ImportTabs(importer::ImporterType importer_type) {
  const auto tabs =
      session_importer::ChromiumSessionImporter::GetOpenTabs(
          profile_dir_, importer_type);

  std::vector<ImportedTabEntry> imported_tabs;

  std::transform(tabs.begin(), tabs.end(), std::back_inserter(imported_tabs),
                 [](const auto& it) {
                   return ImportedTabEntry::FromSessionTab(*it.second);
                 });

  bridge_->AddOpenTabs(std::move(imported_tabs));
}
