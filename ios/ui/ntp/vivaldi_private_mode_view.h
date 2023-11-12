// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_PRIVATE_MODE_VIEW_H_
#define IOS_UI_NTP_VIVALDI_PRIVATE_MODE_VIEW_H_

#import <UIKit/UIKit.h>

@protocol NewTabPageURLLoaderDelegate;

// A view to hold the top menu items of the start page.
@interface VivaldiPrivateModeView : UIScrollView

// INITIALIZERS
- (instancetype)initWithFrame:(CGRect)frame
    showTopIncognitoImageAndTitle:(BOOL)showTopIncognitoImageAndTitle
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Delegate to load urls in the current tab.
@property(nonatomic, weak) id<NewTabPageURLLoaderDelegate> URLLoaderDelegate;

@end

#endif  // IOS_UI_NTP_VIVALDI_PRIVATE_MODE_VIEW_H_
