// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_container_delegate.h"

// A view to hold the top menu items of the start page.
@interface VivaldiSpeedDialContainerView : UIView

// INITIALIZERS
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// DELEGATE
@property (nonatomic, weak) id<VivaldiSpeedDialContainerDelegate> delegate;

// SETTERS
- (void)configureWith:(NSArray*)speedDials
               parent:(VivaldiSpeedDialItem*)parent
        faviconLoader:(FaviconLoader*)faviconLoader
    deviceOrientation:(BOOL)isLandscape;

- (void)setDeviceOrientation:(BOOL)isLandscape;

@end

#endif  // IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_CONTAINER_VIEW_H_
