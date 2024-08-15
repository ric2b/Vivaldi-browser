// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/start_page/quick_settings/vivaldi_start_page_quick_settings_wallpaper_vc.h"

#import "ios/ui/settings/start_page/wallpaper_settings/wallpaper_settings_swift.h"

@implementation VivaldiStartPageQuickSettingsWallpaperVC

- (void)loadView {
  [super loadView];

  // Create the child view controller
  UIViewController *childViewController =
      [VivaldiWallpaperSettingsViewProvider
          makeViewControllerWithHorizontalLayout:YES];

  // Add the child view controller to the parent (self)
  [self addChildViewController:childViewController];
  [self.view addSubview:childViewController.view];

  childViewController.view.backgroundColor = UIColor.clearColor;
  childViewController.view.frame = self.view.bounds;
  childViewController.view.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [childViewController didMoveToParentViewController:self];
}

@end
