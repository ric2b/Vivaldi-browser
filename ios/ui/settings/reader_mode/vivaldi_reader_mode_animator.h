// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_ANIMATOR_H_
#define IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_ANIMATOR_H_

#import <UIKit/UIKit.h>

@interface VivaldiReaderModeAnimator : NSObject
// 1) Captures a snapshot of `contentContainerView` BEFORE applying changes
// 2) Executes `toggler` to switch Reader Mode state
// 3) Animates hiding the snapshot to reveal updated content
+ (void)playTransitionOnContentContainerView:(UIView*)contentContainerView
                                 contentView:(UIView*)contentView
                                  startPoint:(CGPoint)startPoint
                                   wipeColor:(UIColor*)wipeColor
                                     toggler:(dispatch_block_t)toggler;
@end

#endif  // IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_ANIMATOR_H_
