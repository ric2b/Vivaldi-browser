// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_VIVALDI_BROWSER_SYNCED_WINDOW_DELEGATE_H_
#define SYNC_VIVALDI_BROWSER_SYNCED_WINDOW_DELEGATE_H_

#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"
#include "components/sessions/core/session_id.h"

class Browser;

namespace browser_sync {
class SyncedTabDelegate;
}

class VivaldiBrowserSyncedWindowDelegate : public BrowserSyncedWindowDelegate {
 public:
  explicit VivaldiBrowserSyncedWindowDelegate(Browser* browser);
  ~VivaldiBrowserSyncedWindowDelegate() override;
  VivaldiBrowserSyncedWindowDelegate(
      const VivaldiBrowserSyncedWindowDelegate&) = delete;
  VivaldiBrowserSyncedWindowDelegate& operator=(
      const VivaldiBrowserSyncedWindowDelegate&) = delete;

  // BrowserSyncedWindowDelegate:
  sync_sessions::SyncedTabDelegate* GetTabAt(int index) const override;
  SessionID GetTabIdAt(int index) const override;

 private:
  const raw_ptr<Browser> browser_copy_ = nullptr;  // Already private member in base
};

#endif  // SYNC_VIVALDI_BROWSER_SYNCED_WINDOW_DELEGATE_H_
