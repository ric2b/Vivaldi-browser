// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_VIVALDI_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_VIVALDI_

#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"

@interface BrowserViewController(Vivaldi)

- (void)showNotesManager:(Browser*)browser
          parentController:(BrowserViewController*)bvc;
#if !defined(__IPHONE_16_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_16_0
- (void)onCopyToNote:(UIMenuController*)sender;
#endif
- (void)dismissNoteController;
- (void)shutdownNoteController;
- (void)dismissNoteSnackbar;
- (void)dismissNoteModalControllerAnimated;

@end
#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_VIVALDI_
