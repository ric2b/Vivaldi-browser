// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using ::testing::IsNull;
using ::testing::NotNull;

namespace ash::boca {
namespace {

constexpr char kTestUrl[] = "https://www.test.com";

class OnTaskSystemWebAppManagerImplBrowserTest : public InProcessBrowserTest {
 protected:
  OnTaskSystemWebAppManagerImplBrowserTest() {
    // Enable Boca and consumer experience for testing purposes. This is used
    // to set up the Boca SWA for OnTask.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kBoca, features::kBocaConsumer},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    ash::SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  Browser* FindBocaSystemWebAppBrowser() {
    return ash::FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::BOCA);
  }

  Profile* profile() { return browser()->profile(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       LaunchSystemWebAppAsync) {
  // Verify no Boca app is launched initially.
  ASSERT_THAT(FindBocaSystemWebAppBrowser(), IsNull());

  // Launch Boca app and verify launch result.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());

  // Also verify the new app window is the active window and is set up for
  // locked mode transition.
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  EXPECT_TRUE(boca_app_browser->IsLockedForOnTask());
  EXPECT_EQ(boca_app_browser->session_id(),
            system_web_app_manager.GetActiveSystemWebAppWindowID());
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       CloseSystemWebAppWindow) {
  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  // Close Boca app and verify there is no active app instance.
  system_web_app_manager.CloseSystemWebAppWindow(
      boca_app_browser->session_id());
  content::RunAllTasksUntilIdle();
  EXPECT_THAT(FindBocaSystemWebAppBrowser(), IsNull());
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       PinSystemWebAppWindow) {
  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  // Pin the Boca app and verify result.
  system_web_app_manager.SetPinStateForSystemWebAppWindow(
      /*pinned=*/true, boca_app_browser->session_id());
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       UnpinSystemWebAppWindow) {
  // Launch Boca app and pin it for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  system_web_app_manager.SetPinStateForSystemWebAppWindow(
      /*pinned=*/true, boca_app_browser->session_id());
  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  // Unpin the Boca app and verify result.
  system_web_app_manager.SetPinStateForSystemWebAppWindow(
      /*pinned=*/false, boca_app_browser->session_id());
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       CreateBackgroundTabWithUrl) {
  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  // Boca homepage is by default opened.
  EXPECT_EQ(boca_app_browser->tab_strip_model()->count(), 1);

  // Create tab from the url and verify that Boca has the tab.
  system_web_app_manager.CreateBackgroundTabWithUrl(
      boca_app_browser->session_id(), GURL(kTestUrl),
      OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  EXPECT_EQ(boca_app_browser->tab_strip_model()->count(), 2);
  content::WebContents* web_contents =
      boca_app_browser->tab_strip_model()->GetWebContentsAt(1);
  content::TestNavigationObserver observer(web_contents);
  observer.Wait();
  EXPECT_EQ(web_contents->GetLastCommittedURL(), GURL(kTestUrl));

  // Verify that the restriction is applied to the tab.
  LockedSessionWindowTracker* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  OnTaskBlocklist* const blocklist = window_tracker->on_task_blocklist();
  EXPECT_EQ(
      blocklist
          ->parent_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              web_contents)],
      OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
}

}  // namespace
}  // namespace ash::boca
