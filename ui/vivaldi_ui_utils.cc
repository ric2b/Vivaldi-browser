// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_ui_utils.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#if defined(OS_WIN) || defined(OS_LINUX)
#include "ui/vivaldi_browser_window.h"
#endif // OS_WIN || OS_LINUX
#include "ui/views/widget/widget.h"


namespace vivaldi {

namespace ui_tools {

/*static*/
extensions::WebViewGuest* GetActiveWebViewGuest() {

  Browser *browser = chrome::FindLastActive();

  if (!browser)
    return nullptr;

  return GetActiveWebGuestFromBrowser(browser);
}

/*static*/
extensions::WebViewGuest *
GetActiveWebViewGuest(extensions::NativeAppWindow *app_window) {

  Browser *browser =
      chrome::FindBrowserWithWindow(app_window->GetNativeWindow());

  if (!browser)
    return nullptr;

  return GetActiveWebGuestFromBrowser(browser);
}

/*static*/
extensions::WebViewGuest* GetActiveWebGuestFromBrowser(Browser* browser) {
  content::WebContents* active_web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!active_web_contents)
    return nullptr;

  return extensions::WebViewGuest::FromWebContents(active_web_contents);
}

/*static*/
extensions::AppWindow* GetActiveAppWindow() {
#if defined(OS_WIN) || defined(OS_LINUX)
  Browser* browser = chrome::FindLastActive();
  if (browser && browser->is_vivaldi())
    return static_cast<const VivaldiBrowserWindow *>(browser->window())
        ->GetAppWindow();
#endif

  return nullptr;
}

} // namespace ui_tools

} // namespace vivaldi