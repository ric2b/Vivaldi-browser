// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "importer/chromium_importer.h"

#include <stack>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/ini_parser.h"
#include "components/autofill/core/common/password_form.h"
#include "components/os_crypt/os_crypt.h"
#include "importer/chrome_importer_utils.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "ui/base/l10n/l10n_util.h"

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
    ImportPasswords();
    bridge_->NotifyItemEnded(importer::PASSWORDS);
  }

  bridge_->NotifyEnded();
}

void ChromiumImporter::ImportPasswords() {
  // Initializes Chrome decryptor

  std::vector<autofill::PasswordForm> forms;
  base::FilePath source_path = profile_dir_;

  base::FilePath file = source_path.AppendASCII("Login Data");
  if (base::PathExists(file)) {
    ReadAndParseSignons(file, &forms);
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
    std::vector<autofill::PasswordForm>* forms) {
  sql::Connection db;
  if (!db.Open(sqlite_file))
    return false;

  const char query2[] =
      "SELECT origin_url, action_url, username_element, username_value, "
      "password_element, password_value, submit_element, signon_realm, "
      "preferred FROM logins";

  sql::Statement s2(db.GetUniqueStatement(query2));
  if (!s2.is_valid())
    return false;

  while (s2.Step()) {
    autofill::PasswordForm form;
    form.origin = GURL(s2.ColumnString(0));
    form.action = GURL(s2.ColumnString(1));
    form.username_element = base::UTF8ToUTF16(s2.ColumnString(2));
    form.username_value = base::UTF8ToUTF16(s2.ColumnString(3));
    form.password_element = base::UTF8ToUTF16(s2.ColumnString(4));
    const std::string& cipher_text = s2.ColumnString(5);
    base::string16 plain_text;

#if defined(OS_MACOSX)
    const std::string service_name = "Chrome Safe Storage";
    const std::string account_name = "Chrome";
    OSCrypt::DecryptImportedString16(cipher_text, &plain_text, service_name,
                                     account_name);
#else
    OSCrypt::DecryptString16(cipher_text, &plain_text);
#endif

    form.password_value = plain_text;

    form.submit_element = base::UTF8ToUTF16(s2.ColumnString(6));
    form.signon_realm = s2.ColumnString(7);
    form.preferred = s2.ColumnBool(8);

    forms->push_back(form);
  }
#if defined(OS_MACOSX)
  OSCrypt::ResetImportCache();
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
  sql::Connection db;
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
