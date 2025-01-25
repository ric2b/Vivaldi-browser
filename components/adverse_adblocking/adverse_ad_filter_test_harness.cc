// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "components/adverse_adblocking/adverse_ad_filter_test_harness.h"

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/subresource_filter/chrome_content_subresource_filter_web_contents_helper_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_throttle_manager.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/safe_browsing_ruleset_publisher.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/common/constants.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

constexpr char const AdverseAdFilterTestHarness::kDefaultAllowedSuffix[];
constexpr char const AdverseAdFilterTestHarness::kDefaultDisallowedSuffix[];
constexpr char const AdverseAdFilterTestHarness::kDefaultDisallowedUrl[];

AdverseAdFilterTestHarness::AdverseAdFilterTestHarness() = default;
AdverseAdFilterTestHarness::~AdverseAdFilterTestHarness() = default;

// ChromeRenderViewHostTestHarness:
void AdverseAdFilterTestHarness::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  NavigateAndCommit(GURL("https://example.first"));

  // Set up safe browsing service.
  //
  // TODO(csharrison): This is a bit ugly. See if the instructions in
  // test_safe_browsing_service.h can be adapted to be used in unit tests.
  safe_browsing::TestSafeBrowsingServiceFactory sb_service_factory;
  safe_browsing::SafeBrowsingService::RegisterFactory(&sb_service_factory);
  auto* safe_browsing_service = sb_service_factory.CreateSafeBrowsingService();
  safe_browsing::SafeBrowsingService::RegisterFactory(nullptr);
  TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
      safe_browsing_service);
  g_browser_process->safe_browsing_service()->Initialize();

  // Set up the ruleset service.
  ASSERT_TRUE(ruleset_service_dir_.CreateUniqueTempDir());
  subresource_filter::IndexedRulesetVersion::RegisterPrefs(
      pref_service_.registry(),
      subresource_filter::kSafeBrowsingRulesetConfig.filter_tag);
  // TODO(csharrison): having separated blocking and background task runners
  // for |ContentRulesetService| and |RulesetService| would be a good idea, but
  // external unit tests code implicitly uses knowledge that blocking and
  // background task runners are initiazlied from
  // |base::SingleThreadTaskRunner::GetCurrentDefault()|:
  // 1. |TestRulesetPublisher| uses this knowledge in |SetRuleset| method. It
  //    is waiting for the ruleset published callback.
  // 2. Navigation simulator uses this knowledge. It knows that
  //    |AsyncDocumentSubresourceFilter| posts core initialization tasks on
  //    blocking task runner and this it is the current thread task runner.
  auto ruleset_service = std::make_unique<subresource_filter::RulesetService>(
    subresource_filter::kSafeBrowsingRulesetConfig,
    &pref_service_, base::SingleThreadTaskRunner::GetCurrentDefault(),
    ruleset_service_dir_.GetPath(),
    base::SingleThreadTaskRunner::GetCurrentDefault(),
    subresource_filter::SafeBrowsingRulesetPublisher::Factory()
    );
  TestingBrowserProcess::GetGlobal()->SetRulesetService(
      std::move(ruleset_service));

  // Publish the test ruleset.
  subresource_filter::testing::TestRulesetCreator ruleset_creator;
  subresource_filter::testing::TestRulesetPair test_ruleset_pair;
  ruleset_creator.CreateRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule(kDefaultDisallowedSuffix),
       subresource_filter::testing::CreateAllowlistSuffixRule(
           kDefaultAllowedSuffix)},
      &test_ruleset_pair);
  subresource_filter::testing::TestRulesetPublisher test_ruleset_publisher(
      g_browser_process->subresource_filter_ruleset_service());
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));

  // Set up the tab helpers.
  infobars::ContentInfoBarManager::CreateForWebContents(web_contents());
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(
          web_contents()));

  VivaldiSubresourceFilterAdblockingThrottleManager::
      CreateSubresourceFilterWebContentsHelper(web_contents());

  CreateSubresourceFilterWebContentsHelper(web_contents());

  base::RunLoop().RunUntilIdle();
}

void AdverseAdFilterTestHarness::TearDown() {
  TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();

  // Must explicitly set these to null and pump the run loop to ensure that
  // all cleanup related to these classes actually happens.
  TestingBrowserProcess::GetGlobal()->SetRulesetService(nullptr);
  TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);

  base::RunLoop().RunUntilIdle();

  ChromeRenderViewHostTestHarness::TearDown();
}

// Will return nullptr if the navigation fails.
content::RenderFrameHost* AdverseAdFilterTestHarness::SimulateNavigateAndCommit(
    const GURL& url,
    content::RenderFrameHost* rfh) {
  auto simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, rfh);
  simulator->Commit();
  return simulator->GetLastThrottleCheckResult().action() ==
                 content::NavigationThrottle::PROCEED
             ? simulator->GetFinalRenderFrameHost()
             : nullptr;
}

// Returns the frame host the navigation commit in, or nullptr if it did not
// succeed.
content::RenderFrameHost*
AdverseAdFilterTestHarness::CreateAndNavigateDisallowedSubframe(
    content::RenderFrameHost* parent) {
  auto* subframe =
      content::RenderFrameHostTester::For(parent)->AppendChild("subframe");
  return SimulateNavigateAndCommit(GURL(kDefaultDisallowedUrl), subframe);
}
