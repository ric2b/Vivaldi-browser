// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.ContactRow

#include "contact/contact_type.h"

#include <limits>
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"

namespace contact {

Contact::Contact() : name(u""), updateFields(0) {}

Contact::Contact(const Contact& contact) = default;

Contact::~Contact() {}

AddPropertyObject::AddPropertyObject() {}

AddPropertyObject::AddPropertyObject(const AddPropertyObject& contact) =
    default;

AddPropertyObject::~AddPropertyObject() {}

AddPropertyObject& AddPropertyObject::operator=(
    const AddPropertyObject& other) = default;

UpdatePropertyObject::UpdatePropertyObject() {}

UpdatePropertyObject::UpdatePropertyObject(
    const UpdatePropertyObject& contact) = default;

UpdatePropertyObject::~UpdatePropertyObject() {}

UpdatePropertyObject& UpdatePropertyObject::operator=(
    const UpdatePropertyObject& other) = default;

RemovePropertyObject::RemovePropertyObject() {}

RemovePropertyObject::RemovePropertyObject(
    const RemovePropertyObject& contact) = default;

RemovePropertyObject::~RemovePropertyObject() {}

ContactRow::ContactRow() {}

ContactRow::ContactRow(ContactID contact_id, std::u16string name)
    : contact_id_(contact_id), name_(name) {}

ContactRow::~ContactRow() {}

void ContactRow::Swap(ContactRow* other) {
  std::swap(contact_id_, other->contact_id_);
  std::swap(name_, other->name_);
  std::swap(birthday_, other->birthday_);
  std::swap(note_, other->note_);
  std::swap(emails_, other->emails_);
  std::swap(phones_, other->phones_);
  std::swap(postaladdresses_, other->postaladdresses_);
  std::swap(avatar_url_, other->avatar_url_);
  std::swap(separator_, other->separator_);
  std::swap(generated_from_sent_mail_, other->generated_from_sent_mail_);
  std::swap(trusted_, other->trusted_);
}

ContactRow::ContactRow(const ContactRow& other) = default;

ContactRow& ContactRow::operator=(const ContactRow& other) = default;

ContactResult::ContactResult() {}

ContactResult::ContactResult(const ContactRow& contact_row)
    : ContactRow(contact_row) {}

void ContactResult::SwapResult(ContactResult* other) {
  ContactRow::Swap(other);
}

ContactQueryResults::~ContactQueryResults() {}

ContactQueryResults::ContactQueryResults() {}

void ContactQueryResults::AppendContactBySwapping(ContactResult* result) {
  std::unique_ptr<ContactResult> new_result =
      base::WrapUnique(new ContactResult);
  new_result->SwapResult(result);

  results_.push_back(std::move(new_result));
}

ContactResult::~ContactResult() {}

}  // namespace contact
