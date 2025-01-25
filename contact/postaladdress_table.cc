// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact/postaladdress_table.h"

#include <string>
#include <vector>
#include "base/strings/utf_string_conversions.h"
#include "contact/postaladdress_type.h"
#include "sql/statement.h"

namespace contact {

namespace {

void FillPostalAddressRow(sql::Statement& statement,
                          PostalAddressRow* postaladdress_row) {
  int postaladdress_id = statement.ColumnInt(0);
  int contact_id = statement.ColumnInt(1);
  std::u16string postal_address = statement.ColumnString16(2);
  std::string type = statement.ColumnString(3);

  postaladdress_row->set_postaladdress_id(postaladdress_id);
  postaladdress_row->set_contact_id(contact_id);
  postaladdress_row->set_postal_address(postal_address);
  postaladdress_row->set_type(type);
}

// static
bool FillPostalAddressVector(sql::Statement& statement,
                             PostalAddressRows* postaladdresses) {
  if (!statement.is_valid())
    return false;

  while (statement.Step()) {
    PostalAddressRow postal_address_row;
    FillPostalAddressRow(statement, &postal_address_row);
    postaladdresses->push_back(postal_address_row);
  }

  return statement.Succeeded();
}

}  // namespace

PostalAddressTable::PostalAddressTable() {}

PostalAddressTable::~PostalAddressTable() {}

bool PostalAddressTable::CreatePostalAddressTable() {
  const char* name = "postaladdress";
  if (GetDB().DoesTableExist(name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append(
      "("
      "postaladdress_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "contact_id INTEGER,"
      "postaladdress LONGVARCHAR,"
      "type LONGVARCHAR,"
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  return GetDB().Execute(sql);
}

PostalAddressID PostalAddressTable::AddPostalAddress(AddPropertyObject row) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO postaladdress "
      "(contact_id, postaladdress, type, created, last_modified) "
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

bool PostalAddressTable::UpdatePostalAddress(UpdatePropertyObject row) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "UPDATE postaladdress SET \
        postaladdress=? ,type=?, last_modified=? \
        WHERE postaladdress_id=? and contact_id=?"));

  int modified = base::Time().Now().ToInternalValue();
  statement.BindString16(0, row.value);
  statement.BindString(1, row.type);
  statement.BindInt64(2, modified);
  statement.BindInt64(3, row.property_id);
  statement.BindInt64(4, row.contact_id);

  return statement.Run();
}

bool PostalAddressTable::DeletePostalAddress(PostalAddressID postaladdress_id,
                                             ContactID contact_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE from postaladdress WHERE postaladdress_id=? and contact_id=?"));
  statement.BindInt64(0, postaladdress_id);
  statement.BindInt64(1, contact_id);

  return statement.Run();
}

bool PostalAddressTable::GetPostalAddressesForContact(
    ContactID contact_id,
    PostalAddressRows* postaladdresses) {
  postaladdresses->clear();

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT postaladdress_id, contact_id, postaladdress, type "
      "FROM postaladdress WHERE contact_id=?"));
  statement.BindInt64(0, contact_id);
  return FillPostalAddressVector(statement, postaladdresses);
}

bool PostalAddressTable::DoesPostalAddressIdExist(
    PostalAddressID postaladdress_id,
    ContactID contact_id) {
  sql::Statement statement(
      GetDB().GetUniqueStatement("select count(*) as count from postaladdress \
        WHERE postaladdress_id=? and contact_id=?"));
  statement.BindInt64(0, postaladdress_id);
  statement.BindInt64(1, contact_id);

  if (!statement.Step())
    return false;

  return statement.ColumnInt(0) == 1;
}

bool PostalAddressTable::DeletePostalAddressesForContact(ContactID contact_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from postaladdress WHERE contact_id=?"));
  statement.BindInt64(0, contact_id);

  return statement.Run();
}

}  // namespace contact
