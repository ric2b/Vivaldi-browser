// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_plugin_guest_delegate.h"

#include "content/browser/browser_plugin/browser_plugin_guest.h"  // nogncheck
#include "content/browser/web_contents/web_contents_impl.h"       // nogncheck

namespace content {

WebContents* BrowserPluginGuestDelegate::CreateNewGuestWindow(
    const WebContents::CreateParams& create_params) {
  NOTREACHED();
  return nullptr;
}

WebContents* BrowserPluginGuestDelegate::GetOwnerWebContents() {
  return nullptr;
}

void BrowserPluginGuestDelegate::CreatePluginGuest(
    WebContents* contents) {
  content::WebContentsImpl* contentsimpl =
      static_cast<content::WebContentsImpl*>(contents);

  content::BrowserPluginGuest::CreateInWebContents(contentsimpl, this);
  contentsimpl->GetBrowserPluginGuest()->Init();
  contentsimpl->GetBrowserPluginGuest()->set_allow_blocked_by_client();
}

}  // namespace content
