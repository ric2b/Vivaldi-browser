// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_MODEL_OBSERVER_H_
#define COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_MODEL_OBSERVER_H_

#include "base/observer_list_types.h"

namespace mail_client {

class MailClientService;

// Observer for the MailClient Model/Service.
class MailClientModelObserver : public base::CheckedObserver {
 public:
  // Invoked when the model has finished loading.
  virtual void MailClientModelLoaded(MailClientService* model) {}

  virtual void ExtensiveMailClientChangesBeginning(MailClientService* service) {
  }
  virtual void ExtensiveMailClientChangesEnded(MailClientService* service) {}

  virtual void OnMigrationProgress(MailClientService* service,
                                   int progress,
                                   int total,
                                   std::string msg) {}

  virtual void OnDeleteMessagesProgress(MailClientService* service,
                                        int delete_progress_count) {}

  // Invoked from the destructor of the MailClientService.
  virtual void OnMailClientModelBeingDeleted(MailClientService* service) {}

  virtual void OnMailClientServiceLoaded(MailClientService* service) {}

 protected:
  ~MailClientModelObserver() override {}
};

}  // namespace mail_client

#endif  //  COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_MODEL_OBSERVER_H_
