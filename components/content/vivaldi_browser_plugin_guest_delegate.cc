// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "content/public/browser/browser_plugin_guest_delegate.h"

#include "content/browser/browser_plugin/browser_plugin_guest.h"  // nogncheck
#include "content/browser/web_contents/web_contents_impl.h"       // nogncheck

namespace content {

void BrowserPluginGuestDelegate::CreatePluginGuest(
  WebContents* contents) {
  content::WebContentsImpl* contentsimpl =
    static_cast<content::WebContentsImpl*>(contents);

  content::BrowserPluginGuest::CreateInWebContents(contentsimpl, this);
  contentsimpl->GetBrowserPluginGuest()->Init();
}

}  // namespace content
