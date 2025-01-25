// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"

#include "ash/constants/ash_features.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_navigation_throttle.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {
constexpr char kTabUrl1[] = "http://example.com";
constexpr char kTabUrl1SubDomain1[] = "http://example.child.com";
constexpr char kTabUrl1SubDomain2[] = "http://example.b.com";
constexpr char kTabUrl1FrontSubDomain1[] = "http://sub.example.com";
constexpr char kTabUrl1WithPath[] = "http://example.child.com/random/path/";
constexpr char kTabUrl1WithSubPage[] = "http://example.com/blah-blah";
constexpr char kTabUrl1WithRandomQuery[] =
    "http://example.child.com/q?randomness";
constexpr char kTabUrl1DomainRedirect[] =
    "http://example.child.com/redirected/url/path.html";
constexpr char kTabUrlRedirectedUrl[] = "http://redirect-url.com/q?randomness";
constexpr char kTabUrl2[] = "http://company.org";
constexpr char kTabUrl2SubDomain1[] = "http://company.a.org";
constexpr char kTabGoogleUrl[] = "http://google.com";
constexpr char kTabDocsUrl[] = "http://docs.google.com";
constexpr char kTabGooglePath[] = "http://google.com/blah-blah";

}  // namespace

class OnTaskLockedSessionWindowTrackerTest : public BrowserWithTestWindowTest {
 public:
  std::unique_ptr<Browser> CreateTestBrowser(bool popup) {
    auto window = std::make_unique<TestBrowserWindow>();
    Browser::Type type = popup ? Browser::TYPE_APP_POPUP : Browser::TYPE_NORMAL;

    std::unique_ptr<Browser> browser =
        CreateBrowser(profile(), type, false, window.get());
    // Self deleting.
    new TestBrowserWindowOwner(std::move(window));
    return browser;
  }

  void CreateWindowTrackerServiceForTesting() {
    LockedSessionWindowTrackerFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          PrefService* pref_service = user_prefs::UserPrefs::Get(context);
          auto url_blocklist_manager =
              std::make_unique<policy::URLBlocklistManager>(
                  pref_service, policy::policy_prefs::kUrlBlocklist,
                  policy::policy_prefs::kUrlAllowlist);
          auto on_task_blocklist = std::make_unique<OnTaskBlocklist>(
              std::move(url_blocklist_manager));
          return std::make_unique<LockedSessionWindowTracker>(
              std::move(on_task_blocklist));
        }));
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return (LockedSessionWindowTrackerFactory::GetForBrowserContext(
                  profile()) != nullptr);
    }));
  }

  void TearDown() override {
    task_environment()->RunUntilIdle();
    auto* const window_tracker =
        LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
    if (window_tracker) {
      window_tracker->InitializeBrowserInfoForTracking(nullptr);
    }
    BrowserWithTestWindowTest::TearDown();
  }
};

TEST_F(OnTaskLockedSessionWindowTrackerTest, RegisterUrlsAndRestrictionLevels) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_b(kTabUrl2);
  const GURL url_a_subdomain(kTabUrl1SubDomain1);
  const GURL url_b_subdomain(kTabUrl2SubDomain1);
  const GURL url_a_subdomain2(kTabUrl1SubDomain2);
  AddTab(browser(), url_a);
  AddTab(browser(), url_b);
  AddTab(browser(), url_a_subdomain);
  AddTab(browser(), url_b_subdomain);
  AddTab(browser(), url_a_subdomain2);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();
  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(4),
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(3),
      OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(2),
      OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(1),
      OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);
  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation);
  ASSERT_EQ(on_task_blocklist->parent_tab_to_nav_filters().size(), 5u);
  EXPECT_EQ(
      on_task_blocklist
          ->parent_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              tab_strip_model->GetWebContentsAt(4))],
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  EXPECT_EQ(
      on_task_blocklist
          ->parent_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              tab_strip_model->GetWebContentsAt(3))],
      OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  EXPECT_EQ(
      on_task_blocklist
          ->parent_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              tab_strip_model->GetWebContentsAt(2))],
      OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  EXPECT_EQ(
      on_task_blocklist
          ->parent_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              tab_strip_model->GetWebContentsAt(1))],
      OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);
  EXPECT_EQ(
      on_task_blocklist
          ->parent_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              tab_strip_model->GetWebContentsAt(0))],
      OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation);
  EXPECT_EQ(on_task_blocklist->one_level_deep_original_url().size(), 2u);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       RegisterChildUrlsWithRestrictions) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_a_child(kTabUrl1SubDomain1);
  AddTab(browser(), url_a);
  AddTab(browser(), url_a_child);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();

  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(1),
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  on_task_blocklist->MaybeSetURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);
  ASSERT_EQ(on_task_blocklist->parent_tab_to_nav_filters().size(), 1u);
  ASSERT_EQ(on_task_blocklist->child_tab_to_nav_filters().size(), 1u);

  EXPECT_EQ(
      on_task_blocklist
          ->parent_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              tab_strip_model->GetWebContentsAt(1))],
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  EXPECT_EQ(
      on_task_blocklist
          ->child_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              tab_strip_model->GetWebContentsAt(0))],
      OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);
  EXPECT_EQ(on_task_blocklist->one_level_deep_original_url().size(), 1u);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       NavigateCurrentTabWithMultipleRestrictionsMaintainTabRestrictions) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url(kTabUrl1);
  const GURL url_subdomain(kTabUrl1SubDomain1);
  const GURL url_with_query(kTabUrl1WithRandomQuery);
  const GURL url_with_path(kTabUrl1WithPath);
  AddTab(browser(), url_subdomain);
  AddTab(browser(), url);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();

  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(1),
      OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  ASSERT_EQ(on_task_blocklist->parent_tab_to_nav_filters().size(), 2u);
  ASSERT_EQ(
      on_task_blocklist
          ->parent_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              tab_strip_model->GetWebContentsAt(0))],
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  window_tracker->RefreshUrlBlocklist();
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  on_task_blocklist->MaybeSetURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(1),
      OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  NavigateAndCommitActiveTab(url_subdomain);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);

  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);

  NavigateAndCommitActiveTab(url_with_query);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  NavigateAndCommitActiveTab(url_with_path);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest, NavigateNonParentTab) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url(kTabUrl1);
  const GURL url_subdomain(kTabUrl1SubDomain1);
  const GURL url_with_query(kTabUrl1WithRandomQuery);
  const GURL url_with_path(kTabUrl1WithPath);
  AddTab(browser(), url_subdomain);
  AddTab(browser(), url);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();

  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  on_task_blocklist->MaybeSetURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(1),
      OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  ASSERT_EQ(on_task_blocklist->parent_tab_to_nav_filters().size(), 1u);
  ASSERT_EQ(
      on_task_blocklist
          ->parent_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              tab_strip_model->GetWebContentsAt(0))],
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  ASSERT_EQ(on_task_blocklist->child_tab_to_nav_filters().size(), 1u);
  ASSERT_EQ(
      on_task_blocklist
          ->child_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              tab_strip_model->GetWebContentsAt(1))],
      OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  window_tracker->RefreshUrlBlocklist();
  task_environment()->RunUntilIdle();

  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  browser()->tab_strip_model()->ActivateTabAt(1);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);

  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       NavigateCurrentTabWithNewRestrictedLevelFromRedirectUrl) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url(kTabUrl1);
  const GURL url_subdomain(kTabUrl1SubDomain1);
  AddTab(browser(), url);
  const auto* const tab_strip_model = browser()->tab_strip_model();

  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();

  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  ASSERT_EQ(on_task_blocklist->parent_tab_to_nav_filters().size(), 1u);
  ASSERT_EQ(
      on_task_blocklist
          ->parent_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              tab_strip_model->GetWebContentsAt(0))],
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  window_tracker->RefreshUrlBlocklist();
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  NavigateAndCommitActiveTab(url_subdomain);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  const GURL url_redirect(kTabUrlRedirectedUrl);

  NavigateAndCommitActiveTab(url_redirect);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       NavigateCurrentTabThatSpawnsNewTab) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url(kTabUrl1);
  const GURL url_subdomain(kTabUrl1SubDomain1);
  AddTab(browser(), url);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();

  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);
  ASSERT_EQ(on_task_blocklist->parent_tab_to_nav_filters().size(), 1u);
  ASSERT_EQ(
      on_task_blocklist
          ->parent_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              tab_strip_model->GetWebContentsAt(0))],
      OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);
  window_tracker->RefreshUrlBlocklist();
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);
  AddTab(browser(), url_subdomain);
  const GURL url_redirect(kTabUrl1DomainRedirect);

  NavigateAndCommitActiveTab(url_redirect);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  // Sanity check to make sure child tabs aren't added as parent tabs.
  EXPECT_FALSE(
      on_task_blocklist->IsParentTab(tab_strip_model->GetWebContentsAt(0)));
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       NavigateCurrentTabWithSameDomainAndOneLevelDeepFromRedirectUrl) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url(kTabUrl1);
  const GURL url_subdomain(kTabUrl1SubDomain1);
  const GURL url_with_query(kTabUrl1WithRandomQuery);
  AddTab(browser(), url_subdomain);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();

  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation);
  ASSERT_EQ(on_task_blocklist->parent_tab_to_nav_filters().size(), 1u);
  ASSERT_EQ(
      on_task_blocklist
          ->parent_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              tab_strip_model->GetWebContentsAt(0))],
      OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation);
  window_tracker->RefreshUrlBlocklist();
  EXPECT_EQ(
      on_task_blocklist->current_page_restriction_level(),
      OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation);
  NavigateAndCommitActiveTab(url);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  const GURL url_redirect(kTabUrl1DomainRedirect);

  NavigateAndCommitActiveTab(url_redirect);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(
      on_task_blocklist->current_page_restriction_level(),
      OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation);

  const GURL url_redirect_not_same_domain(kTabUrlRedirectedUrl);

  NavigateAndCommitActiveTab(url_redirect_not_same_domain);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(
      on_task_blocklist->current_page_restriction_level(),
      OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation);
  // Redirect happens in a new tab.
  AddTab(browser(), url_redirect);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  NavigateAndCommitActiveTab(url_redirect_not_same_domain);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest, SwitchTabWithNewRestrictedLevel) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_b(kTabUrl2);
  // Add Tab inserts tab at the 0th index.
  AddTab(browser(), url_a);
  AddTab(browser(), url_b);
  const auto* const tab_strip_model = browser()->tab_strip_model();

  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();

  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(1),
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  window_tracker->RefreshUrlBlocklist();
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       BlockUrlSuccessfullyForLimitedNav) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_b(kTabUrl2);
  AddTab(browser(), url_a);
  const auto* const tab_strip_model = browser()->tab_strip_model();

  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();

  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  window_tracker->RefreshUrlBlocklist();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_b),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       AllowAndBlockUrlSuccessfullyForSameDomainNav) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_a_front_subdomain(kTabUrl1FrontSubDomain1);
  const GURL url_a_subpage(kTabUrl1WithSubPage);
  const GURL url_a_subdomain_page(kTabUrl1WithPath);
  const GURL url_a_subdomain(kTabUrl1SubDomain1);
  const GURL url_b(kTabUrl2);

  AddTab(browser(), url_a);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();

  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  window_tracker->RefreshUrlBlocklist();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_a_front_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_a_subpage),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_a_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_a_subdomain_page),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_b),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       AllowUrlSuccessfullyForUnrestrictedNav) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_a_front_subdomain(kTabUrl1FrontSubDomain1);
  const GURL url_a_path(kTabUrl1WithPath);
  const GURL url_a_subdomain(kTabUrl1SubDomain1);
  const GURL url_b(kTabUrl2);

  AddTab(browser(), url_a);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();

  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  window_tracker->RefreshUrlBlocklist();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_a_front_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_a_path),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_a_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_b),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       AllowAndBlockUrlSuccessfullyForGoogleSameDomainNav) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL google_url(kTabGoogleUrl);
  const GURL docs_url(kTabDocsUrl);
  const GURL random_google_url(kTabGooglePath);
  const GURL url_b(kTabUrl1SubDomain1);
  const GURL not_google_url(kTabUrl2);

  AddTab(browser(), google_url);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();

  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  window_tracker->RefreshUrlBlocklist();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(docs_url),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(random_google_url),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_b),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(not_google_url),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest, NewBrowserWindowsDontOpen) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  window_tracker->InitializeBrowserInfoForTracking(browser());
  std::unique_ptr<Browser> normal_browser(CreateTestBrowser(/*popup=*/false));
  ASSERT_TRUE(base::test::RunUntil(
      [&normal_browser]() { return normal_browser != nullptr; }));

  EXPECT_TRUE(
      static_cast<TestBrowserWindow*>(normal_browser->window())->IsClosed());
}

TEST_F(OnTaskLockedSessionWindowTrackerTest, NewBrowserPopupIsRegistered) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  window_tracker->InitializeBrowserInfoForTracking(browser());
  std::unique_ptr<Browser> popup_browser(CreateTestBrowser(/*popup=*/true));
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_FALSE(
      static_cast<TestBrowserWindow*>(popup_browser->window())->IsClosed());
  EXPECT_FALSE(window_tracker->CanProcessPopup());
  popup_browser->OnWindowClosing();
  EXPECT_TRUE(window_tracker->CanProcessPopup());
}

TEST_F(OnTaskLockedSessionWindowTrackerTest, BrowserClose) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_a_child(kTabUrl1SubDomain1);
  AddTab(browser(), url_a);
  AddTab(browser(), url_a_child);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);

  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  browser()->OnWindowClosing();
  ASSERT_TRUE(base::test::RunUntil(
      [&window_tracker]() { return !window_tracker->browser(); }));
  EXPECT_FALSE(window_tracker->browser());
}

TEST_F(OnTaskLockedSessionWindowTrackerTest, BrowserTrackingOverride) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  std::unique_ptr<Browser> normal_browser(CreateTestBrowser(/*popup=*/false));
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  window_tracker->InitializeBrowserInfoForTracking(normal_browser.get());
  ASSERT_NE(window_tracker->browser(), browser());
  EXPECT_EQ(window_tracker->browser(), normal_browser.get());
  // Set back to nullptr so during tear down we are not accessing a deleted
  // normal_browser ptr. Since normal_browser is created only in the lifetime of
  // this one unittest, and we set the window_tracker to track this, by the time
  // tear down is called, normal_browser ptr is freed, but there is still a ref
  // to that ptr by the window_tracker during tear down.
  window_tracker->InitializeBrowserInfoForTracking(nullptr);
}

class OnTaskNavigationThrottleTest
    : public OnTaskLockedSessionWindowTrackerTest {
 protected:
  OnTaskNavigationThrottleTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kBoca,
                              ash::features::kBocaConsumer},
        /*disabled_features=*/{});
  }

  std::unique_ptr<content::NavigationSimulator> StartNavigation(
      const GURL& first_url,
      content::RenderFrameHost* rfh) {
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(first_url, rfh);
    simulator->Start();
    return simulator;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OnTaskNavigationThrottleTest,
       NoNavigationThrottleRegisteredWithoutTracker) {
  const GURL url_a(kTabUrl1);
  const GURL url_a_front_subdomain(kTabUrl1FrontSubDomain1);
  const GURL url_a_path(kTabUrl1WithPath);
  const GURL url_a_subdomain(kTabUrl1SubDomain1);
  const GURL url_b(kTabUrl2);

  AddTab(browser(), url_a);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  auto simulator = StartNavigation(
      url_a_front_subdomain,
      tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
  const auto throttle =
      ash::OnTaskLockedSessionNavigationThrottle::MaybeCreateThrottleFor(
          simulator->GetNavigationHandle());
  EXPECT_THAT(throttle, testing::IsNull());
}

TEST_F(OnTaskNavigationThrottleTest, AllowUrlSuccessfullyForUnrestrictedNav) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_a_front_subdomain(kTabUrl1FrontSubDomain1);
  const GURL url_a_path(kTabUrl1WithPath);
  const GURL url_a_subdomain(kTabUrl1SubDomain1);
  const GURL url_b(kTabUrl2);

  AddTab(browser(), url_a);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();
  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  window_tracker->RefreshUrlBlocklist();
  task_environment()->RunUntilIdle();

  ASSERT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  {
    auto simulator = StartNavigation(
        url_a_front_subdomain,
        tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              simulator->GetLastThrottleCheckResult());
  }
  {
    auto simulator = StartNavigation(
        url_a_path,
        tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              simulator->GetLastThrottleCheckResult());
  }
  {
    auto simulator = StartNavigation(
        url_a_subdomain,
        tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              simulator->GetLastThrottleCheckResult());
  }
  {
    auto simulator = StartNavigation(
        url_b, tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              simulator->GetLastThrottleCheckResult());
  }
}

TEST_F(OnTaskNavigationThrottleTest, BlockUrlSuccessfullyForRestrictedNav) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_a_front_subdomain(kTabUrl1FrontSubDomain1);
  const GURL url_a_path(kTabUrl1WithPath);
  const GURL url_a_subdomain(kTabUrl1SubDomain1);
  const GURL url_b(kTabUrl2);

  AddTab(browser(), url_a);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();

  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  window_tracker->RefreshUrlBlocklist();
  task_environment()->RunUntilIdle();

  ASSERT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  {
    auto simulator = StartNavigation(
        url_a_front_subdomain,
        tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
    EXPECT_EQ(content::NavigationThrottle::CANCEL,
              simulator->GetLastThrottleCheckResult());
  }
  {
    auto simulator = StartNavigation(
        url_a_path,
        tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
    EXPECT_EQ(content::NavigationThrottle::CANCEL,
              simulator->GetLastThrottleCheckResult());
  }
  {
    auto simulator = StartNavigation(
        url_a_subdomain,
        tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
    EXPECT_EQ(content::NavigationThrottle::CANCEL,
              simulator->GetLastThrottleCheckResult());
  }
  {
    auto simulator = StartNavigation(
        url_b, tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
    EXPECT_EQ(content::NavigationThrottle::CANCEL,
              simulator->GetLastThrottleCheckResult());
  }
}

TEST_F(OnTaskNavigationThrottleTest,
       BlockAndAllowUrlSuccessfullyForSameDomainNav) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_a_front_subdomain(kTabUrl1FrontSubDomain1);
  const GURL url_a_subpage(kTabUrl1WithSubPage);
  const GURL url_a_subdomain_page(kTabUrl1WithPath);
  const GURL url_a_subdomain(kTabUrl1SubDomain1);
  const GURL url_b(kTabUrl2);

  AddTab(browser(), url_a);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();

  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  window_tracker->RefreshUrlBlocklist();
  task_environment()->RunUntilIdle();

  ASSERT_EQ(on_task_blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  {
    auto simulator = StartNavigation(
        url_a_front_subdomain,
        tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              simulator->GetLastThrottleCheckResult());
  }
  {
    auto simulator = StartNavigation(
        url_a_subpage,
        tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              simulator->GetLastThrottleCheckResult());
  }
  {
    auto simulator = StartNavigation(
        url_a_subdomain,
        tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
    EXPECT_EQ(content::NavigationThrottle::CANCEL,
              simulator->GetLastThrottleCheckResult());
  }
  {
    auto simulator = StartNavigation(
        url_b, tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
    EXPECT_EQ(content::NavigationThrottle::CANCEL,
              simulator->GetLastThrottleCheckResult());
  }
}

TEST_F(OnTaskNavigationThrottleTest, SuccessNavigationWorksEvenWithRedirects) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const std::vector<GURL>& redirect_chain = {GURL(kTabUrlRedirectedUrl),
                                             GURL(kTabUrl1DomainRedirect)};
  AddTab(browser(), url_a);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();
  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  window_tracker->RefreshUrlBlocklist();
  task_environment()->RunUntilIdle();

  auto simulator = StartNavigation(
      url_a, tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame());
  for (const GURL& redirect_url : redirect_chain) {
    simulator->Redirect(redirect_url);
  }
  simulator->Commit();
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult());
}

TEST_F(OnTaskNavigationThrottleTest, BlockUrlInNewTabShouldClose) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_b(kTabUrl2);

  AddTab(browser(), url_a);
  const auto* const tab_strip_model = browser()->tab_strip_model();
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const on_task_blocklist = window_tracker->on_task_blocklist();
  on_task_blocklist->SetParentURLRestrictionLevel(
      tab_strip_model->GetWebContentsAt(0),
      OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  window_tracker->RefreshUrlBlocklist();
  task_environment()->RunUntilIdle();
  content::WebContents* new_tab = browser()->OpenURL(
      content::OpenURLParams(url_b, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK,
                             /* is_renderer_initiated= */ false),
      /*navigation_handle_callback=*/{});
  ASSERT_EQ(tab_strip_model->count(), 2);
  ASSERT_FALSE(new_tab->GetLastCommittedURL().is_valid());
  auto simulator = StartNavigation(url_b, new_tab->GetPrimaryMainFrame());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return tab_strip_model->GetIndexOfWebContents(new_tab) ==
           TabStripModel::kNoTab;
  }));
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            simulator->GetLastThrottleCheckResult());
  EXPECT_EQ(tab_strip_model->count(), 1);
}
