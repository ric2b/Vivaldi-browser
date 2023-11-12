// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_PANEL_SIDEBAR_PANEL_PRESENTATION_CONTROLLER_H_
#define IOS_PANEL_SIDEBAR_PANEL_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@interface SidebarPanelPresentationController
    : UIPresentationController

- (instancetype)initWithPresentedViewController:
                (UIViewController*)presentedViewController
                       presentingViewController:
                (UIViewController*)presentingViewController;

@end

#endif  // IOS_PANEL_SIDEBAR_PANEL_PRESENTATION_CONTROLLER_H_
