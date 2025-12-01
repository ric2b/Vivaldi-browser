// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_animator.h"

#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_prefs_helper.h"
#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_animation.h"

namespace {
  const char kReaderViewAnimationColor[] = "reader_view_animation";
}

@implementation VivaldiReaderModeAnimator

// Captures a snapshot, toggles content, then wipes the snapshot away with color.
+ (void)playTransitionOnContentContainerView:(UIView*)contentContainerView
                                 contentView:(UIView*)contentView
                                  startPoint:(CGPoint)startPoint
                                   wipeColor:(UIColor*)wipeColor
                                     toggler:(dispatch_block_t)toggler {
  if (!contentContainerView || !contentView || ![NSThread isMainThread])
    return;

  // 1) Snapshot current content.
  UIView* snapshot = nil;
  if ([contentContainerView
        respondsToSelector:@selector(snapshotViewAfterScreenUpdates:)]) {
    snapshot = [contentContainerView snapshotViewAfterScreenUpdates:NO];
  }
  if (!snapshot) {
    UIGraphicsBeginImageContextWithOptions(
        contentContainerView.bounds.size, NO, UIScreen.mainScreen.scale);
    [contentContainerView.layer renderInContext:UIGraphicsGetCurrentContext()];
    UIImage* img = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    snapshot = [[UIImageView alloc] initWithImage:img];
    snapshot.frame = contentContainerView.bounds;
  }

  // 2) Apply the toggle (Reader Mode on/off) so new content is ready beneath.
  if (toggler) {
    toggler();
  }

  // Check if motion is reduced for accessibility
  if (UIAccessibilityIsReduceMotionEnabled()) {
    // Skip animation and just remove snapshot immediately
    [snapshot removeFromSuperview];
    return;
  }

  // 3) Animate hiding the snapshot to reveal new content.
  [self playHidingSnapshotOnView:contentContainerView
                        snapshot:snapshot
                      startPoint:startPoint
                       wipeColor:wipeColor];
}

// Hides the snapshot to reveal new content.
+ (void)playHidingSnapshotOnView:(UIView*)view
                        snapshot:(UIView*)snapshot
                      startPoint:(CGPoint)startPoint
                       wipeColor:(UIColor*)wipeColor {
  if (!view || !snapshot || ![NSThread isMainThread])
    return;

  // Insert snapshot above content; this mimics "captured previous view".
  snapshot.frame = view.bounds;
  snapshot.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [view addSubview:snapshot];

  // Non-interactive overlay to host the wipe animation.
  UIView* overlay = [[UIView alloc] initWithFrame:view.bounds];
  overlay.backgroundColor = [UIColor clearColor];
  overlay.userInteractionEnabled = NO;
  overlay.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [view addSubview:overlay];

  // Animation is a copy of tabs_closure_animation.h
  VivaldiReaderModeAnimation* anim =
    [[VivaldiReaderModeAnimation alloc] initWithWindow:overlay
                                              gridCells:@[ snapshot ]];
  // Hide the snapshot with the wipe, revealing new content underneath.
  anim.type = VivaldiReaderModeAnimationType::kHideView;
  anim.startPoint = startPoint;

   UIColor *fallbackColor = [UIColor colorNamed:@(kReaderViewAnimationColor)];

  if ([VivaldiAppearanceSettingsPrefsHelper dynamicAccentColorEnabled]) {
    // Use fallback color, if dynamic accent color enabled
    anim.wipeColor = fallbackColor;
  } else {
    /// dynamic accent color disabled
    NSString* accentHex =
        [VivaldiAppearanceSettingsPrefsHelper getCustomAccentColor];
    UIColor* accent =
        [VivaldiGlobalHelpers colorWithHexString:accentHex ?: @""];
    if (!accent) accent = fallbackColor;
    anim.wipeColor = accent;
  }

  __weak UIView* weakOverlay = overlay;
  __weak UIView* weakSnapshot = snapshot;
  [anim animateWithCompletion:^{
    [weakOverlay removeFromSuperview];
    [weakSnapshot removeFromSuperview];
  }];

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.9 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   if (overlay.superview) {
                     [overlay removeFromSuperview];
                   }
                   if (snapshot.superview) {
                     [snapshot removeFromSuperview];
                   }
                 });
}

@end
