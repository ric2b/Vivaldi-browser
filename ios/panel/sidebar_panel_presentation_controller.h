// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_PANEL_SIDEBAR_PANEL_PRESENTATION_CONTROLLER_H_
#define IOS_PANEL_SIDEBAR_PANEL_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"

@interface SidebarPanelPresentationController
    : UIPresentationController

- (instancetype)initWithPresentedViewController:
                (UIViewController*)presentedViewController
                       presentingViewController:
                (UIViewController*)presentingViewController;

@property(nonatomic, assign) ToolbarType toolbarType;

@end

#endif  // IOS_PANEL_SIDEBAR_PANEL_PRESENTATION_CONTROLLER_H_
