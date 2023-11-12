// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/account_type.h"

namespace calendar {

AccountRow::AccountRow()
    : account_type(ACCOUNT_TYPE_LOCAL), interval(0), updateFields(0) {}

/* CreateAccountResult::CreateAccountResult(
    const CreateAccountResult& account) =
    default;*/

AccountRow::AccountRow(const AccountRow& account)
    : id(account.id),
      name(account.name),
      url(account.url),
      username(account.username),
      account_type(account.account_type),
      interval(account.interval),
      updateFields(account.updateFields) {}

}  // namespace calendar
