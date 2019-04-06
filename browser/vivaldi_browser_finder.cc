// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "browser/vivaldi_browser_finder.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/vivaldi_browser_window.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#endif

using content::WebContents;

namespace vivaldi {

Browser* FindBrowserForEmbedderWebContents(const WebContents* contents) {
  for (auto* browser : *BrowserList::GetInstance()) {
    // Devtools windows are not vivaldi windows, so exclude those.
    if (browser->is_vivaldi() &&
        static_cast<VivaldiBrowserWindow*>(browser->window())->web_contents() ==
            contents) {
      return browser;
    }
  }
  return nullptr;
}

Browser* FindBrowserWithWebContents(WebContents* web_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);

  // NOTE(espen@vivaldi.com): Some elements (e.g., within panels) will not match
  // in the function above. We have to find the window that contains the web
  // content and use that information to look up the browser.
  if (!browser) {
    guest_view::GuestViewBase* gvb =
        guest_view::GuestViewBase::FromWebContents(web_contents);
    if (gvb) {
      WebContents* embedder_web_contents = gvb->embedder_web_contents();
      content::BrowserContext* browser_context = gvb->browser_context();
      if (embedder_web_contents && browser_context) {
        browser = FindBrowserForEmbedderWebContents(embedder_web_contents);
      }
    }
  }

  return browser;
#else
  return NULL;
#endif
}

}  // namespace vivaldi
