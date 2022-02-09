// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_EMAIL_TABLE_H_
#define CONTACT_EMAIL_TABLE_H_

#include <stddef.h>
#include "sql/statement.h"

#include "contact/contact_type.h"
#include "contact/email_type.h"

namespace sql {
class Database;
}

namespace contact {

// Encapsulates an SQL table that holds Email address info.
//
// This is refcounted to support calling InvokeLater() with some of its methods
// (necessary to maintain ordering of DB operations).
class EmailTable {
 public:
  // Must call CreateEmailTable() before to make sure the database is
  // initialized.
  EmailTable();

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~EmailTable();

  EmailTable(const EmailTable&) = delete;
  EmailTable& operator=(const EmailTable&) = delete;

  EmailAddressID AddEmailAddress(EmailAddressRow row);
  bool UpdateEmailAddress(EmailAddressRow row);
  bool DeleteEmail(EmailAddressID email_id, ContactID contact_id);
  bool GetEmailsForContact(ContactID contact_id, EmailAddressRows* emails);
  bool GetAllEmailAddresses(EmailAddressRows* emails);
  bool DoesEmailAddressIdExist(EmailAddressID email_address_id,
                               ContactID contact_id);
  bool DeleteEmailsForContact(ContactID contact_id);

 protected:
  virtual sql::Database& GetDB() = 0;
  bool CreateEmailTable();
};

}  // namespace contact

#endif  // CONTACT_EMAIL_TABLE_H_
