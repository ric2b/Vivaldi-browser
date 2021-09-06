// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mail_client_database_params.h"

namespace mail_client {

MailClientDatabaseParams::MailClientDatabaseParams() {}

MailClientDatabaseParams::MailClientDatabaseParams(
    const base::FilePath& mail_client_databse_dir)
    : mail_client_databse_dir(mail_client_databse_dir) {}

MailClientDatabaseParams ::~MailClientDatabaseParams() {}

}  // namespace mail_client
