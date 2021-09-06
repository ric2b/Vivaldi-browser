// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_PHONENUMBER_TYPE_H_
#define CONTACT_PHONENUMBER_TYPE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "contact/contact_typedefs.h"
#include "url/gurl.h"

namespace contact {

// Contacts phonenumber row.
class PhonenumberRow {
 public:
  PhonenumberRow();
  ~PhonenumberRow();

  PhonenumberRow(const PhonenumberRow& row);

  PhonenumberRow& operator=(const PhonenumberRow& other);

  PhonenumberID phonenumber_id() const { return phonenumber_id_; }
  void set_phonenumber_id(PhonenumberID phonenumber_id) {
    phonenumber_id_ = phonenumber_id;
  }

  ContactID contact_id() const { return contact_id_; }
  void set_contact_id(ContactID contact_id) { contact_id_ = contact_id; }

  std::string phonenumber() const { return phonenumber_; }
  void set_phonenumber(std::string phonenumber) { phonenumber_ = phonenumber; }

  std::string type() const { return type_; }
  void set_type(std::string type) { type_ = type; }

  bool is_favorite() const { return favorite_; }
  void set_favorite(bool favorite) { favorite_ = favorite; }

  PhonenumberID phonenumber_id_;
  ContactID contact_id_;
  std::string phonenumber_;
  std::string type_;
  bool favorite_;

 protected:
  void Swap(PhonenumberRow* other);
};

typedef std::vector<PhonenumberRow> PhonenumberRows;

}  // namespace contact

#endif  // CONTACT_PHONENUMBER_TYPE_H_
