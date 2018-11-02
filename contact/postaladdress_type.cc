// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact/postaladdress_type.h"

#include <limits>
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"

namespace contact {

void PostalAddressRow::Swap(PostalAddressRow* other) {
  std::swap(postal_address_id_, other->postal_address_id_);
  std::swap(contact_id_, other->contact_id_);
  std::swap(postal_address_, other->postal_address_);
  std::swap(type_, other->type_);
}

PostalAddressRow::PostalAddressRow(const PostalAddressRow& other) = default;

PostalAddressRow& PostalAddressRow::operator=(
    const PostalAddressRow& postaladdress_row) = default;

}  // namespace contact
