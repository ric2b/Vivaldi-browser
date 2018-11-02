// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_PHONENUMBER_TABLE_H_
#define CONTACT_PHONENUMBER_TABLE_H_

#include <stddef.h>

#include "base/macros.h"
#include "contact/contact_type.h"
#include "contact/phonenumber_type.h"
#include "sql/statement.h"

namespace sql {
class Connection;
}

namespace contact {

// Encapsulates an SQL table that holds contacts phonenumbers.
//
class PhonenumberTable {
 public:
  // Must call CreatePhonenumberTable() before to make sure the database is
  // initialized.
  PhonenumberTable();

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~PhonenumberTable();

  PhonenumberID AddPhoneNumber(AddPropertyObject row);
  bool UpdatePhoneNumber(UpdatePropertyObject row);
  bool DeletePhoneNumber(PhonenumberID phonenumber_id, ContactID contact_id);
  bool GetPhonenumbersForContact(ContactID contact_id,
                                 PhonenumberRows* phonenumbers);
  bool DoesPhonumberIdExist(PhonenumberID phonenumber_id, ContactID contact_id);

 protected:
  virtual sql::Connection& GetDB() = 0;
  bool CreatePhonenumberTable();

 private:
  DISALLOW_COPY_AND_ASSIGN(PhonenumberTable);
};

}  // namespace contact

#endif  // CONTACT_PHONENUMBER_TABLE_H_
