// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_ACCOUNT_TABLE_H_
#define CALENDAR_ACCOUNT_TABLE_H_

#include <stddef.h>

#include "calendar/account_type.h"
#include "sql/statement.h"

namespace sql {
class Database;
}

namespace calendar {

// Encapsulates an SQL table that holds calendar account info.
//
// This is refcounted to support calling InvokeLater() with some of its methods
// (necessary to maintain ordering of DB operations).
class AccountTable {
 public:
  // Must call CreateAccountTable() before to make sure the database is
  // initialized.
  AccountTable();

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~AccountTable();
  AccountTable(const AccountTable&) = delete;
  AccountTable& operator=(const AccountTable&) = delete;

  bool CreateAccountTable();
  AccountID CreateDefaultAccount();

  AccountID CreateAccount(AccountRow row);
  bool GetAllAccounts(AccountRows* accounts);
  bool GetRowForAccount(AccountID account_id, AccountRow* out_account);
  bool UpdateAccountRow(const AccountRow& account);
  bool DeleteAccount(AccountID account_id);
  bool MigrateCalendarToVersion7();

 protected:
  virtual sql::Database& GetDB() = 0;
  void FillAccountRow(sql::Statement& statement, AccountRow* account);

 private:
  std::string GURLToDatabaseURL(const GURL& gurl);
  bool DoesLocalAccountExist();
};

#define ACCOUNT_ROW_FIELDS \
  " id, name, url, username, type, created, last_modified "

}  // namespace calendar

#endif  // CALENDAR_ACCOUNT_TABLE_H_
