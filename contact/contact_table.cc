// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact/contact_table.h"

#include <string>
#include <vector>
#include "base/strings/utf_string_conversions.h"
#include "contact/contact_type.h"
#include "sql/statement.h"

namespace contact {

namespace {

void FillContactRow(sql::Statement& statement, ContactRow* contact) {
  int id = statement.ColumnInt(0);
  base::string16 name = statement.ColumnString16(1);
  base::Time birthday =
    base::Time::FromInternalValue(statement.ColumnInt64(2));
  base::string16 note = statement.ColumnString16(3);
  contact->set_contact_id(id);
  contact->set_name(name);
  contact->set_birthday(birthday);
  contact->set_note(note);
}

}  // namespace

ContactTable::ContactTable() {}

ContactTable::~ContactTable() {}

bool ContactTable::CreateContactTable() {
  const char* name = "contacts";
  if (GetDB().DoesTableExist(name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append(
      "("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      // Using AUTOINCREMENT is for sync propose. Sync uses this |id| as an
      // unique key to identify the Contact. If here did not use
      // AUTOINCREMEclNT, and Sync was not working somehow, a ROWID could be
      // deleted and re-used during this period. Once Sync come back, Sync would
      // use ROWIDs and timestamps to see if there are any updates need to be
      // synced. And sync
      //  will only see the new Contact, but missed the deleted Contact.
      "fn LONGVARCHAR,"
      "birthday INTEGER,"  // Timestamp since epoch
      "note LONGVARCHAR,"
      "last_used INTEGER,"  // Timestamp since epoch since you either sent or
                            // received an email for given contact
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  bool res = GetDB().Execute(sql.c_str());

  return res;
}

ContactID ContactTable::CreateContact(ContactRow row) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO contacts "
                                 "(fn, birthday, note, created, last_modified) "
                                 "VALUES (?, ?, ?, ?, ?)"));

  statement.BindString16(0, row.name());
  statement.BindInt64(1, row.birthday().ToInternalValue());
  statement.BindString16(2, row.note());

  int created = base::Time().Now().ToInternalValue();
  statement.BindInt64(3, created);
  statement.BindInt64(4, created);

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool ContactTable::GetAllContacts(ContactRows* contacts) {
  contacts->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT id, fn, birthday, note FROM contacts"));
  while (s.Step()) {
    ContactRow contact;
    FillContactRow(s, &contact);
    contacts->push_back(contact);
  }

  return true;
}

bool ContactTable::UpdateContactRow(const ContactRow& contact) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "UPDATE contacts SET fn=?, birthday=?, "
                                 "note=?, last_modified=? WHERE id=?"));
  statement.BindString16(0, contact.name());
  statement.BindInt64(1, contact.birthday().ToInternalValue());
  statement.BindString16(2, contact.note());
  statement.BindInt64(3, base::Time().Now().ToInternalValue());
  statement.BindInt64(4, contact.contact_id());
  return statement.Run();
}

bool ContactTable::DeleteContact(contact::ContactID contact_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from contacts WHERE id=?"));
  statement.BindInt64(0, contact_id);

  return statement.Run();
}

bool ContactTable::GetRowForContact(ContactID contact_id,
                                    ContactRow* out_contact) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT id, fn, birthday, note, created, "
                                 "last_modified FROM contacts WHERE id=?"));
  statement.BindInt64(0, contact_id);

  if (!statement.Step())
    return false;

  FillContactRow(statement, out_contact);

  return true;
}

}  // namespace contact
