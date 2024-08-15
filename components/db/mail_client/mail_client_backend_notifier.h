// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_BACKEND_NOTIFIER_H_
#define COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_BACKEND_NOTIFIER_H_

namespace mail_client {

// The MailClientBackendNotifier forwards notifications from the
// MailClientBackend's client to all the interested observers.
class MailClientBackendNotifier {
 public:
  MailClientBackendNotifier() {}
  virtual ~MailClientBackendNotifier() {}

  // Reports search database migration process
  virtual void NotifyMigrationProgress(int progress,
                                       int total,
                                       std::string msg) = 0;

  // Reports status on Deleting messages from search db
  virtual void NotifyDeleteMessages(int delete_progress_count) = 0;
};

}  // namespace mail_client

#endif  // COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_BACKEND_NOTIFIER_H_
