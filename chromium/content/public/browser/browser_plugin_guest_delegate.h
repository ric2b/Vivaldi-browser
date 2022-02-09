// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_

#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"

namespace content {

class GuestHost;
class BrowserPluginGuest;

// Objects implement this interface to get notified about changes in the guest
// WebContents and to provide necessary functionality.
class CONTENT_EXPORT BrowserPluginGuestDelegate {
 public:
  virtual ~BrowserPluginGuestDelegate() {}

  virtual WebContents* CreateNewGuestWindow(
      const WebContents::CreateParams& create_params);

  // Returns the WebContents that currently owns this guest.
  virtual WebContents* GetOwnerWebContents();

  // Provides the delegate with an interface with which to communicate with the
  // content module.
  virtual void SetGuestHost(GuestHost* guest_host) {}

  // NOTE(andre@vivaldi.com):
  // It is always set for tab and inspected webviews that might move between
  // embedders. Used to reset guest_host_ in between hand-overs. Ie. move
  // between docked/un-docked devtools.
  BrowserPluginGuest* delegate_to_browser_plugin_ = nullptr;

  // NOTE(andre@vivaldi.com):
  // Helper to create and initialize a BrowserPluginGuest for a webcontents
  // already created.
  virtual void CreatePluginGuest(WebContents* contents);

};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
