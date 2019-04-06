// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "content/browser/browser_plugin/browser_plugin_embedder.h"

#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_plugin_guest_manager.h"

namespace content {

bool BrowserPluginEmbedder::AreAnyGuestsFocused() {
  BrowserPluginGuestManager::GuestCallback callback =
      base::Bind([](WebContents* guest) {
        return static_cast<WebContentsImpl*>(guest)
            ->GetBrowserPluginGuest()
            ->focused();
      });
  return GetBrowserPluginGuestManager()->ForEachGuest(web_contents(), callback);
}

}  // namespace content
