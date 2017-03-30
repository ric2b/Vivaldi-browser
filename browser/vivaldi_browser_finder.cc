// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "browser/vivaldi_browser_finder.h"

#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/guest_view/browser/guest_view_base.h"

using content::WebContents;

namespace vivaldi {

Browser* FindBrowserWithWebContents(WebContents* web_contents) {
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
        extensions::AppWindowRegistry* registry =
          extensions::AppWindowRegistry::Get(browser_context);
        extensions::AppWindow* app_window =
          registry ? registry->GetAppWindowForWebContents(embedder_web_contents)
                   : nullptr;
        if (app_window) {
          browser =
            chrome::FindBrowserWithWindow(app_window->GetNativeWindow());
        }
      }
    }
  }
  return browser;
}

}  // namespace vivaldi
