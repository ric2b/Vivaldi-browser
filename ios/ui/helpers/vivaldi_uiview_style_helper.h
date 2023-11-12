// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_HELPERS_VIVALDI_UIVIEW_STYLE_HELPER_H_
#define IOS_UI_HELPERS_VIVALDI_UIVIEW_STYLE_HELPER_H_

#import <UIKit/UIKit.h>

@interface UIView(VivaldiStyle)
#pragma mark:- SETTERS
/// Applies shadow to the view
- (void)addShadowWithBackground:(UIColor*)backgroundColor
                         offset:(CGSize)offset
                    shadowColor:(UIColor*)shadowColor
                         radius:(CGFloat)radius
                        opacity:(CGFloat)opacity;

@end
#endif  // IOS_CHROME_BROWSER_UI_STYLE_HELPER_H_
