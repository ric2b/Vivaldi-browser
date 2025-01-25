// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#import <UIKit/UIKit.h>

@class VivaldiPageZoomViewController;

@interface UIWindow (PageZoom)

// Property to hold a reference to the VivaldiPageZoomViewController
@property (nonatomic, strong) VivaldiPageZoomViewController *viewController;

// Show the page zoom view controller
- (void)showPageZoomViewController:
    (VivaldiPageZoomViewController *)viewController;

// Hide the currently displayed page zoom view controller
- (void)hidePageZoomViewController;

@end
