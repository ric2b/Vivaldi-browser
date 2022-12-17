// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/helpers/vivaldi_uiview_style_helper.h"
#import <UIKit/UIKit.h>

@implementation UIView(VivaldiStyle)
#pragma mark:- SETTERS

// Applies shadow to the view
- (void)addShadowWithBackground:(UIColor*)backgroundColor
                         offset:(CGSize)offset
                    shadowColor:(UIColor*)shadowColor
                         radius:(CGFloat)radius
                        opacity:(CGFloat)opacity {
  self.layer.masksToBounds = NO;
  self.layer.shadowOffset = offset;
  self.layer.shadowColor = shadowColor.CGColor;
  self.layer.shadowRadius = radius;
  self.layer.shadowOpacity = opacity;

  self.backgroundColor = nil;
  self.layer.backgroundColor =  backgroundColor.CGColor;
}

@end
