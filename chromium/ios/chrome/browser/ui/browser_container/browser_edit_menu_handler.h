// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_EDIT_MENU_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_EDIT_MENU_HANDLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/browser_container/edit_menu_builder.h"

@protocol LinkToTextDelegate;
@protocol PartialTranslateDelegate;
@protocol SearchWithDelegate;

#if defined(VIVALDI_BUILD)
@protocol CopyToNoteDelegate;
#endif // End Vivaldi

// A handler for the Browser edit menu.
// This class is in charge of customising the menu and executing the commands.
@interface BrowserEditMenuHandler : NSObject <EditMenuBuilder>

// The delegate to handle link to text button selection.
@property(nonatomic, weak) id<LinkToTextDelegate> linkToTextDelegate;

// The delegate to handle Partial Translate button selection.
@property(nonatomic, weak) id<PartialTranslateDelegate>
    partialTranslateDelegate;

// The delegate to handle Search With button selection.
@property(nonatomic, weak) id<SearchWithDelegate> searchWithDelegate;

// Will be called to customize edit menus.
- (void)buildEditMenuWithBuilder:(id<UIMenuBuilder>)builder;

#if defined(VIVALDI_BUILD)
@property(nonatomic, weak) id<CopyToNoteDelegate> vivaldiCopyToNoteDelegate;
#endif // End Vivaldi

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_EDIT_MENU_HANDLER_H_
