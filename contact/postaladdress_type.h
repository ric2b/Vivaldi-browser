// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_POSTALADDRESS_TYPE_H_
#define CONTACT_POSTALADDRESS_TYPE_H_

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

// Contacts postal address row.
class PostalAddressRow {
 public:
  PostalAddressRow() = default;
  ~PostalAddressRow() = default;

  PostalAddressRow(const PostalAddressRow& row);

  PostalAddressRow& operator=(const PostalAddressRow& other);

  PostalAddressID postal_address_id() const { return postal_address_id_; }
  void set_postaladdress_id(PostalAddressID postal_address_id) {
    postal_address_id_ = postal_address_id;
  }

  ContactID contact_id() const { return contact_id_; }
  void set_contact_id(ContactID contact_id) { contact_id_ = contact_id; }

  std::u16string postal_address() const { return postal_address_; }
  void set_postal_address(std::u16string postal_address) {
    postal_address_ = postal_address;
  }

  std::string type() const { return type_; }
  void set_type(std::string type) { type_ = type; }

  PostalAddressID postal_address_id_;
  ContactID contact_id_;
  std::u16string postal_address_;
  std::string type_;

 protected:
  void Swap(PostalAddressRow* other);
};

typedef std::vector<PostalAddressRow> PostalAddressRows;

}  // namespace contact

#endif  // CONTACT_POSTALADDRESS_TYPE_H_
