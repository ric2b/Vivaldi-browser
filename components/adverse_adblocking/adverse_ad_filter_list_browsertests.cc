// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/adverse_adblocking/adverse_ad_filter_list.h"

#include "app/vivaldi_apptools.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/pattern.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_database_helper.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/subresource_filter/test_ruleset_publisher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"

#include "components/adverse_adblocking/adverse_ad_filter_list_factory.h"
#include "components/adverse_adblocking/vivaldi_content_browser_client.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_client.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter_test_utils.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using subresource_filter::ActivationList;
using subresource_filter::ActivationDecision;
using subresource_filter::Configuration;
using subresource_filter::kActivationConsoleMessage;
using subresource_filter::kAdTagging;
using subresource_filter::ScopedThreadTimers;
using subresource_filter::testing::CreateSuffixRule;
using subresource_filter::testing::CreateWhitelistRuleForDocument;
using subresource_filter::testing::TestRulesetPair;

namespace {

namespace proto = url_pattern_index::proto;

// The path to a multi-frame document used for tests.
static constexpr const char kTestFrameSetPath[] =
    "/subresource_filter/frame_set.html";

constexpr const char kActivationDecision[] =
    "SubresourceFilter.PageLoad.ActivationDecision";

// Names of navigation chain patterns histogram.
const char kActivationListHistogram[] =
    "SubresourceFilter.PageLoad.ActivationList";

// Other histograms.
const char kSubresourceFilterActionsHistogram[] = "SubresourceFilter.Actions2";

GURL GetURLWithFragment(const GURL& url, base::StringPiece fragment) {
  GURL::Replacements replacements;
  replacements.SetRefStr(fragment);
  return url.ReplaceComponents(replacements);
}

}  // namespace

class VivaldiSubresourceFilterBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  void SetUp() override {
    vivaldi::ForceVivaldiRunning(true);
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }
  void TearDown() override {
    subresource_filter::SubresourceFilterBrowserTest::TearDown();
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

// Tests -----------------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest,
                       MainFrameActivation_SubresourceFilterList) {
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kActivationConsoleMessage);
  web_contents()->SetDelegate(&console_observer);
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsSubresourceFilterOnlyURL(url);
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));

  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ACTIVATION_LIST,
                       subresource_filter::ActivationList::SUBRESOURCE_FILTER);
  ResetConfiguration(std::move(config));

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  EXPECT_EQ(kActivationConsoleMessage, console_observer.message());

  // The main frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("frame_with_included_script.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(
    VivaldiSubresourceFilterBrowserTest,
    ExpectRedirectPatternHistogramsAreRecordedForSubresourceFilterOnlyRedirectMatch) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  const std::string initial_host("a.com");
  const std::string redirected_host("b.com");

  GURL redirect_url(embedded_test_server()->GetURL(
      redirected_host, "/subresource_filter/frame_with_included_script.html"));
  GURL url(embedded_test_server()->GetURL(
      initial_host, "/server-redirect?" + redirect_url.spec()));

  ConfigureAsSubresourceFilterOnlyURL(url.GetOrigin());
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);
  tester.ExpectUniqueSample(kActivationListHistogram,
                            static_cast<int>(ActivationList::NONE), 1);
}

IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest,
                       MainFrameActivation) {
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kActivationConsoleMessage);
  web_contents()->SetDelegate(&console_observer);
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsSubresourceFilterOnlyURL(url);
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  EXPECT_EQ(console_observer.message(), kActivationConsoleMessage);

  // The main frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("frame_with_included_script.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

// There should be no document-level de-/reactivation happening on the renderer
// side as a result of a same document navigation.
IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest,
                       DocumentActivationOutlivesSameDocumentNavigation) {
  GURL url(GetTestUrl("subresource_filter/frame_with_delayed_script.html"));
  ConfigureAsSubresourceFilterOnlyURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Deactivation would already detected by the IsDynamicScriptElementLoaded
  // line alone. To ensure no reactivation, which would muddy up recorded
  // histograms, also set a ruleset that allows everything. If there was
  // reactivation, then this new ruleset would be picked up, once again causing
  // the IsDynamicScriptElementLoaded check to fail.
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));
  NavigateFromRendererSide(GetURLWithFragment(url, "ref"));
  EXPECT_FALSE(IsDynamicScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest,
                       SubframeDocumentLoadFiltering) {
  base::HistogramTester histogram_tester;
  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsSubresourceFilterOnlyURL(url);

  // Disallow loading subframe documents that in turn would end up loading
  // included_script.js, unless the document is loaded from a whitelisted
  // domain. This enables the third part of this test disallowing a load only
  // after the first redirect.
  const char kWhitelistedDomain[] = "whitelisted.com";
  proto::UrlRule rule =
      subresource_filter::testing::CreateSuffixRule("included_script.html");
  proto::UrlRule whitelist_rule =
      subresource_filter::testing::CreateSuffixRule(kWhitelistedDomain);
  whitelist_rule.set_anchor_right(proto::ANCHOR_TYPE_NONE);
  whitelist_rule.set_semantics(proto::RULE_SEMANTICS_WHITELIST);
  ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules({rule, whitelist_rule}));

  ui_test_utils::NavigateToURL(browser(), url);

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectOnlySecondSubframe{false, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);
  histogram_tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                                     SubresourceFilterAction::kUIShown, 1);

  // Now navigate the first subframe to an allowed URL and ensure that the load
  // successfully commits and the frame gets restored (no longer collapsed).
  GURL allowed_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_allowed_script.html"));
  NavigateFrame(kSubframeNames[0], allowed_subdocument_url);

  const std::vector<bool> kExpectFirstAndSecondSubframe{true, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectFirstAndSecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectFirstAndSecondSubframe);

  // Navigate the first subframe to a document that does not load the probe JS.
  GURL allowed_empty_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_no_subresources.html"));
  NavigateFrame(kSubframeNames[0], allowed_empty_subdocument_url);

  // Finally, navigate the first subframe to an allowed URL that redirects to a
  // disallowed URL, and verify that:
  //  -- The navigation gets blocked and the frame collapsed (with PlzNavigate).
  //  -- The navigation is cancelled, but the frame is not collapsed (without
  //  PlzNavigate, where BLOCK_REQUEST_AND_COLLAPSE is not supported).
  GURL disallowed_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  GURL redirect_to_disallowed_subdocument_url(embedded_test_server()->GetURL(
      kWhitelistedDomain,
      "/server-redirect?" + disallowed_subdocument_url.spec()));
  NavigateFrame(kSubframeNames[0], redirect_to_disallowed_subdocument_url);

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));

  content::RenderFrameHost* frame = FindFrameByName(kSubframeNames[0]);
  ASSERT_TRUE(frame);
  EXPECT_EQ(disallowed_subdocument_url, frame->GetLastCommittedURL());
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);
}

IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest,
                       HistoryNavigationActivation) {
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kActivationConsoleMessage);
  web_contents()->SetDelegate(&console_observer);
  GURL url_with_activation(GetTestUrl(kTestFrameSetPath));
  GURL url_without_activation(
      embedded_test_server()->GetURL("a.com", kTestFrameSetPath));
  ConfigureAsSubresourceFilterOnlyURL(url_with_activation);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectScriptInFrameToLoadWithoutActivation{
      true, true, true};
  const std::vector<bool> kExpectScriptInFrameToLoadWithActivation{false, true,
                                                                   false};

  ui_test_utils::NavigateToURL(browser(), url_without_activation);
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithoutActivation));

  // No message should be displayed for navigating to URL without activation.
  EXPECT_TRUE(console_observer.message().empty());

  ui_test_utils::NavigateToURL(browser(), url_with_activation);
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithActivation));

  // Console message should now be displayed.
  EXPECT_EQ(console_observer.message(), kActivationConsoleMessage);

  ASSERT_TRUE(web_contents()->GetController().CanGoBack());
  content::WindowedNotificationObserver back_navigation_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  web_contents()->GetController().GoBack();
  back_navigation_stop_observer.Wait();
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithoutActivation));

  ASSERT_TRUE(web_contents()->GetController().CanGoForward());
  content::WindowedNotificationObserver forward_navigation_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  web_contents()->GetController().GoForward();
  forward_navigation_stop_observer.Wait();
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithActivation));
}

// The page-level activation state on the browser-side should not be reset when
// a same document navigation starts in the main frame. Verify this by
// dynamically inserting a subframe afterwards, and still expecting activation.
IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest,
                       PageLevelActivationOutlivesSameDocumentNavigation) {
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kActivationConsoleMessage);
  web_contents()->SetDelegate(&console_observer);
  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsSubresourceFilterOnlyURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::RenderFrameHost* frame = FindFrameByName("one");
  ASSERT_TRUE(frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(frame));

  NavigateFromRendererSide(GetURLWithFragment(url, "ref"));

  ASSERT_NO_FATAL_FAILURE(InsertDynamicFrameWithScript());
  content::RenderFrameHost* dynamic_frame = FindFrameByName("dynamic");
  ASSERT_TRUE(dynamic_frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(dynamic_frame));

  EXPECT_EQ(console_observer.message(), kActivationConsoleMessage);
}

// If a navigation starts but aborts before commit, page level activation should
// remain unchanged.
IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest,
                       PageLevelActivationOutlivesAbortedNavigation) {
  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsSubresourceFilterOnlyURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::RenderFrameHost* frame = FindFrameByName("one");
  EXPECT_FALSE(WasParsedScriptElementLoaded(frame));

  // Start a new navigation, but abort it right away.
  GURL aborted_url = GURL("https://abort-me.com");
  content::TestNavigationManager manager(
      browser()->tab_strip_model()->GetActiveWebContents(), aborted_url);

  NavigateParams params(browser(), aborted_url, ui::PAGE_TRANSITION_LINK);
  Navigate(&params);
  ASSERT_TRUE(manager.WaitForRequestStart());
  browser()->tab_strip_model()->GetActiveWebContents()->Stop();

  // Will return false if the navigation was successfully aborted.
  ASSERT_FALSE(manager.WaitForResponse());
  manager.WaitForNavigationFinished();

  // Now, dynamically insert a frame and expect that it is still activated.
  ASSERT_NO_FATAL_FAILURE(InsertDynamicFrameWithScript());
  content::RenderFrameHost* dynamic_frame = FindFrameByName("dynamic");
  ASSERT_TRUE(dynamic_frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(dynamic_frame));
}

IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest, DynamicFrame) {
  GURL url(GetTestUrl("subresource_filter/frame_set.html"));
  ConfigureAsSubresourceFilterOnlyURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(InsertDynamicFrameWithScript());
  content::RenderFrameHost* dynamic_frame = FindFrameByName("dynamic");
  ASSERT_TRUE(dynamic_frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(dynamic_frame));
}

IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest,
                       PRE_MainFrameActivationOnStartup) {
  SetRulesetToDisallowURLsWithPathSuffix("included_script.js");
}

IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest,
                       MainFrameActivationOnStartup) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsSubresourceFilterOnlyURL(url);
  // Verify that the ruleset persisted in the previous session is used for this
  // page load right after start-up.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest,
                       CrossSiteSubFrameActivationWithoutWhitelist) {
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/frame_cross_site_set.html"));
  ConfigureAsSubresourceFilterOnlyURL(a_url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), a_url);
  ExpectParsedScriptElementLoadedStatusInFrames(
      std::vector<const char*>{"b", "c", "d"}, {false, false, false});
}

IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest,
                       CrossSiteSubFrameActivationWithWhitelist) {
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/frame_cross_site_set.html"));
  ConfigureAsSubresourceFilterOnlyURL(a_url);
  ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("included_script.js"),
       subresource_filter::testing::CreateWhitelistRuleForDocument("c.com")}));
  ui_test_utils::NavigateToURL(browser(), a_url);
  ExpectParsedScriptElementLoadedStatusInFrames(
      std::vector<const char*>{"b", "d"}, {false, true});
}

IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest,
                       RendererDebugURL_NoLeakedThrottlePtrs) {
  // Allow crashes caused by the navigation to kChromeUICrashURL below.
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes(
      browser()->tab_strip_model()->GetActiveWebContents());

  // We have checks in the throttle manager that we don't improperly leak
  // activation state throttles. It would be nice to test things directly but it
  // isn't very feasible right now without exposing a bunch of internal guts of
  // the throttle manager.
  //
  // This test should crash the *browser process* with CHECK failures if the
  // component is faulty. The CHECK assumes that the crash URL and other
  // renderer debug URLs do not create a navigation throttle. See
  // crbug.com/736658.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      content::NotificationService::AllSources());
  browser()->OpenURL(content::OpenURLParams(
      GURL(content::kChromeUICrashURL), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  observer.Wait();
}

// Tests checking how histograms are recorded. ---------------------------------

IN_PROC_BROWSER_TEST_F(VivaldiSubresourceFilterBrowserTest,
                       ActivationEnabledOnReload) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsSubresourceFilterOnlyURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::MessageLoopRunner::QuitMode::DEFERRED);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  tester.ExpectTotalCount(kActivationDecision, 2);
  tester.ExpectBucketCount(kActivationDecision,
                           static_cast<int>(ActivationDecision::ACTIVATED), 2);
}
