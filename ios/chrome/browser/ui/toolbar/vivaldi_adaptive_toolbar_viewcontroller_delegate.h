// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_VIVALDI_ADAPTIVE_TOOLBAR_VIEWCONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_VIVALDI_ADAPTIVE_TOOLBAR_VIEWCONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol VivaldiAdaptiveToolbarViewControllerDelegate<NSObject>

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_VIVALDI_ADAPTIVE_TOOLBAR_VIEWCONTROLLER_DELEGATE_H_
