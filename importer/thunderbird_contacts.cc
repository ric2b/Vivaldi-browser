// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_util.h"
#include "contact/contact_type.h"
#include "imported_contact_row.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace thunderbirdContacts {

using contact::ContactRow;
using contact::ContactRows;
using contact::EmailAddressRow;
using contact::EmailAddressRows;
using contact::PhonenumberRow;
using contact::PhonenumberRows;
using contact::PostalAddressRow;
using contact::PostalAddressRows;

ContactRows CombineContacts(const std::vector<ImportedContact>& contact_rows) {
  std::map<std::u16string, ContactRow*> contacts;

  for (auto& contact : contact_rows) {
    auto iter = contacts.find(contact.id);
    if (iter == contacts.end()) {
      contacts.emplace(contact.id, new ContactRow());
    }
  }

  for (auto& contact : contact_rows) {
    auto iter = contacts.find(contact.id);
    if (contact.name == u"DisplayName") {
      iter->second->set_name(contact.value);
    } else if (contact.name == u"PrimaryEmail" ||
               contact.name == u"SecondEmail") {
      EmailAddressRows emails = iter->second->emails();
      EmailAddressRow email;
      email.set_email_address(contact.value);
      emails.push_back(email);
      iter->second->set_emails(emails);
    } else if (contact.name == u"HomePhone" || contact.name == u"WorkPhone") {
      PhonenumberRows phone_numbers = iter->second->phones();
      PhonenumberRow phone;
      phone.set_phonenumber(base::UTF16ToUTF8(contact.value));
      phone_numbers.push_back(phone);
      iter->second->set_phones(phone_numbers);
    } else if (contact.name == u"HomeAddress") {
      PostalAddressRows addresses = iter->second->postaladdresses();
      PostalAddressRow address;
      address.set_postal_address(contact.value);
      addresses.push_back(address);
      iter->second->set_postaladdresses(addresses);
    } else if (contact.name == u"Notes") {
      iter->second->set_note(contact.value);
    }
  }

  ContactRows contacts_to_create;
  for (auto it = contacts.begin(); it != contacts.end(); it++) {
    contacts_to_create.push_back(std::move(*it->second));
  }

  return contacts_to_create;
}

void ImportContacts(const base::FilePath file,
                    std::vector<ImportedContact>& contacts) {
  sql::Database db;
  if (!db.Open(file)) {
    return;
  }

  const char query[] = "select card as id, name, value from properties";
  sql::Statement s(db.GetUniqueStatement(query));

  while (s.Step()) {
    ImportedContact contact;
    std::u16string id = s.ColumnString16(0);
    contact.id = id;
    contact.name = s.ColumnString16(1);
    contact.value = s.ColumnString16(2);
    contacts.push_back(std::move(contact));
  }

  s.Clear();
  db.Close();
}

void Import(std::string path,
            std::string db_name,
            std::vector<ImportedContact>& contacts) {
  const base::FilePath file =
      base::FilePath::FromUTF8Unsafe(path).AppendASCII(db_name);
  if (!base::PathExists(file)) {
    LOG(ERROR) << "SQLite file path not found: " << file;
    return;
  }

  ImportContacts(file, contacts);
}

void Read(std::string path, ContactRows& contact_rows) {
  std::vector<ImportedContact> contacts;
  Import(path, "abook.sqlite", contacts);
  Import(path, "history.sqlite", contacts);

  contact_rows = CombineContacts(contacts);
}

}  // namespace thunderbirdContacts
