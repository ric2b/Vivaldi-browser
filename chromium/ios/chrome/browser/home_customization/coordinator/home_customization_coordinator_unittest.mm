// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_coordinator+Testing.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_main_view_controller.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Tests for the Home Customization coordinator.
class HomeCustomizationCoordinatorUnitTest : public PlatformTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({kHomeCustomization}, {});

    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    base_view_controller_ = [[UIViewController alloc] init];

    coordinator_ = [[HomeCustomizationCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  HomeCustomizationCoordinator* coordinator_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
};

// Tests that the coordinator is successfully started and presents the right
// page, then stopped when it's dismissed.
TEST_F(HomeCustomizationCoordinatorUnitTest, TestPresentMenuPage) {
  // Test that the main VC and mediator are nil, indicating that the coordinator
  // has not been started.
  EXPECT_EQ(nil, coordinator_.mainViewController);
  EXPECT_EQ(nil, coordinator_.mediator);

  // Present the menu and check that the main VC and mediator exist, indicating
  // that the coordinator is started.
  [coordinator_ start];
  EXPECT_NE(nil, coordinator_.mainViewController);
  EXPECT_NE(nil, coordinator_.mediator);

  // Present the menu at a specified page and check that it opened and navigated
  // properly.
  // TODO(crbug.com/350990359): Change this to the Magic Stack VC once it's
  // implemented.
  [coordinator_ presentCustomizationMenuAtPage:CustomizationMenuPage::kMain];
  EXPECT_NSEQ(
      NSStringFromClass(
          [coordinator_.navigationController.visibleViewController class]),
      NSStringFromClass([coordinator_.mainViewController class]));

  // Stop the coordinator and check that the main VC and mediator have been set
  // back to nil.
  [coordinator_ stop];
  EXPECT_EQ(nil, coordinator_.mainViewController);
  EXPECT_EQ(nil, coordinator_.mediator);
}
