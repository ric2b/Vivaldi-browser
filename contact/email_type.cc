// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.EmailRow

#include "contact/email_type.h"

#include <limits>
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"

namespace contact {

Email::Email() : email(""), updateFields(0) {}

Email::Email(const Email& email) = default;

Email::~Email() {}

EmailRow::EmailRow() {}

EmailRow::~EmailRow() {}

void EmailRow::Swap(EmailRow* other) {
  std::swap(email_id_, other->email_id_);
  std::swap(contact_id_, other->contact_id_);
  std::swap(email_, other->email_);
  std::swap(type_, other->type_);
}

EmailRow::EmailRow(const EmailRow& other) = default;

EmailRow& EmailRow::operator=(const EmailRow& email_row) = default;

}  // namespace contact
