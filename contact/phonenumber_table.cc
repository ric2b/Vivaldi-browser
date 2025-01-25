// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact/phonenumber_table.h"

#include <string>
#include <vector>
#include "base/strings/utf_string_conversions.h"
#include "contact/contact_type.h"
#include "sql/statement.h"

namespace contact {

namespace {

void FillPhonenumberRow(sql::Statement& statement,
                        PhonenumberRow* phonenumber_row) {
  int phonenumber_id = statement.ColumnInt(0);
  int contact_id = statement.ColumnInt(1);
  std::string phonenumber = statement.ColumnString(2);
  std::string type = statement.ColumnString(3);
  bool favorite = statement.ColumnInt(4) == 1 ? true : false;

  phonenumber_row->set_phonenumber_id(phonenumber_id);
  phonenumber_row->set_contact_id(contact_id);
  phonenumber_row->set_phonenumber(phonenumber);
  phonenumber_row->set_type(type);
  phonenumber_row->set_favorite(favorite);
}

// static
bool FillPhonenumberVector(sql::Statement& statement,
                           PhonenumberRows* phonenumbers) {
  if (!statement.is_valid())
    return false;

  while (statement.Step()) {
    PhonenumberRow phoneRow;
    FillPhonenumberRow(statement, &phoneRow);
    phonenumbers->push_back(phoneRow);
  }

  return statement.Succeeded();
}

}  // namespace

PhonenumberTable::PhonenumberTable() {}

PhonenumberTable::~PhonenumberTable() {}

bool PhonenumberTable::CreatePhonenumberTable() {
  const char* name = "phonenumbers";
  if (GetDB().DoesTableExist(name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append(
      "("
      "phonenumber_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "contact_id INTEGER,"
      "phonenumber LONGVARCHAR,"
      "type LONGVARCHAR,"
      "is_default integer DEFAULT 0,"
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  bool res = GetDB().Execute(sql);

  return res;
}

PhonenumberID PhonenumberTable::AddPhoneNumber(AddPropertyObject row) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO phonenumbers "
      "(contact_id, phonenumber, type, is_default, created, last_modified) "
      "VALUES (?, ?, ?, ?, ?, ?)"));

  statement.BindInt64(0, row.contact_id);
  statement.BindString16(1, row.value);
  statement.BindString(2, row.type);
  statement.BindInt(3, row.favorite ? 1 : 0);

  int created = base::Time().Now().ToInternalValue();
  statement.BindInt64(4, created);
  statement.BindInt64(5, created);

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool PhonenumberTable::UpdatePhoneNumber(UpdatePropertyObject row) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                                      "UPDATE phonenumbers SET \
        phonenumber=? ,type=?, is_default=?, last_modified=? \
        WHERE phonenumber_id=? and contact_id=?"));

  int modified = base::Time().Now().ToInternalValue();
  statement.BindString16(0, row.value);
  statement.BindString(1, row.type);
  statement.BindInt(2, row.favorite ? 1 : 0);
  statement.BindInt64(3, modified);
  statement.BindInt64(4, row.property_id);
  statement.BindInt64(5, row.contact_id);

  return statement.Run();
}

bool PhonenumberTable::DeletePhoneNumber(PhonenumberID phonenumber_id,
                                         ContactID contact_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE from phonenumbers WHERE phonenumber_id=? and contact_id=?"));
  statement.BindInt64(0, phonenumber_id);
  statement.BindInt64(1, contact_id);

  return statement.Run();
}

bool PhonenumberTable::GetPhonenumbersForContact(
    ContactID contact_id,
    PhonenumberRows* phonenumbers) {
  phonenumbers->clear();

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT phonenumber_id, contact_id, phonenumber, type, is_default "
      "FROM phonenumbers WHERE contact_id=?"));
  statement.BindInt64(0, contact_id);
  return FillPhonenumberVector(statement, phonenumbers);
}

bool PhonenumberTable::DoesPhonumberIdExist(PhonenumberID phonenumber_id,
                                            ContactID contact_id) {
  sql::Statement statement(
      GetDB().GetUniqueStatement("select count(*) as count from phonenumbers \
        WHERE phonenumber_id=? and contact_id=?"));
  statement.BindInt64(0, phonenumber_id);
  statement.BindInt64(1, contact_id);

  if (!statement.Step())
    return false;

  return statement.ColumnInt(0) == 1;
}

bool PhonenumberTable::DeletePhoneNumbersForContact(ContactID contact_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from phonenumbers WHERE contact_id=?"));
  statement.BindInt64(0, contact_id);

  return statement.Run();
}

}  // namespace contact
