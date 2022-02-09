// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_CONTACT_TABLE_H_
#define CONTACT_CONTACT_TABLE_H_

#include <stddef.h>

#include "contact/contact_type.h"
#include "sql/statement.h"

namespace sql {
class Database;
}

namespace contact {

// Encapsulates an SQL table that holds Contact info.
//
// This is refcounted to support calling InvokeLater() with some of its methods
// (necessary to maintain ordering of DB operations).
class ContactTable {
 public:
  // Must call CreateContactTable() before to make sure the database is
  // initialized.
  ContactTable();

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~ContactTable();

  ContactTable(const ContactTable&) = delete;
  ContactTable& operator=(const ContactTable&) = delete;

  ContactID CreateContact(ContactRow row);
  bool GetAllContacts(ContactRows* contacts);
  bool GetRowForContact(ContactID contact_id, ContactRow* out_contact);
  bool UpdateContactRow(const ContactRow& contact);
  bool DeleteContact(ContactID contact_id);

 protected:
  virtual sql::Database& GetDB() = 0;
  bool CreateContactTable();
};

}  // namespace contact

#endif  // CONTACT_CONTACT_TABLE_H_
