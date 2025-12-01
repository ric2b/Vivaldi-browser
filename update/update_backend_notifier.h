// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_UPDATE_BACKEND_NOTIFIER_H_
#define UPDATE_UPDATE_BACKEND_NOTIFIER_H_

#include "extensions/api/auto_update/auto_update_status.h"

namespace update {

// The UpdateBackendNotifier forwards notifications from the UpdateBackend's
// client to all the interested observers (in both update and main thread).
class UpdateBackendNotifier {
 public:
  virtual ~UpdateBackendNotifier() = default;

  // Sends progress information

  virtual void NotifyUpdateProgress(const AutoUpdateStatus& status,
                                    const std::string& reason,
                                    const int progress) = 0;

 protected:
  UpdateBackendNotifier() = default;
};
}  // namespace update

#endif  // UPDATE_UPDATE_BACKEND_NOTIFIER_H_
