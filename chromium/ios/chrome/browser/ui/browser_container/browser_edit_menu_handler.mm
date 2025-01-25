// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_edit_menu_handler.h"

#import "ios/chrome/browser/link_to_text/ui_bundled/link_to_text_delegate.h"
#import "ios/chrome/browser/ui/partial_translate/partial_translate_delegate.h"
#import "ios/chrome/browser/ui/search_with/search_with_delegate.h"

#if defined(VIVALDI_BUILD)
#import "app/vivaldi_apptools.h"
#import "ios/ui/copy_to_note/copy_to_note_delegate.h"
#endif // End Vivaldi

@implementation BrowserEditMenuHandler

- (void)buildEditMenuWithBuilder:(id<UIMenuBuilder>)builder {

  // Vivaldi
#if defined(__IPHONE_16_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_0
  if (@available(iOS 16, *)) {
    if (vivaldi::IsVivaldiRunning()) {
      [self.vivaldiCopyToNoteDelegate buildMenuWithBuilder:builder];
    }
  }
#endif // End Vivaldi

  [self.linkToTextDelegate buildMenuWithBuilder:builder];
  [self.searchWithDelegate buildMenuWithBuilder:builder];
  [self.partialTranslateDelegate buildMenuWithBuilder:builder];
}

@end
