// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_CONTACT_DATABASE_PARAMS_H_
#define CONTACT_CONTACT_DATABASE_PARAMS_H_

#include "base/files/file_path.h"

namespace contact {

// ContactDatabaseParams store parameters for ContactDatabase constructor and
// Init methods so that they can be easily passed around between ContactService
// and ContactBackend.
struct ContactDatabaseParams {
  ContactDatabaseParams();
  ContactDatabaseParams(const base::FilePath& contact_dir);
  ~ContactDatabaseParams();

  base::FilePath contact_dir;
};

}  // namespace contact

#endif  // CONTACT_CONTACT_DATABASE_PARAMS_H_
