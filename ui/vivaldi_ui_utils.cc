// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_ui_utils.h"

#include <string>
#include <vector>

#include "app/vivaldi_constants.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "ui/base/base_window.h"
#include "ui/views/widget/widget.h"
#include "ui/vivaldi_browser_window.h"

#include "base/task/current_thread.h"
#include "base/run_loop.h"
#include "ui/base/resource/resource_bundle.h"

namespace vivaldi {
namespace ui_tools {

namespace {

bool IsMainVivaldiBrowserWindow(VivaldiBrowserWindow* window) {
  DCHECK(window);
  // Don't track popup windows (like settings) in the session.
  return !window->browser()->is_type_popup();
}

}  // namespace

extensions::WebViewGuest* GetActiveWebViewGuest() {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return nullptr;

  return GetActiveWebGuestFromBrowser(browser);
}

extensions::WebViewGuest* GetActiveWebGuestFromBrowser(Browser* browser) {
  content::WebContents* active_web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!active_web_contents)
    return nullptr;

  return extensions::WebViewGuest::FromWebContents(active_web_contents);
}

VivaldiBrowserWindow* GetActiveAppWindow() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  Browser* browser = chrome::FindLastActive();
  if (browser && browser->is_vivaldi())
    return static_cast<VivaldiBrowserWindow*>(browser->window());
#endif
  return nullptr;
}

VivaldiBrowserWindow* GetLastActiveMainWindow() {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (auto i = browser_list->begin_browsers_ordered_by_activation();
       i != browser_list->end_browsers_ordered_by_activation(); ++i) {
    VivaldiBrowserWindow* window = VivaldiBrowserWindow::FromBrowser(*i);
    if (window && IsMainVivaldiBrowserWindow(window))
      return window;
  }
  return nullptr;
}

content::WebContents* GetWebContentsFromTabStrip(
    int tab_id,
    content::BrowserContext* browser_context,
    std::string* error) {
  content::WebContents* contents = nullptr;
  bool include_incognito = true;
  extensions::WindowController* browser;
  int tab_index;
  extensions::ExtensionTabUtil::GetTabById(tab_id, browser_context,
                                           include_incognito, &browser,
                                           &contents, &tab_index);
  if (error && !contents) {
    *error = "Failed to find a tab with id " + std::to_string(tab_id);
  }
  return contents;
}

bool IsOutsideAppWindow(int screen_x, int screen_y) {
  gfx::Point screen_point(screen_x, screen_y);

  bool outside = true;
  for (Browser* browser : *BrowserList::GetInstance()) {
    gfx::Rect rect = browser->is_type_devtools()
                         ? gfx::Rect()
                         : browser->window()->GetBounds();
    if (rect.Contains(screen_point)) {
      outside = false;
      break;
    }
  }
  return outside;
}

Browser* FindBrowserForPersistentTabs(Browser* current_browser) {
  if (browser_shutdown::IsTryingToQuit() ||
      browser_shutdown::GetShutdownType() !=
          browser_shutdown::ShutdownType::kNotValid) {
    // Do not move anything on shutdown
    return nullptr;
  }
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser == current_browser) {
      continue;
    }
    VivaldiBrowserWindow* window = VivaldiBrowserWindow::FromBrowser(browser);
    if (!window || !IsMainVivaldiBrowserWindow(window)) {
      continue;
    }
    if (browser->type() != current_browser->type()) {
      continue;
    }
    if (browser->is_type_devtools()) {
      continue;
    }
    // Only move within the same profile.
    if (current_browser->profile() != browser->profile()) {
      continue;
    }
    if (browser->tab_strip_model()->empty() ||
        browser->tab_strip_model()->closing_all()) {
      // The browser window is not yet fully initialized or is about to close.
      continue;
    }
    return browser;
  }
  return nullptr;
}

// Based on TabsMoveFunction::MoveTab() but greatly simplified.
bool MoveTabToWindow(Browser* source_browser,
                     Browser* target_browser,
                     int tab_index,
                     int* new_index,
                     int iteration,
                     int add_types) {
  DCHECK(source_browser != target_browser);

  // Insert the tabs one after another.
  *new_index += iteration;

  TabStripModel* source_tab_strip = source_browser->tab_strip_model();
  std::unique_ptr<tabs::TabModel> tab =
      source_tab_strip->DetachTabAtForInsertion(tab_index);
  if (!tab) {
    return false;
  }
  TabStripModel* target_tab_strip = target_browser->tab_strip_model();

  // Clamp move location to the last position.
  // This is ">" because it can append to a new index position.
  // -1 means set the move location to the last position.
  if (*new_index > target_tab_strip->count() || *new_index < 0)
    *new_index = target_tab_strip->count();

  target_tab_strip->InsertDetachedTabAt(*new_index, std::move(tab),
                                        add_types);

  return true;
}

bool GetTabById(int tab_id, content::WebContents** contents, int* index) {
  for (Browser* target_browser : *BrowserList::GetInstance()) {
    TabStripModel* target_tab_strip = target_browser->tab_strip_model();
    for (int i = 0; i < target_tab_strip->count(); ++i) {
      content::WebContents* target_contents =
          target_tab_strip->GetWebContentsAt(i);
      if (sessions::SessionTabHelper::IdForTab(target_contents).id() ==
          tab_id) {
        if (contents)
          *contents = target_contents;
        if (index)
          *index = i;
        return true;
      }
    }
  }
  return false;
}

bool IsUIAvailable() {
  return base::CurrentUIThread::IsSet() &&
         base::RunLoop::IsRunningOnCurrentThread() &&
         ui::ResourceBundle::HasSharedInstance();
}

}  // namespace ui_tools
}  // namespace vivaldi
