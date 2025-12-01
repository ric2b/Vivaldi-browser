// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note:(prio@vivaldi.com) - It is a chromium implementation which we moved
// to Vivaldi module to reduce the patch on Chromium git.
// This file is more heavily changed hence no // Vivaldi mark is added
// for the patches.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_STRIP_UI_VIVALDI_TAB_STRIP_GROUP_STROKE_VIEW_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_STRIP_UI_VIVALDI_TAB_STRIP_GROUP_STROKE_VIEW_H_

#import <UIKit/UIKit.h>

// A view that displays a group stroke indicator in a tab strip cell.
// Components:
// 1. A horizontal line from leading to trailing edge.
// 2. A custom path at the top (e.g., arc or curve).
// 3. A custom path at the bottom.
// 4. Optional corner decorations (left/right).
// Hidden by default.
@interface VivaldiTabStripGroupStrokeView : UIView

// Sets the stroke color for all visible elements.
- (void)setStrokeColor:(UIColor*)color;

// Sets the top decorative path (e.g., arc or shape).
- (void)setTopPath:(CGPathRef)path;

// Sets the bottom decorative path.
- (void)setBottomPath:(CGPathRef)path;

// Sets the path for the left corner decoration.
- (void)setLeftCornerPath:(CGPathRef)path;

// Toggles fill for the left corner.
- (void)setLeftCornerFill:(BOOL)fillLeftCorner;

// Sets the path for the right corner decoration.
- (void)setRightCornerPath:(CGPathRef)path;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_STRIP_UI_VIVALDI_TAB_STRIP_GROUP_STROKE_VIEW_H_
