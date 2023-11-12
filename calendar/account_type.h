// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_ACCOUNT_TYPE_H_
#define CALENDAR_ACCOUNT_TYPE_H_

#include <string>
#include <vector>
#include "url/gurl.h"

#include "calendar/calendar_typedefs.h"

namespace calendar {

enum AccountType {
  ACCOUNT_TYPE_LOCAL = 0,
  ACCOUNT_TYPE_VIVALDINET = 1,
  ACCOUNT_TYPE_GOOGLE = 2,
  ACCOUNT_TYPE_CALDAV = 3,
  ACCOUNT_TYPE_ICAL = 4,
  ACCOUNT_TYPE_FASTMAIL = 5
};

// Bit flags determing which fields should be updated in the
// UpdateAccount method
enum UpdateAccountFields {
  ACCOUNT_NAME = 1 << 0,
  ACCOUNT_URL = 1 << 1,
  ACCOUNT_USERNAME = 1 << 2,
  ACCOUNT_TYPE = 1 << 3,
  ACCOUNT_INTERVAL = 1 << 4,
};

// AccountRow
// Holds all information associated with calendar account row.
class AccountRow {
 public:
  AccountRow();
  ~AccountRow() = default;
  AccountRow(const AccountRow& account);

  AccountID id;
  std::u16string name;
  GURL url;
  std::u16string username;
  int account_type;  // The type of account.
                     // 0: Local.
                     // 1: Vivaldi.net calendar.
                     // 2: Google Calendar.
                     // 3: CalDAV.
                     // 4: Read only iCal.
                     // The local account is created automatically.
                     // Only one local account is permitted.
  int interval;
  int updateFields;
};

typedef std::vector<AccountRow> AccountRows;

class CreateAccountResult {
 public:
  CreateAccountResult() = default;
  CreateAccountResult(const CreateAccountResult&) = default;
  CreateAccountResult& operator=(CreateAccountResult& account) = default;

  bool success;
  std::string message;
  AccountRow createdRow;
};

class UpdateAccountResult {
 public:
  UpdateAccountResult() = default;
  UpdateAccountResult(const UpdateAccountResult&) = default;
  UpdateAccountResult& operator=(UpdateAccountResult& account) = default;

  bool success;
  std::string message;
  AccountRow updatedRow;
};

class DeleteAccountResult {
 public:
  DeleteAccountResult() = default;
  DeleteAccountResult(const DeleteAccountResult& account) = default;
  DeleteAccountResult& operator=(DeleteAccountResult& account) = default;

  bool success;
  std::string message;
};

}  // namespace calendar

#endif  // CALENDAR_ACCOUNT_TYPE_H_
