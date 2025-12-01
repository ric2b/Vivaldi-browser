// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
// End Vivaldi

@implementation TableViewNavigationController

#pragma mark - Public Interface

- (instancetype)initWithTable:(UIViewController*)table {
  return [super initWithRootViewController:table];
}

#pragma mark - View Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];

  if (vivaldi::IsVivaldiRunning()) {
    self.toolbar.translucent = NO;
    self.navigationBar.translucent = NO;
  } // End Vivaldi

  self.navigationBar.prefersLargeTitles = YES;

  self.navigationBar.barTintColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.toolbar.barTintColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}

@end
