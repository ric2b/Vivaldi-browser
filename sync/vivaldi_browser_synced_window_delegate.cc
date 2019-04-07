// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/vivaldi_browser_synced_window_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/core/session_id.h"
#include "ui/vivaldi_browser_window.h"

VivaldiBrowserSyncedWindowDelegate::VivaldiBrowserSyncedWindowDelegate(Browser* browser)
    : BrowserSyncedWindowDelegate(browser), browser_copy_(browser) {}

VivaldiBrowserSyncedWindowDelegate::~VivaldiBrowserSyncedWindowDelegate() {}

sync_sessions::SyncedTabDelegate* VivaldiBrowserSyncedWindowDelegate::GetTabAt(
    int index) const {
  sync_sessions::SyncedTabDelegate* delegate =
      BrowserSyncedWindowDelegate::GetTabAt(index);
  if (!delegate) {
    VivaldiBrowserWindow* window =
        static_cast<VivaldiBrowserWindow*>(browser_copy_->window());
    LOG(ERROR) << "BrowserSyncedWindowDelegate found no SyncedTabDelegate for "
                  "tab position "
               << index << ", meaning this will crash now.";
    LOG(ERROR) << "Window had title: " << window->GetTitle();
    LOG(ERROR) << "Window top level url: " << window->web_contents()->GetURL().spec();

    content::WebContents* contents =
        browser_copy_->tab_strip_model()->GetWebContentsAt(index);
    if (!contents) {
      LOG(ERROR) << "WebContents for given index was a nullptr. tab strip has "
                 << browser_copy_->tab_strip_model()->count() << " entries.";
    }
  }
  return delegate;
}

SessionID VivaldiBrowserSyncedWindowDelegate::GetTabIdAt(int index) const {
  return GetTabAt(index)->GetSessionId();
}
