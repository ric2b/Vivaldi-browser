// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_PANEL_PANEL_TRANSITIONING_DELEGATE_H_
#define IOS_PANEL_PANEL_TRANSITIONING_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"

@interface PanelTransitioningDelegate
    : NSObject<UIViewControllerTransitioningDelegate>

@property(nonatomic, assign) ToolbarType toolbarType;

@end

#endif  // IOS_PANEL_PANEL_TRANSITIONING_DELEGATE_H_
