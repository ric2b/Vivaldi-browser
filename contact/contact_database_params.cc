// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact_database_params.h"

namespace contact {

ContactDatabaseParams::ContactDatabaseParams() {}

ContactDatabaseParams::ContactDatabaseParams(const base::FilePath& contact_dir)
    : contact_dir(contact_dir) {}

ContactDatabaseParams::~ContactDatabaseParams() {}

}  // namespace contact
