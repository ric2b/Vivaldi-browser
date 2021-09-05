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

  // Bit flags determing which fields should be updated in the
// UpdateAccount method
  enum UpdateAccountFields {
    ACCOUNT_NAME = 1 << 0,
    ACCOUNT_URL = 1 << 2,
  };


// AccountRow
// Holds all information associated with calendar account row.
class AccountRow {
 public:
   AccountRow() { type = 2; }
  ~AccountRow() = default;

  AccountID id;
  base::string16 name;
  GURL url;
  int type;  // 1 = Local account. // 2 = Server account.
             // Only one local account is allowed.
             // The local account is created automatically
  int updateFields;
};

typedef std::vector<AccountRow> AccountRows;

class CreateAccountResult {
 public:
  CreateAccountResult() = default;
  bool success;
  std::string message;
  AccountRow createdRow;


 private:
  DISALLOW_COPY_AND_ASSIGN(CreateAccountResult);
};

class UpdateAccountResult {
 public:
  UpdateAccountResult() = default;
  bool success;
  std::string message;
  AccountRow updatedRow;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateAccountResult);
};

class DeleteAccountResult {
 public:
  DeleteAccountResult() = default;
  bool success;
  std::string message;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteAccountResult);
};

}  // namespace calendar

#endif  // CALENDAR_ACCOUNT_TYPE_H_
