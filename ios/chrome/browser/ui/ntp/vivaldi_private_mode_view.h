// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_VIVALDI_PRIVATE_MODE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_VIVALDI_PRIVATE_MODE_VIEW_H_

#import <UIKit/UIKit.h>

// A view to hold the top menu items of the start page.
@interface VivaldiPrivateModeView : UIView

// INITIALIZERS
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_VIVALDI_PRIVATE_MODE_VIEW_H_