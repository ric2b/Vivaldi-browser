// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/model/browser_impl.h"

#import <memory>

#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/fake_browser_observer.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class BrowserImplTest : public PlatformTest {
 protected:
  BrowserImplTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    scene_state_ =
        [[FakeSceneState alloc] initWithAppState:nil
                                    browserState:chrome_browser_state_.get()];
  }

  std::unique_ptr<BrowserImpl> CreateBrowser() {
    return std::make_unique<BrowserImpl>(
        chrome_browser_state_.get(), scene_state_,
        [[CommandDispatcher alloc] init],
        /*active_browser=*/nullptr,
        BrowserWebStateListDelegate::InsertionPolicy::kDoNothing,
        BrowserWebStateListDelegate::ActivationPolicy::kDoNothing,
        Browser::Type::kRegular);
  }

  TestChromeBrowserState* browser_state() {
    return chrome_browser_state_.get();
  }
  SceneState* scene_state() { return scene_state_; }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  __strong FakeSceneState* scene_state_;
};

// Tests that the accessors return the expected values.
TEST_F(BrowserImplTest, TestAccessors) {
  std::unique_ptr<BrowserImpl> browser = CreateBrowser();
  EXPECT_EQ(Browser::Type::kRegular, browser->type());
  EXPECT_FALSE(browser->IsInactive());
  EXPECT_NE(browser->GetWebStateList(), nullptr);
  EXPECT_NE(browser->GetCommandDispatcher(), nullptr);
  EXPECT_EQ(browser->GetBrowserState(), browser_state());
  EXPECT_EQ(browser->GetActiveBrowser(), browser.get());
  EXPECT_EQ(browser->GetInactiveBrowser(), nullptr);
  EXPECT_EQ(browser->GetSceneState(), scene_state());

  // Check that the inactive and the regular Browsers are correctly referencing
  // each other after creating the inactive Browser.
  {
    Browser* inactive_browser = browser->CreateInactiveBrowser();
    EXPECT_EQ(Browser::Type::kInactive, inactive_browser->type());
    EXPECT_TRUE(inactive_browser->IsInactive());
    EXPECT_NE(inactive_browser->GetWebStateList(), nullptr);
    EXPECT_NE(inactive_browser->GetCommandDispatcher(), nullptr);
    EXPECT_EQ(inactive_browser->GetBrowserState(), browser_state());
    EXPECT_EQ(inactive_browser->GetActiveBrowser(), browser.get());
    EXPECT_EQ(inactive_browser->GetInactiveBrowser(), inactive_browser);
    EXPECT_EQ(inactive_browser->GetSceneState(), scene_state());
    EXPECT_EQ(browser->GetInactiveBrowser(), inactive_browser);
  }

  // Check that destroying the inactive browser resets the reference to the
  // inactive Browser.
  browser->DestroyInactiveBrowser();
  EXPECT_FALSE(browser->IsInactive());
  EXPECT_EQ(browser->GetActiveBrowser(), browser.get());
  EXPECT_EQ(browser->GetInactiveBrowser(), nullptr);
}

// Tests that a temporary browser has the right type.
TEST_F(BrowserImplTest, TemporaryType) {
  std::unique_ptr<BrowserImpl> browser = CreateBrowser();
  std::unique_ptr<Browser> temporary_browser =
      browser->CreateTemporary(browser_state());
  EXPECT_EQ(Browser::Type::kTemporary, temporary_browser->type());
}

// Tests that a browser created with a regular browser state has the right type.
TEST_F(BrowserImplTest, RegularType) {
  std::unique_ptr<Browser> browser =
      BrowserImpl::Create(browser_state(), scene_state());
  EXPECT_EQ(Browser::Type::kRegular, browser->type());
}

// Tests that a browser created with an incognito browser state has the right
// type.
TEST_F(BrowserImplTest, IncognitoType) {
  ChromeBrowserState* incognito_browser_state =
      browser_state()->GetOffTheRecordChromeBrowserState();
  std::unique_ptr<Browser> incognito_browser =
      BrowserImpl::Create(incognito_browser_state, scene_state());
  EXPECT_EQ(Browser::Type::kIncognito, incognito_browser->type());
}

// Tests that the BrowserDestroyed() callback is sent when a browser is deleted.
TEST_F(BrowserImplTest, BrowserDestroyed) {
  std::unique_ptr<BrowserImpl> browser = CreateBrowser();
  FakeBrowserObserver observer(browser.get());
  ASSERT_FALSE(observer.browser_destroyed());

  browser.reset();
  EXPECT_TRUE(observer.browser_destroyed());
}

// Tests that the BrowserDestroyed() callback is sent when destroying the
// inactive Browser.
TEST_F(BrowserImplTest, InactiveBrowserDestroyed) {
  std::unique_ptr<BrowserImpl> browser = CreateBrowser();
  Browser* inactive_browser = browser->CreateInactiveBrowser();
  FakeBrowserObserver observer(inactive_browser);
  ASSERT_FALSE(observer.browser_destroyed());

  browser->DestroyInactiveBrowser();
  EXPECT_TRUE(observer.browser_destroyed());
}
