// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact/phonenumber_type.h"

#include <limits>
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"

namespace contact {

PhonenumberRow::PhonenumberRow() {}

PhonenumberRow::~PhonenumberRow() {}

void PhonenumberRow::Swap(PhonenumberRow* other) {
  std::swap(phonenumber_id_, other->phonenumber_id_);
  std::swap(contact_id_, other->contact_id_);
  std::swap(type_, other->type_);
  std::swap(phonenumber_, other->phonenumber_);
  std::swap(favorite_, other->favorite_);
}

PhonenumberRow::PhonenumberRow(const PhonenumberRow& other) = default;

PhonenumberRow& PhonenumberRow::operator=(const PhonenumberRow& telephone_row) =
    default;

}  // namespace contact
