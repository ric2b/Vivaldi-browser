// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/adverse_adblocking/adverse_ad_filter_list.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/subresource_filter/subresource_filter_test_harness.h"
#include "components/adverse_adblocking/adverse_ad_filter_list_factory.h"
#include "components/adverse_adblocking/vivaldi_content_browser_client.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class VivaldiSubresourceFilterTest : public SubresourceFilterTestHarness {
 public:
  void SetUp() override {
    vivaldi::ForceVivaldiRunning(true);
    SubresourceFilterTestHarness::SetUp();
    VivaldiSubresourceFilterClient::CreateForWebContents(web_contents());
    browser_content_client_.reset(new VivaldiContentBrowserClient);
    content::SetBrowserClientForTesting(browser_content_client_.get());
  }
  void TearDown() override {
    SubresourceFilterTestHarness::TearDown();
    browser_content_client_.reset();
    content::SetContentClient(NULL);
    vivaldi::ForceVivaldiRunning(false);
  }
  void ConfigureAsSubresourceFilterOnlyURL(const GURL& url) {
    ASSERT_TRUE(url.SchemeIsHTTPOrHTTPS());
    adblock_ = VivaldiAdverseAdFilterListFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    adblock_->ClearSiteList();
    adblock_->AddBlockItem(url.host());
  }
  VivaldiSubresourceFilterClient* GetClient() {
    return VivaldiSubresourceFilterClient::FromWebContents(web_contents());
  }

 private:
  std::unique_ptr<VivaldiContentBrowserClient> browser_content_client_;
  AdverseAdFilterListService* adblock_;
};

TEST_F(VivaldiSubresourceFilterTest, SimpleAllowedLoad) {
  GURL url("https://example.test");
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));
  EXPECT_FALSE(GetClient()->did_show_ui_for_navigation());
}

TEST_F(VivaldiSubresourceFilterTest, SimpleDisallowedLoad) {
  GURL url("https://example.test");
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(main_rfh()));
  EXPECT_TRUE(GetClient()->did_show_ui_for_navigation());
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
            *observer.GetSubframeLoadPolicy(allowed_url));
  EXPECT_FALSE(*observer.GetIsAdSubframe(subframe->GetFrameTreeNodeId()));
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
  EXPECT_FALSE(
      SimulateNavigateAndCommit(GURL(kDefaultDisallowedUrl), subframe));
  EXPECT_EQ(subresource_filter::LoadPolicy::DISALLOW,
            *observer.GetSubframeLoadPolicy(disallowed_url));
  EXPECT_TRUE(*observer.GetIsAdSubframe(subframe->GetFrameTreeNodeId()));
}
