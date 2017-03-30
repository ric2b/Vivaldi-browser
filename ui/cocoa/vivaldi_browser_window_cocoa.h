// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef UI_COCOA_VIVALDI_BROWSER_WINDOW_COCOA_H_
#define UI_COCOA_VIVALDI_BROWSER_WINDOW_COCOA_H_

#include "ui/vivaldi_browser_window.h"

@class BrowserWindowController;

// Overriding as little as possible from VivaldiBrowserWindow for it to work in
// Cocoa

class VivaldiBrowserWindowCocoa : public VivaldiBrowserWindow {
 public:
  VivaldiBrowserWindowCocoa(Browser* browser,
                            BrowserWindowController* controller);
  ~VivaldiBrowserWindowCocoa() override;

  // Overridden from VivaldiBrowserWindow
  void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadClosePreventionType dialog_type,
      bool app_modal,
      const base::Callback<void(bool)>& callback) override;
  void VivaldiShowWebsiteSettingsAt(
      Profile* profile,
      content::WebContents* web_contents,
      const GURL& url,
      const security_state::SecurityStateModel::SecurityInfo& security_info,
      gfx::Point anchor) override;
  void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
};

#endif  // UI_COCOA_VIVALDI_BROWSER_WINDOW_COCOA_H_
