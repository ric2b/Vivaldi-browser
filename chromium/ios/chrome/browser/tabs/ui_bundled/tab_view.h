// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_VIEW_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/tabs/ui_bundled/tab_view_delegate.h"

@protocol TabViewDelegate;

// View class that draws a Chrome-style tab.
@interface TabView : UIControl

@property(nonatomic, weak) id<TabViewDelegate> delegate;
@property(nonatomic, readonly, strong) UILabel* titleLabel;
@property(nonatomic, strong) UIImage* favicon;
@property(nonatomic, assign, getter=isCollapsed) BOOL collapsed;
@property(nonatomic, strong) UIImage* background;
@property(nonatomic, assign) BOOL incognitoStyle;

// Designated initializer.  Creates a TabView with frame equal to CGRectZero.
// If `emptyView` is YES, it creates a TabView without buttons or spinner.
// `selected`, the selected state of the tab, is provided to ensure the
// background is drawn correctly the first time, rather than requiring that
// -setSelected be called in order for it to be drawn correctly.
- (id)initWithEmptyView:(BOOL)emptyView selected:(BOOL)selected;

// Sets the title.
- (void)setTitle:(NSString*)title;
// Starts the progress spinner animation.
- (void)startProgressSpinner;
// Stops the progress spinner animation.
- (void)stopProgressSpinner;

// Vivaldi
- (id)initWithEmptyView:(BOOL)emptyView
             isSelected:(BOOL)isSelected
       bottomOmniboxEnabled:(BOOL)bottomOmniboxEnabled
  showXButtonBackgroundTabs:(BOOL)showXButton
                 themeColor:(UIColor*)themeColor
                  tintColor:(UIColor*)tintColor;
- (void)updateTabViewStyleWithBottomOmniboxEnabled:(BOOL)bottomOmniboxEnabled;
- (void)updateTabViewStyleWithThemeColor:(UIColor*)themeColor
                               tintColor:(UIColor*)tintColor
                              isSelected:(BOOL)isSelected
               showXButtonBackgroundTabs:(BOOL)showXButton;
- (void)updateTabViewWithXButtonVisibleBackgroundTabs:(BOOL)visible
                                           isSelected:(BOOL)isSelected;
// End Vivaldi

@end

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_VIEW_H_
