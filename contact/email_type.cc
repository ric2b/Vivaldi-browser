// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact/email_type.h"

#include <limits>
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"

namespace contact {

EmailAddressRow::EmailAddressRow() {}

EmailAddressRow::~EmailAddressRow() {}

void EmailAddressRow::Swap(EmailAddressRow* other) {
  std::swap(email_address_id_, other->email_address_id_);
  std::swap(contact_id_, other->contact_id_);
  std::swap(email_address_, other->email_address_);
  std::swap(type_, other->type_);
  std::swap(favorite_, other->favorite_);
  std::swap(obsolete_, other->obsolete_);
}

EmailAddressRow::EmailAddressRow(const EmailAddressRow& other) = default;

EmailAddressRow& EmailAddressRow::operator=(const EmailAddressRow& email_row) =
    default;

}  // namespace contact
