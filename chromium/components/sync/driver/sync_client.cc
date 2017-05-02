// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_client.h"

namespace syncer {

SyncClient::SyncClient() {}
SyncClient::~SyncClient() {}

Profile *SyncClient::GetProfile() {
  return nullptr;
}

vivaldi::Notes_Model* SyncClient::GetNotesModel() {
  return nullptr;
}

}  // namespace syncer
