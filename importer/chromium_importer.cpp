// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "importer/chromium_importer.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/importer/importer_bridge.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "importer/imported_tab_entry.h"
#include "sql/statement.h"

#include "app/vivaldi_resources.h"
#include "components/sessions/vivaldi_session_service_commands.h"
#include "importer/chromium_extension_importer.h"
#include "importer/chromium_session_importer.h"
#include "importer/import_error_utils.h"

#if BUILDFLAG(IS_LINUX)
#include "chrome/grit/branded_strings.h"
#include "components/os_crypt/sync/key_storage_config_linux.h"
#include "components/password_manager/core/browser/password_manager_switches.h"
#endif  // IS_LINUX

namespace {
#if BUILDFLAG(IS_WIN)
std::string* GetImportEncryptionKey() {
  static base::NoDestructor<std::string> import_encryption_key;
  return import_encryption_key.get();
}
#endif
}  // namespace

ChromiumImporter::ChromiumImporter() {}

ChromiumImporter::~ChromiumImporter() {}

void ChromiumImporter::StartImport(
    const user_data_importer::SourceProfile& source_profile,
    uint16_t items,
    ImporterBridge* bridge) {
  bridge_ = bridge;
  std::string name = source_profile.selected_profile_name;
  if (name.empty()) {
    name = "Default";  // Fallback to Default profile if no profile specified
  }
  profile_dir_ = source_profile.source_path.AppendASCII(name);

  bridge_->NotifyStarted();

  if ((items & user_data_importer::HISTORY) && !cancelled()) {
    bridge_->NotifyItemStarted(user_data_importer::HISTORY);
    auto res = ImportHistory();
    import_result::NotifyBridge(res, bridge_.get(),
                                user_data_importer::HISTORY);
  }

  if ((items & user_data_importer::FAVORITES) && !cancelled()) {
    base::FilePath bookmarkFilePath = profile_dir_.AppendASCII("Bookmarks");

    bookmarkfilename_ = bookmarkFilePath.value();
    if (!base::PathExists(bookmarkFilePath)) {
      // Just notify about start and end if the file doesn't exist, otherwise
      // the end of import detection won't work.
      bridge_->NotifyItemStarted(user_data_importer::FAVORITES);
      bridge_->NotifyItemEnded(user_data_importer::FAVORITES);
    } else {
      bridge_->NotifyItemStarted(user_data_importer::FAVORITES);
      const auto res = ImportBookMarks();
      import_result::NotifyBridge(res, bridge_.get(),
                                  user_data_importer::FAVORITES);
    }
  }

  if ((items & user_data_importer::PASSWORDS) && !cancelled()) {
    bridge_->NotifyItemStarted(user_data_importer::PASSWORDS);

    const auto res = ImportPasswords(source_profile.importer_type);
    import_result::NotifyBridge(res, bridge_.get(),
                                user_data_importer::PASSWORDS);
  }

  if ((items & user_data_importer::TABS) && !cancelled()) {
    bridge_->NotifyItemStarted(user_data_importer::TABS);
    const auto res = ImportTabs(source_profile.importer_type);
    import_result::NotifyBridge(res, bridge_.get(), user_data_importer::TABS);
  }

  if ((items & user_data_importer::EXTENSIONS) && !cancelled()) {
    bridge_->NotifyItemStarted(user_data_importer::EXTENSIONS);
    const auto res = ImportExtensions();
    import_result::NotifyBridge(res, bridge_.get(),
                                user_data_importer::EXTENSIONS);
  }
  bridge_->NotifyEnded();
}

ImportResult ChromiumImporter::ImportPasswords(
    user_data_importer::ImporterType importer_type) {
  // Initializes Chrome decryptor

  std::vector<user_data_importer::ImportedPasswordForm> forms;
  base::FilePath source_path = profile_dir_;

#if BUILDFLAG(IS_WIN)
  // Read encryption key from other browser local state
  base::FilePath local_state_file =
      profile_dir_.DirName().AppendASCII("Local State");

  base::Value local_state;
  auto result = ImportFileOperations::ParseJsonFile(
      local_state_file, &local_state, IDS_IMPORT_ERROR_LOCAL_STATE_NOT_FOUND,
      IDS_IMPORT_ERROR_LOCAL_STATE_READ_FAILED,
      IDS_IMPORT_ERROR_LOCAL_STATE_PARSE_FAILED);
  if (!result.has_value()) {
    return result;
  }

  if (local_state.is_dict()) {
    base::Value* os_crypt_dict = local_state.GetDict().Find("os_crypt");
    if (!os_crypt_dict) {
      return import_result::Error(IDS_IMPORT_ERROR_OS_CRYPT_NOT_FOUND);
    }

    const std::string* base64_encoded_key =
        os_crypt_dict->GetDict().FindString("encrypted_key");
    if (!base64_encoded_key) {
      return import_result::Error(IDS_IMPORT_ERROR_ENCRYPTED_KEY_NOT_FOUND);
    }

    std::string encrypted_key_with_header;
    base::Base64Decode(*base64_encoded_key, &encrypted_key_with_header);

    // Key prefix for a key encrypted with DPAPI.
    const char kDPAPIKeyPrefix[] = "DPAPI";
    if (!base::StartsWith(encrypted_key_with_header, kDPAPIKeyPrefix,
                          base::CompareCase::SENSITIVE)) {
      return import_result::Error(IDS_IMPORT_ERROR_NOT_DPAPI_KEY);
    }

    std::string dpapi_encrypted_key =
        encrypted_key_with_header.substr(sizeof(kDPAPIKeyPrefix) - 1);

    // This DPAPI decryption can fail if the user's password has been reset
    // by an Administrator.
    if (!OSCrypt::DecryptString(dpapi_encrypted_key,
                                GetImportEncryptionKey())) {
      return import_result::Error(IDS_IMPORT_ERROR_DECRYPTION_KEY_INVALID);
    }
  }
#endif  // IS_WIN

  base::FilePath file = source_path.AppendASCII("Login Data");
  auto file_check = ImportFileOperations::CheckFileExists(
      file, IDS_IMPORT_ERROR_LOGIN_DATA_NOT_FOUND);
  if (!file_check.has_value()) {
    return file_check;
  }

  bool failed_decrypt = false;

  const auto read_result =
      ReadAndParseSignons(file, &forms, importer_type, &failed_decrypt);
  if (!read_result.has_value()) {
    return read_result;
  }

  if (!cancelled()) {
    for (size_t i = 0; i < forms.size(); ++i) {
      if (!forms[i].username_value.empty() ||
          !forms[i].password_value.empty()) {
        bridge_->SetPasswordForm(forms[i]);
      }
    }
  }

  if (failed_decrypt) {
    return import_result::Error(IDS_IMPORT_ERROR_PASSWORD_DECRYPTION_FAILED);
  }

  return import_result::Success();
}

ImportResult ChromiumImporter::ReadAndParseSignons(
    const base::FilePath& sqlite_file,
    std::vector<user_data_importer::ImportedPasswordForm>* forms,
    user_data_importer::ImporterType importer_type,
    bool* failed_decrypt) {
  sql::Database db("Importer");
  auto db_result = ImportDatabaseOperations::OpenDatabase(
      sqlite_file, &db, IDS_IMPORT_ERROR_LOGIN_DATABASE_OPEN_FAILED);
  if (!db_result.has_value()) {
    return db_result;
  }

  const std::string query =
      "SELECT origin_url, action_url, username_element, username_value, "
      "password_element, password_value, signon_realm "
      "FROM logins";

  sql::Statement s2{db.GetUniqueStatement(query)};
  if (!s2.is_valid()) {
    return import_result::Error(IDS_IMPORT_ERROR_LOGIN_DATABASE_READ_FAILED);
  }

  *failed_decrypt = false;

  while (s2.Step()) {
    user_data_importer::ImportedPasswordForm form;
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
    if (importer_type == user_data_importer::TYPE_BRAVE) {
      service_name = "Brave Safe Storage";
      account_name = "Brave";
    }
    if (importer_type == user_data_importer::TYPE_EDGE_CHROMIUM) {
      service_name = "Microsoft Edge Safe Storage";
      account_name = "Microsoft Edge";
    }
    if (importer_type == user_data_importer::TYPE_OPERA_OPIUM) {
      service_name = "Opera Safe Storage";
      account_name = "Opera";
    }
    if (importer_type == user_data_importer::TYPE_VIVALDI) {
      service_name = "Vivaldi Safe Storage";
      account_name = "Vivaldi";
    }
    if (importer_type == user_data_importer::TYPE_YANDEX) {
      service_name = "Yandex Safe Storage";
      account_name = "Yandex";
    }
    if (importer_type == user_data_importer::TYPE_CHROMIUM) {
      service_name = "Chromium Safe Storage";
      account_name = "Chromium";
    }
    if (importer_type == user_data_importer::TYPE_ARC) {
      service_name = "Arc Safe Storage";
      account_name = "Arc";
    }
    if (importer_type == user_data_importer::TYPE_OPERA_GX) {
      service_name = "Opera GX Safe Storage";
      account_name = "Opera GX";
    }

    if (!OSCryptImpl::GetInstance()->DecryptImportedString16(
            cipher_text, &plain_text, service_name, account_name)) {
      *failed_decrypt = true;
      continue;
    }
#else  // IS_MAC
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
    if (!OSCryptImpl::GetInstance()->DecryptString16(cipher_text,
                                                     &plain_text)) {
      *failed_decrypt = true;
      continue;
    }
#endif  // IS_LINUX
#if BUILDFLAG(IS_WIN)
    if (!OSCryptImpl::GetInstance()->DecryptImportedString16(
            cipher_text, &plain_text, *GetImportEncryptionKey())) {
      *failed_decrypt = true;
      continue;
    }
#endif  // IS_WIN
#endif  // !IS_MAC

    form.password_value = plain_text;
    form.signon_realm = s2.ColumnString(6);

    forms->push_back(form);

    // Clear the plaintext password from memory
    std::fill(plain_text.begin(), plain_text.end(), u'\0');
  }
#if BUILDFLAG(IS_MAC)
  OSCryptImpl::GetInstance()->ResetImportCache();
#endif

#if BUILDFLAG(IS_WIN)
  // Clear the encryption key from memory
  std::fill(GetImportEncryptionKey()->begin(), GetImportEncryptionKey()->end(),
            '\0');
#endif

  return import_result::Success();
}

ImportResult ChromiumImporter::ImportHistory() {
  std::vector<user_data_importer::ImporterURLRow> historyRows;
  base::FilePath source_path = profile_dir_;

  base::FilePath file = source_path.AppendASCII("History");
  auto file_check = ImportFileOperations::CheckFileExists(
      file, IDS_IMPORT_ERROR_HISTORY_FILE_NOT_FOUND);
  if (!file_check.has_value()) {
    return file_check;
  }

  auto res = ReadAndParseHistory(file, &historyRows);
  if (!res.has_value()) {
    return res;
  }

  if (!historyRows.empty() && !cancelled()) {
    bridge_->SetHistoryItems(
        historyRows, user_data_importer::VISIT_SOURCE_CHROMIUM_IMPORTED);
  }

  return import_result::Success();
}

ImportResult ChromiumImporter::ReadAndParseHistory(
    const base::FilePath& sqlite_file,
    std::vector<user_data_importer::ImporterURLRow>* forms) {
  sql::Database db("Importer");
  auto db_result = ImportDatabaseOperations::OpenDatabase(
      sqlite_file, &db, IDS_IMPORT_ERROR_HISTORY_DATABASE_OPEN_FAILED);
  if (!db_result.has_value()) {
    return db_result;
  }

  const std::string query =
      "SELECT url, title, visit_count, hidden, typed_count, case when "
      "last_visit_time = 0 then 1 else last_visit_time end as last_visit_time "
      "FROM urls";

  sql::Statement s2{db.GetUniqueStatement(query)};
  if (!s2.is_valid()) {
    return import_result::Error(IDS_IMPORT_ERROR_HISTORY_DATABASE_QUERY_FAILED);
  }

  while (s2.Step()) {
    user_data_importer::ImporterURLRow row(GURL(s2.ColumnString(0)));
    row.title = s2.ColumnString16(1);
    row.visit_count = s2.ColumnInt(2);
    row.hidden = s2.ColumnInt(3) == 1;
    row.typed_count = s2.ColumnInt(4);

    base::Time t = base::Time::FromInternalValue(s2.ColumnInt64(5));
    row.last_visit = t;
    forms->push_back(row);
  }
  return import_result::Success();
}

ImportResult ChromiumImporter::ImportExtensions() {
  const auto extensions =
      extension_importer::ChromiumExtensionsImporter::GetImportableExtensions(
          profile_dir_);
  if (!extensions.empty() && !cancelled()) {
    bridge_->AddExtensions(extensions);
  }

  return import_result::Success();
}

ImportResult ChromiumImporter::ImportTabs(
    user_data_importer::ImporterType importer_type) {
  const sessions::IdToSessionTab tabs =
      session_importer::ChromiumSessionImporter::GetOpenTabs(profile_dir_,
                                                             importer_type);

  std::vector<ImportedTabEntry> imported_tabs;

  std::transform(tabs.begin(), tabs.end(), std::back_inserter(imported_tabs),
                 [](const auto& it) {
                   return ImportedTabEntry::FromSessionTab(*it.second);
                 });

  bridge_->AddOpenTabs(std::move(imported_tabs));

  return import_result::Success();
}
