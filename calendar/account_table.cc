// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/account_table.h"

#include <string>
#include <vector>

#include "app/vivaldi_resources.h"
#include "base/strings/utf_string_conversions.h"
#include "calendar/account_type.h"
#include "sql/statement.h"
#include "ui/base/l10n/l10n_util.h"

namespace calendar {

AccountTable::AccountTable() {}

AccountTable::~AccountTable() {}

bool AccountTable::CreateAccountTable() {
  const char* name = "accounts";
  if (GetDB().DoesTableExist(name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append(
      "("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "name LONGVARCHAR,"
      "url LONGVARCHAR,"
      "username LONGVARCHAR,"
      "type INTEGER,"
      "interval INTEGER DEFAULT 0,"
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  bool res = GetDB().Execute(sql);

  return res;
}

AccountID AccountTable::CreateDefaultAccount() {
  if (DoesLocalAccountExist())
    return false;

  AccountRow row;
  row.name = l10n_util::GetStringUTF16(IDS_DEFAULT_CALENDAR_ACCOUNT_NAME);
  row.account_type = ACCOUNT_TYPE_LOCAL;
  row.interval = 0;
  row.username = u"";
  return CreateAccount(row);
}

// static
std::string AccountTable::GURLToDatabaseURL(const GURL& gurl) {
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();

  return (gurl.ReplaceComponents(replacements)).spec();
}

AccountID AccountTable::CreateAccount(AccountRow row) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO accounts "
                                 "(name, url, username, type, interval, "
                                 "created, last_modified) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?)"));
  int column_index = 0;
  statement.BindString16(column_index++, row.name);
  statement.BindString(column_index++, GURLToDatabaseURL(row.url));
  statement.BindString16(column_index++, row.username);
  statement.BindInt(column_index++, row.account_type);
  statement.BindInt(column_index++, row.interval);

  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());
  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool AccountTable::GetAllAccounts(AccountRows* accounts) {
  accounts->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, name, url, username, type, interval, "
      "created, last_modified FROM accounts"));
  while (s.Step()) {
    AccountRow account;
    FillAccountRow(s, &account);
    accounts->push_back(account);
  }

  return true;
}

bool AccountTable::UpdateAccountRow(const AccountRow& account) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                                      "UPDATE accounts SET \
        name=?, url=?, type=?, username=?, interval=? WHERE id=?"));
  int column_index = 0;
  statement.BindString16(column_index++, account.name);
  statement.BindString(column_index++, GURLToDatabaseURL(account.url));
  statement.BindInt64(column_index++, account.account_type);
  statement.BindString16(column_index++, account.username);
  statement.BindInt64(column_index++, account.interval);
  statement.BindInt64(column_index, account.id);

  return statement.Run();
}

bool AccountTable::DeleteAccount(AccountID calendar_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from accounts WHERE id=?"));
  statement.BindInt64(0, calendar_id);

  return statement.Run();
}

bool AccountTable::GetRowForAccount(AccountID account_id,
                                    AccountRow* out_account) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" ACCOUNT_ROW_FIELDS "FROM accounts WHERE id=?"));
  statement.BindInt64(0, account_id);

  if (!statement.Step())
    return false;

  FillAccountRow(statement, out_account);

  return true;
}

void AccountTable::FillAccountRow(sql::Statement& statement,
                                  AccountRow* account) {
  int column_index = 0;
  AccountID id = statement.ColumnInt64(column_index++);
  std::u16string name = statement.ColumnString16(column_index++);
  std::string url = statement.ColumnString(column_index++);
  std::u16string username = statement.ColumnString16(column_index++);
  int type = statement.ColumnInt(column_index++);
  int interval = statement.ColumnInt(column_index++);

  account->id = id;
  account->name = name;
  account->url = GURL(url);
  account->username = username;
  account->account_type = type;
  account->interval = interval;
}

bool AccountTable::DoesLocalAccountExist() {
  sql::Statement statement(GetDB().GetUniqueStatement(
      "select count(*) from accounts where type = 0"));

  if (!statement.Step())
    return false;

  return statement.ColumnInt(0) > 0;
}

// Updates to version 7. Adds column username and interval to account table
bool AccountTable::MigrateCalendarToVersion7() {
  if (!GetDB().DoesTableExist("accounts")) {
    NOTREACHED() << "accounts table should exist before migration";
    //return false;
  }

  if (!GetDB().DoesColumnExist("accounts", "username")) {
    if (!GetDB().Execute("ALTER TABLE accounts "
                         "ADD COLUMN username LONGVARCHAR"))
      return false;
  }

  if (!GetDB().DoesColumnExist("accounts", "interval")) {
    if (!GetDB().Execute("ALTER TABLE accounts "
                         "ADD COLUMN interval INTEGER DEFAULT 0"))
      return false;
  }

  if (!GetDB().DoesColumnExist("calendar", "account_id")) {
    if (!GetDB().Execute("ALTER TABLE calendar "
                         "ADD COLUMN account_id INTEGER"))
      return false;
  }

  return true;
}

}  // namespace calendar
