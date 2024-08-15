// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "browser/vivaldi_browser_finder.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/content/vivaldi_tab_check.h"
#include "ui/vivaldi_browser_window.h"

using content::WebContents;

namespace vivaldi {

Browser* FindBrowserForEmbedderWebContents(const WebContents* web_contents) {
  if (!web_contents)
    return nullptr;
  VivaldiBrowserWindow* window = FindWindowForEmbedderWebContents(web_contents);
  return window ? window->browser() : nullptr;
}

VivaldiBrowserWindow* FindWindowForEmbedderWebContents(
    const content::WebContents* web_contents) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    VivaldiBrowserWindow* window = VivaldiBrowserWindow::FromBrowser(browser);
    if (window && window->web_contents() == web_contents) {
      return window;
    }
  }
  return nullptr;
}

Browser* FindBrowserWithTab(const content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Browser* browser = chrome::FindBrowserWithTab(web_contents);

  // NOTE(espen@vivaldi.com): Some elements (e.g., within panels) will not match
  // in the function above. We have to find the window that contains the web
  // content and use that information to look up the browser.
  if (!browser) {
    return FindBrowserWithNonTabContent(web_contents);
  }

  return browser;
#else
  return nullptr;
#endif
}

Browser* FindBrowserWithNonTabContent(
    const content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  if (VivaldiTabCheck::IsOwnedByDevTools(
          const_cast<content::WebContents*>(web_contents))) {
    return nullptr;
  }

  Browser* browser = nullptr;
  guest_view::GuestViewBase* gvb = guest_view::GuestViewBase::FromWebContents(
      const_cast<content::WebContents*>(web_contents));
  if (gvb) {
    WebContents* embedder_web_contents = gvb->embedder_web_contents();
    content::BrowserContext* browser_context = gvb->browser_context();
    if (embedder_web_contents && browser_context) {
      browser = FindBrowserForEmbedderWebContents(embedder_web_contents);
    }
  }
  return browser;
}

Browser* FindBrowserByWindowId(SessionID::id_type window_id) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->session_id().id() == window_id) {
      return browser->window() ? browser : nullptr;
    }
  }
  return nullptr;
}

int GetBrowserCountOfType(Browser::Type type) {
  int count = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->type() == type) {
      count++;
    }
  }
  return count;
}

}  // namespace vivaldi
