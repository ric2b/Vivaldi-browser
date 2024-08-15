// Copyright (c) 2019-2021 Vivaldi Technologies AS. All rights reserved
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/adverse_adblocking/adverse_ad_filter_list.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/test/base/testing_browser_process.h"

#include "components/adverse_adblocking/adverse_ad_filter_list_factory.h"
#include "components/adverse_adblocking/adverse_ad_filter_test_harness.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_throttle.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_throttle_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"

#include "content/public/common/content_client.h"
#include "content/public/test/test_navigation_observer.h"
#include "extraparts/vivaldi_content_browser_client.h"
#include "prefs/vivaldi_local_state_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class PrefRegistrySimple;

class VivaldiSubresourceFilterTest : public AdverseAdFilterTestHarness {
 public:
  void SetUp() override {
    vivaldi::ForceVivaldiRunning(true);
    SystemNetworkContextManager::RegisterPrefs(local_state_.registry());
    ChromeContentBrowserClient::RegisterLocalStatePrefs(
        local_state_.registry());
    safe_browsing::RegisterLocalStatePrefs(local_state_.registry());
    vivaldi::RegisterLocalStatePrefs(local_state_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    AdverseAdFilterTestHarness::SetUp();

    VivaldiSubresourceFilterAdblockingThrottleManager::
        CreateSubresourceFilterWebContentsHelper(web_contents());
    browser_content_client_.reset(new VivaldiContentBrowserClient);
    content::SetBrowserClientForTesting(browser_content_client_.get());

    ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&::password_manager::BuildPasswordStore<
                                       content::BrowserContext,
                                       ::password_manager::TestPasswordStore>));
  }
  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    AdverseAdFilterTestHarness::TearDown();
    browser_content_client_.reset();
    content::SetContentClient(NULL);
    vivaldi::ForceVivaldiRunning(false);
  }
  void ConfigureAsSubresourceFilterOnlyURL(const GURL& url) {
    ASSERT_TRUE(url.SchemeIsHTTPOrHTTPS());
    adblock_ = VivaldiAdverseAdFilterListFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

    VivaldiSubresourceFilterAdblockingThrottleManager::FromWebContents(
        web_contents())
        ->set_adblock_list(adblock_);

    adblock_->ClearSiteList();
    adblock_->AddBlockItem(url.host());
  }

 private:
  std::unique_ptr<VivaldiContentBrowserClient> browser_content_client_;
  raw_ptr<AdverseAdFilterListService> adblock_ = nullptr;
  // local_state_ should be destructed after TestingProfile.
  TestingPrefServiceSimple local_state_;
};

TEST_F(VivaldiSubresourceFilterTest, SimpleAllowedLoad) {
  GURL url("https://example.test");
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));
}

TEST_F(VivaldiSubresourceFilterTest, SimpleDisallowedLoad) {
  GURL url("https://example.test");
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(main_rfh()));
}

TEST_F(VivaldiSubresourceFilterTest, SimpleAllowedLoad_WithObserver) {
  GURL url("https://example.test");
  ConfigureAsSubresourceFilterOnlyURL(url);

  subresource_filter::TestSubresourceFilterObserver observer(web_contents());
  SimulateNavigateAndCommit(url, main_rfh());

  EXPECT_EQ(subresource_filter::mojom::ActivationLevel::kEnabled,
            observer.GetPageActivation(url).value());

  GURL allowed_url("https://example.test/foo");
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  SimulateNavigateAndCommit(GURL(allowed_url), subframe);
  EXPECT_EQ(subresource_filter::LoadPolicy::ALLOW,
            observer.GetChildFrameLoadPolicy(allowed_url).value());
  EXPECT_FALSE(observer.GetIsAdFrame(subframe->GetFrameTreeNodeId()));
}

TEST_F(VivaldiSubresourceFilterTest, SimpleDisallowedLoad_WithObserver) {
  GURL url("https://example.test");
  ConfigureAsSubresourceFilterOnlyURL(url);

  subresource_filter::TestSubresourceFilterObserver observer(web_contents());
  SimulateNavigateAndCommit(url, main_rfh());

  EXPECT_EQ(subresource_filter::mojom::ActivationLevel::kEnabled,
            observer.GetPageActivation(url).value());

  GURL disallowed_url(VivaldiSubresourceFilterTest::kDefaultDisallowedUrl);
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");

  content::TestNavigationObserver navigation_observer(
      web_contents(), content::MessageLoopRunner::QuitMode::IMMEDIATE,
      false /* ignore_uncommitted_navigations */);
  EXPECT_FALSE(
      SimulateNavigateAndCommit(GURL(kDefaultDisallowedUrl), subframe));
  navigation_observer.WaitForNavigationFinished();

  EXPECT_EQ(subresource_filter::LoadPolicy::DISALLOW,
            observer.GetChildFrameLoadPolicy(disallowed_url).value());
  EXPECT_TRUE(observer.GetIsAdFrame(subframe->GetFrameTreeNodeId()));
}
