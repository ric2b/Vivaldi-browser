// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_EMAIL_TYPE_H_
#define CONTACT_EMAIL_TYPE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "contact/contact_typedefs.h"
#include "url/gurl.h"

namespace contact {

// Holds all information associated with a specific contact.
class EmailAddressRow {
 public:
  EmailAddressRow();
  ~EmailAddressRow();

  EmailAddressRow(const EmailAddressRow& row);

  EmailAddressRow& operator=(const EmailAddressRow& other);

  EmailAddressID email_address_id() const { return email_address_id_; }
  void set_email_address_id(EmailAddressID email_address_id) {
    email_address_id_ = email_address_id;
  }

  ContactID contact_id() const { return contact_id_; }
  void set_contact_id(ContactID contact_id) { contact_id_ = contact_id; }

  std::u16string email_address() const { return email_address_; }
  void set_email_address(std::u16string email_address) {
    email_address_ = email_address;
  }

  std::string type() const { return type_; }
  void set_type(std::string type) { type_ = type; }

  bool favorite() const { return favorite_; }
  void set_favorite(bool favorite) { favorite_ = favorite; }

  bool obsolete() const { return obsolete_; }
  void set_obsolete(bool obsolete) { obsolete_ = obsolete; }

  EmailAddressID email_address_id_;
  ContactID contact_id_;
  std::u16string email_address_;
  std::string type_;
  bool favorite_;
  bool obsolete_;

 protected:
  void Swap(EmailAddressRow* other);
};

typedef std::vector<EmailAddressRow> EmailAddressRows;

}  // namespace contact

#endif  // CONTACT_EMAIL_TYPE_H_
