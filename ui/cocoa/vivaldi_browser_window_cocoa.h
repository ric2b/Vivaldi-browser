// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef UI_COCOA_VIVALDI_BROWSER_WINDOW_COCOA_H_
#define UI_COCOA_VIVALDI_BROWSER_WINDOW_COCOA_H_

#include "ui/vivaldi_browser_window.h"
#include "base/mac/scoped_nsobject.h"

@class VivaldiBrowserWindowController;

// Overriding as little as possible from VivaldiBrowserWindow for it to work in
// Cocoa

class VivaldiBrowserWindowCocoa : public VivaldiBrowserWindow {
 public:
  VivaldiBrowserWindowCocoa();
  ~VivaldiBrowserWindowCocoa() override;

  // Overridden from VivaldiBrowserWindow
  void VivaldiShowWebsiteSettingsAt(
      Profile* profile,
      content::WebContents* web_contents,
      const GURL& url,
      const security_state::SecurityInfo& security_info,
      gfx::Point anchor) override;
  void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  void Show() override;

 private:
};

#endif  // UI_COCOA_VIVALDI_BROWSER_WINDOW_COCOA_H_
