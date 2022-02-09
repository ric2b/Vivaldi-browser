// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_POSTALADDRESS_TABLE_H_
#define CONTACT_POSTALADDRESS_TABLE_H_

#include <stddef.h>

#include "contact/contact_type.h"
#include "contact/postaladdress_type.h"
#include "sql/statement.h"

namespace sql {
class Database;
}

namespace contact {

// Encapsulates an SQL table that holds contacts postal adresses.
//
class PostalAddressTable {
 public:
  // Must call CreatePostalAddressTable() before to make sure the database is
  // initialized.
  PostalAddressTable();

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~PostalAddressTable();

  PostalAddressTable(const PostalAddressTable&) = delete;
  PostalAddressTable& operator=(const PostalAddressTable&) = delete;

  PostalAddressID AddPostalAddress(AddPropertyObject row);
  bool UpdatePostalAddress(UpdatePropertyObject row);
  bool DeletePostalAddress(PostalAddressID postaladdress_id,
                           ContactID contact_id);
  bool GetPostalAddressesForContact(ContactID contact_id,
                                    PostalAddressRows* postaladdresses);
  bool DoesPostalAddressIdExist(PostalAddressID postaladdress_id,
                                ContactID contact_id);
  bool DeletePostalAddressesForContact(ContactID contact_id);

 protected:
  virtual sql::Database& GetDB() = 0;
  bool CreatePostalAddressTable();
};

}  // namespace contact

#endif  // CONTACT_POSTALADDRESS_TABLE_H_
