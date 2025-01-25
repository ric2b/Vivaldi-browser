// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/platform_test.h"

class DriveServiceFactoryTest : public PlatformTest {
 protected:
  DriveServiceFactoryTest() {
    scoped_feature_list_.InitAndEnableFeature(kIOSSaveToDrive);
    browser_state_ = TestChromeBrowserState::Builder().Build();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
};

// Checks that the same instance is returned for on-the-record and
// off-the-record browser states.
TEST_F(DriveServiceFactoryTest, BrowserStateRedirectedInIncognito) {
  drive::DriveService* on_the_record_service =
      drive::DriveServiceFactory::GetForBrowserState(browser_state_.get());
  drive::DriveService* off_the_record_service =
      drive::DriveServiceFactory::GetForBrowserState(
          browser_state_->GetOffTheRecordChromeBrowserState());
  EXPECT_TRUE(on_the_record_service != nullptr);
  EXPECT_EQ(on_the_record_service, off_the_record_service);
}
