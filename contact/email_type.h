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
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "contact/contact_typedefs.h"
#include "url/gurl.h"

namespace contact {

// Bit flags determing which fields should be updated in the
// UpdateEmail method
enum UpdateEmailFields {
  EMAIL_ID = 1 << 0,
  TYPE = 1 << 1,
  EMAIL = 1 << 2,
};

// Represents a simplified version of a Email.
struct Email {
  Email();
  Email(const Email& email);
  ~Email();
  EmailID email_id;
  std::string email;
  std::string type;
  int updateFields;
};

// Holds all information associated with a specific contact.
class EmailRow {
 public:
  EmailRow();
  ~EmailRow();

  EmailRow(const EmailRow& row);

  EmailRow& operator=(const EmailRow& other);

  EmailID email_id() const { return email_id_; }
  void set_email_id(EmailID email_id) { email_id_ = email_id; }

  ContactID contact_id() const { return contact_id_; }
  void set_contact_id(ContactID contact_id) { contact_id_ = contact_id; }

  std::string email() const { return email_; }
  void set_email(std::string email) { email_ = email; }

  std::string type() const { return type_; }
  void set_type(std::string type) { type_ = type; }

  EmailID email_id_;
  ContactID contact_id_;
  std::string email_;
  std::string type_;

 protected:
  void Swap(EmailRow* other);
};

typedef std::vector<EmailRow> EmailRows;

}  // namespace contact

#endif  // CONTACT_EMAIL_TYPE_H_
