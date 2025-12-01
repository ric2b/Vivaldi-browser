// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: (Vivaldi) Renamed, This file is a copy of tabs_closure_animation.h
// (praveen@vivaldi.com) copied from chromium codebase.
// Tweaked to fit the reader mode animation & conflicts.
// Rest of the code is original from chromium codebase.


#ifndef IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_VIEW_ANIMATION_H_
#define IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_VIEW_ANIMATION_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// Available types of tab closure animation.
enum class VivaldiReaderModeAnimationType {
  kHideView,
  kRevealView,
};

// Creates and triggers the tab closure animation.
@interface VivaldiReaderModeAnimation : NSObject

// Type of animation. Defaults to `kHideView`.
@property(nonatomic, assign) VivaldiReaderModeAnimationType type;
// Start point of animation in unit coordinate space. Defaults to (0.5, 1.0).
@property(nonatomic, assign) CGPoint startPoint;

// Vivaldi: Base color used for the wipe effect.
// If nil, a default system color is used.
@property(nonatomic, strong) UIColor* wipeColor;

- (instancetype)initWithWindow:(UIView*)window
                     gridCells:(NSArray<UIView*>*)gridCells
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Animates the view in `view` with a "wipe" effect on top of
// `window`.
- (void)animateWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_VIEW_ANIMATION_H_
