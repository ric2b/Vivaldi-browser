// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.

#ifndef UI_VIVALDI_UI_UTILS_H_
#define UI_VIVALDI_UI_UTILS_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "extensions/common/api/extension_types.h"

namespace extensions {
class WebViewGuest;
}

class VivaldiBrowserWindow;

namespace vivaldi {
namespace ui_tools {

// Returns the currently active WebViewGuest.
extern extensions::WebViewGuest* GetActiveWebViewGuest();

// Returns the active BrowserWindow, currently used by progress updates to the
// taskbar on Windows.
extern VivaldiBrowserWindow* GetActiveAppWindow();

// Return the most recently active main window excluding settings and debug
// windows.
extern VivaldiBrowserWindow* GetLastActiveMainWindow();

extern extensions::WebViewGuest* GetActiveWebGuestFromBrowser(Browser* browser);

extern content::WebContents* GetWebContentsFromTabStrip(
    int tab_id,
    content::BrowserContext* browser_context,
    std::string* error = nullptr);

extern bool IsOutsideAppWindow(int screen_x, int screen_y);

extern Browser* FindBrowserForPersistentTabs(Browser* current_browser);
extern bool MoveTabToWindow(Browser* source_browser,
                            Browser* target_browser,
                            int tab_index,
                            int* new_index,
                            int iteration,
                            int add_types);
extern bool GetTabById(int tab_id, content::WebContents** contents, int* index);

// Detects if the current thread can show UI elements. Used to detect if we can safely
// display Dialog boxes in case the code may run before main window shows up.
extern bool IsUIAvailable();

}  // namespace ui_tools
}  // namespace vivaldi

#endif  // UI_VIVALDI_UI_UTILS_H_
