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

VivaldiBrowserSyncedWindowDelegate::VivaldiBrowserSyncedWindowDelegate(
    Browser* browser)
    : BrowserSyncedWindowDelegate(browser), browser_copy_(browser) {}

VivaldiBrowserSyncedWindowDelegate::~VivaldiBrowserSyncedWindowDelegate() {}

sync_sessions::SyncedTabDelegate* VivaldiBrowserSyncedWindowDelegate::GetTabAt(
    int index) const {
  // Inline BrowserSyncedWindowDelegate::GetTabAt(index) to avoid crash on
  // null WebContents.
  content::WebContents* contents =
      browser_copy_->tab_strip_model()->GetWebContentsAt(index);
  if (contents) {
    sync_sessions::SyncedTabDelegate* delegate =
        BrowserSyncedTabDelegate::FromWebContents(contents);
    if (delegate)
      return delegate;
  }

  VivaldiBrowserWindow* window =
      static_cast<VivaldiBrowserWindow*>(browser_copy_->window());
  LOG(ERROR) << "BrowserSyncedWindowDelegate found no SyncedTabDelegate for "
                "tab position "
             << index << " with " << browser_copy_->tab_strip_model()->count()
             << " entries in tab strip. This may lead to a crash.";
  if (!contents) {
    LOG(ERROR) << "WebContents for given index was a nullptr.";
  }
  LOG(ERROR) << "Window had title: " << window->GetTitle();
  LOG(ERROR) << "Window top level url: "
             << window->web_contents()->GetURL().spec();

  return nullptr;
}

SessionID VivaldiBrowserSyncedWindowDelegate::GetTabIdAt(int index) const {
  // On null delegate return an invalid session id avoiding a crash when the
  // code is called from LocalSessionEventHandlerImpl::AssociateWindows under
  // WebContentsImpl destructor, see VB-43254.
  // TODO(igor@vivaldi.com): find out the condition leading to this.
  sync_sessions::SyncedTabDelegate* delegate = GetTabAt(index);
  if (!delegate)
    return SessionID::InvalidValue();
  return delegate->GetSessionId();
}
