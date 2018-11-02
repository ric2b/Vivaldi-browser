// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact/email_table.h"

#include <string>
#include <vector>
#include "base/strings/utf_string_conversions.h"
#include "contact/contact_type.h"
#include "sql/statement.h"

namespace contact {

namespace {

void FillEmailRow(sql::Statement& statement, EmailRow* email_row) {
  int email_id = statement.ColumnInt(0);
  int contact_id = statement.ColumnInt(1);
  std::string email = statement.ColumnString(2);
  std::string type = statement.ColumnString(3);

  email_row->set_email_id(email_id);
  email_row->set_contact_id(contact_id);
  email_row->set_email(email);
  email_row->set_type(type);
}

// static
bool FillEmailVector(sql::Statement& statement, EmailRows* emails) {
  if (!statement.is_valid())
    return false;

  while (statement.Step()) {
    EmailRow email;
    FillEmailRow(statement, &email);
    emails->push_back(email);
  }

  return statement.Succeeded();
}

}  // namespace

EmailTable::EmailTable() {}

EmailTable::~EmailTable() {}

bool EmailTable::CreateEmailTable() {
  const char* name = "emails";
  if (GetDB().DoesTableExist(name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append(
      "("
      "email_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      // Using AUTOINCREMENT is for sync propose. Sync uses this |id| as an
      // unique key to identify the Email. If here did not use
      // AUTOINCREMEclNT, and Sync was not working somehow, a ROWID could be
      // deleted and re-used during this period. Once Sync come back, Sync would
      // use ROWIDs and timestamps to see if there are any updates need to be
      // synced. And sync
      //  will only see the new Email, but missed the deleted Email.
      "contact_id INTEGER,"
      "email LONGVARCHAR,"
      "type LONGVARCHAR,"
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  bool res = GetDB().Execute(sql.c_str());

  return res;
}

EmailID EmailTable::AddEmail(AddPropertyObject row) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO emails "
      "(contact_id, email, type, created, last_modified) "
      "VALUES (?, ?, ?, ?, ?)"));

  statement.BindInt64(0, row.contact_id);
  statement.BindString16(1, row.value);
  statement.BindString(2, row.type);

  int created = base::Time().Now().ToInternalValue();
  statement.BindInt64(3, created);
  statement.BindInt64(4, created);

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool EmailTable::UpdateEmail(UpdatePropertyObject row) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                                      "UPDATE emails SET \
        email=? , last_modified=? \
        WHERE email_id=? and contact_id=?"));

  int modified = base::Time().Now().ToInternalValue();
  statement.BindString16(0, row.value);
  statement.BindInt64(1, modified);
  statement.BindInt64(2, row.property_id);
  statement.BindInt64(3, row.contact_id);

  return statement.Run();
}

bool EmailTable::DeleteEmail(contact::EmailID email_id, ContactID contact_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from emails WHERE email_id=? and contact_id=?"));
  statement.BindInt64(0, email_id);
  statement.BindInt64(1, contact_id);

  return statement.Run();
}

bool EmailTable::GetEmailsForContact(ContactID contact_id, EmailRows* emails) {
  emails->clear();

  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT email_id, contact_id, email, type "
                                 "FROM emails WHERE contact_id=?"));
  statement.BindInt64(0, contact_id);
  return FillEmailVector(statement, emails);
}

bool EmailTable::DoesEmailIdExist(EmailID email_id, ContactID contact_id) {
  sql::Statement statement(
      GetDB().GetUniqueStatement("select count(*) as count from emails \
        WHERE email_id=? and contact_id=?"));
  statement.BindInt64(0, email_id);
  statement.BindInt64(1, contact_id);

  if (!statement.Step())
    return false;

  return statement.ColumnInt(0) == 1;
}

}  // namespace contact
