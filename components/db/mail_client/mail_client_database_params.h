// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_DATABASE_PARAMS_H_
#define COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_DATABASE_PARAMS_H_

#include "base/files/file_path.h"

namespace mail_client {

// MailClientDatabaseParams store parameters for Mail database constructor and
// Init methods so that they can be easily passed around between
// MailClientService and MailClientBackend.
struct MailClientDatabaseParams {
  MailClientDatabaseParams();
  explicit MailClientDatabaseParams(
      const base::FilePath& mail_client_databse_dir);
  ~MailClientDatabaseParams();

  base::FilePath mail_client_databse_dir;
};

}  // namespace mail_client

#endif  // COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_DATABASE_PARAMS_H_
