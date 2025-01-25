// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/test/gtest_tags.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_profile_loader.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_closed_waiter.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

using ::base::test::TestFuture;
using ::testing::_;

const test::UIPath kNetworkConfigureScreenContinueButton = {"error-message",
                                                            "continueButton"};

std::optional<Profile*> LoadKioskProfile(const AccountId& account_id) {
  TestFuture<std::optional<Profile*>> profile_future;
  auto profile_loader = LoadProfile(
      account_id, KioskAppType::kWebApp,
      base::BindOnce([](KioskProfileLoader::Result result) {
        return result.has_value() ? std::make_optional(result.value())
                                  : std::nullopt;
      }).Then(profile_future.GetCallback()));
  return profile_future.Take();
}

// TODO(b/322497609) Replace `URLLoaderInterceptor` with `EmbeddedTestServer`.
content::URLLoaderInterceptor SimplePageInterceptor() {
  return content::URLLoaderInterceptor(base::BindRepeating(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        content::URLLoaderInterceptor::WriteResponse(
            "content/test/data/simple_page.html", params->client.get());
        return true;
      }));
}

bool InstallKioskWebAppInProvider(Profile& profile) {
  // Serve a real page to avoid installing a placeholder app.
  auto url_interceptor = SimplePageInterceptor();

  TestFuture<bool> success_future;

  auto* provider = web_app::WebAppProvider::GetForLocalAppsUnchecked(&profile);

  web_app::ExternalInstallOptions install_options(
      GURL(kAppInstallUrl), web_app::mojom::UserDisplayMode::kStandalone,
      web_app::ExternalInstallSource::kKiosk);
  install_options.install_placeholder = true;

  provider->externally_managed_app_manager().InstallNow(
      install_options,
      base::BindOnce([](const GURL& install_url,
                        web_app::ExternallyManagedAppManager::InstallResult
                            result) {
        return webapps::IsSuccess(result.code);
      }).Then(success_future.GetCallback()));
  return success_future.Wait();
}

Browser::CreateParams CreateNewBrowserParams(Browser* initial_kiosk_browser,
                                             bool is_popup_browser) {
  return is_popup_browser
             ? Browser::CreateParams::CreateForAppPopup(
                   initial_kiosk_browser->app_name(), /*trusted_source=*/true,
                   /*window_bounds=*/gfx::Rect(),
                   initial_kiosk_browser->profile(),
                   /*user_gesture=*/true)
             : Browser::CreateParams(initial_kiosk_browser->profile(),
                                     /*user_gesture=*/true);
}

Browser* OpenNewBrowser(Browser* initial_kiosk_browser, bool is_popup_browser) {
  Browser::CreateParams params =
      CreateNewBrowserParams(initial_kiosk_browser, is_popup_browser);
  Browser* new_browser = Browser::Create(params);
  new_browser->window()->Show();
  return new_browser;
}

class WebKioskTest : public WebKioskBaseTest {
 public:
  WebKioskTest() = default;

  WebKioskTest(const WebKioskTest&) = delete;
  WebKioskTest& operator=(const WebKioskTest&) = delete;

  void EnsureAppIsInstalled() {
    std::optional<Profile*> profile = LoadKioskProfile(account_id());
    ASSERT_TRUE(profile.has_value());
    ASSERT_TRUE(InstallKioskWebAppInProvider(CHECK_DEREF(*profile)))
        << "App was not installed";
    Shell::Get()->session_controller()->RequestSignOut();
  }

  void SetBlockAppLaunch(bool block) {
    if (block) {
      block_app_launch_override_ = KioskTestHelper::BlockAppLaunch();
    } else {
      block_app_launch_override_.reset();
    }
  }

  void WaitNetworkConfigureScreenAndContinueWithOnlineState(
      bool require_network,
      bool auto_close = false) {
    OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
    SetOnline(false);
    // Unblock app launch after the network configure screen is shown.
    SetBlockAppLaunch(false);
    test::OobeJS().ExpectPathDisplayed(!require_network,
                                       kNetworkConfigureScreenContinueButton);
    SetOnline(true);

    if (!auto_close) {
      // Wait for update.
      // Continue button should be visible since we are online.
      test::OobeJS()
          .CreateDisplayedWaiter(true, kNetworkConfigureScreenContinueButton)
          ->Wait();
      test::OobeJS().ExpectPathDisplayed(true,
                                         kNetworkConfigureScreenContinueButton);
      // Click on continue button.
      test::OobeJS().TapOnPath(kNetworkConfigureScreenContinueButton);
    }
  }

  void ExpectKeyboardConfig() {
    const keyboard::KeyboardConfig config =
        KeyboardController::Get()->GetKeyboardConfig();

    // `auto_capitalize` is not controlled by the policy
    // 'VirtualKeyboardFeatures', and its default value remains true.
    EXPECT_TRUE(config.auto_capitalize);

    // The other features are controlled by the policy
    // 'VirtualKeyboardFeatures', and their default values should be false.
    EXPECT_FALSE(config.auto_complete);
    EXPECT_FALSE(config.auto_correct);
    EXPECT_FALSE(config.handwriting);
    EXPECT_FALSE(config.spell_check);
    EXPECT_FALSE(config.voice_input);
  }

 private:
  std::optional<base::AutoReset<bool>> block_app_launch_override_;
};

// Runs the kiosk app when the network is always present.
IN_PROC_BROWSER_TEST_F(WebKioskTest, RegularFlowOnline) {
  InitializeRegularOnlineKiosk();
}

// Runs the kiosk app when the network is not present in the beginning, but
// appears later.
IN_PROC_BROWSER_TEST_F(WebKioskTest, RegularFlowBecomesOnline) {
  SetOnline(false);
  PrepareAppLaunch();
  LaunchApp();
  SetOnline(true);
  KioskSessionInitializedWaiter().Wait();
}

// Runs the kiosk app without a network connection, waits till network wait
// times out. Network configure dialog appears. Afterwards, it configures
// network and closes network configure dialog. Launch proceeds.
IN_PROC_BROWSER_TEST_F(WebKioskTest, NetworkTimeout) {
  SetOnline(false);
  PrepareAppLaunch();
  LaunchApp();

  WaitNetworkConfigureScreenAndContinueWithOnlineState(
      /*require_network*/ true, /*auto_close*/ true);

  KioskSessionInitializedWaiter().Wait();
}

// App Service launcher requires installing web apps to Kiosk profile before
// launching offline.
IN_PROC_BROWSER_TEST_F(WebKioskTest, PRE_AlreadyInstalledOffline) {
  PrepareAppLaunch();
  EnsureAppIsInstalled();
}

// Runs the kiosk app offline when it has been already installed.
IN_PROC_BROWSER_TEST_F(WebKioskTest, AlreadyInstalledOffline) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-35e430a3-04b3-46a7-aa0a-207a368b8cba");

  SetOnline(false);
  PrepareAppLaunch();
  LaunchApp();
  KioskSessionInitializedWaiter().Wait();
}

// Presses a network configure dialog accelerator during app launch which will
// interrupt the startup. We expect this dialog not to require network since the
// app have not yet been installed.
IN_PROC_BROWSER_TEST_F(WebKioskTest, LaunchWithConfigureAcceleratorPressed) {
  SetOnline(true);
  PrepareAppLaunch();
  LaunchApp();

  // Block app launch after it is being installed.
  SetBlockAppLaunch(true);
  OobeScreenWaiter(AppLaunchSplashScreenView::kScreenId).Wait();
  ASSERT_TRUE(LoginScreenTestApi::PressAccelerator(
      ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)));
  WaitNetworkConfigureScreenAndContinueWithOnlineState(
      /* require_network*/ true);
  SetBlockAppLaunch(false);

  KioskSessionInitializedWaiter().Wait();
}

// App Service launcher requires installing web apps to Kiosk profile before
// launching offline.
IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       PRE_AlreadyInstalledWithConfigureAcceleratorPressed) {
  PrepareAppLaunch();
  EnsureAppIsInstalled();
}

// In case when the app was already installed, we should expect to be able to
// configure network without need to be online.
IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       AlreadyInstalledWithConfigureAcceleratorPressed) {
  SetOnline(false);
  // Set the threshold to a max value to disable the offline message screen,
  // otherwise it would interfere with app launch. This is needed as this is
  // happening on the GaiaScreen in terms of screens of WizardController.
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetHandler<GaiaScreenHandler>()
      ->set_offline_timeout_for_testing(base::TimeDelta::Max());
  PrepareAppLaunch();
  LaunchApp();

  // Block app launch after it is being installed.
  SetBlockAppLaunch(true);
  OobeScreenWaiter(AppLaunchSplashScreenView::kScreenId).Wait();

  ASSERT_TRUE(LoginScreenTestApi::PressAccelerator(
      ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)));

  WaitNetworkConfigureScreenAndContinueWithOnlineState(
      /* require_network*/ false);

  KioskSessionInitializedWaiter().Wait();
}

// The shelf should be forcedly hidden in the web kiosk session.
IN_PROC_BROWSER_TEST_F(WebKioskTest, HiddenShelf) {
  InitializeRegularOnlineKiosk();

  // The shelf should be hidden at the beginning.
  EXPECT_FALSE(ShelfTestApi().IsVisible());

  // Simulate the swipe-up gesture.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  BrowserWindow* browser_window = BrowserList::GetInstance()->get(0)->window();
  gfx::NativeWindow window = browser_window->GetNativeWindow()->GetRootWindow();
  const gfx::Rect display_bounds = window->bounds();
  const gfx::Point start_point = gfx::Point(
      display_bounds.width() / 4,
      display_bounds.bottom() - ShelfConfig::Get()->shelf_size() / 2);
  gfx::Point end_point(start_point.x(), start_point.y() - 80);
  ui::test::EventGenerator event_generator(window);
  event_generator.GestureScrollSequence(start_point, end_point,
                                        base::Milliseconds(500), 4);

  // The shelf should be still hidden after the gesture.
  EXPECT_FALSE(ShelfTestApi().IsVisible());
}

IN_PROC_BROWSER_TEST_F(WebKioskTest, KeyboardConfigPolicy) {
  InitializeRegularOnlineKiosk();
  ExpectKeyboardConfig();
}

IN_PROC_BROWSER_TEST_F(WebKioskTest, OpenA11ySettings) {
  InitializeRegularOnlineKiosk();

  Browser* settings_browser =
      OpenA11ySettingsBrowser(KioskController::Get().GetKioskSystemSession());

  // Make sure the settings browser was opened.
  ASSERT_NE(settings_browser, nullptr);
}

// If only the a11y settings window remains open, it should be automatically
// closed in the web kiosk session.
IN_PROC_BROWSER_TEST_F(WebKioskTest, CloseSettingWindowIfOnlyOpen) {
  InitializeRegularOnlineKiosk();
  // The initial browser should exist in the web kiosk session.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);

  KioskSystemSession* session = KioskController::Get().GetKioskSystemSession();

  Browser* settings_browser = OpenA11ySettingsBrowser(session);
  // Make sure the settings browser was opened.
  ASSERT_NE(settings_browser, nullptr);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);

  // Close the initial browser.
  initial_browser->window()->Close();
  // Ensure `settings_browser` is closed too.
  TestBrowserClosedWaiter settings_browser_closed_waiter{settings_browser};
  ASSERT_TRUE(settings_browser_closed_waiter.WaitUntilClosed());

  // No browsers are opened in the web kiosk session, so it should be
  // terminated.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 0u);
  EXPECT_TRUE(session->is_shutting_down());
}

// Closing the a11y settings window should not exit the web app kiosk
// session if another browser is opened.
IN_PROC_BROWSER_TEST_F(WebKioskTest, NotExitIfCloseSettingsWindow) {
  InitializeRegularOnlineKiosk();
  // The initial browser should exist in the web kiosk session.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);

  KioskSystemSession* session = KioskController::Get().GetKioskSystemSession();

  Browser* settings_browser = OpenA11ySettingsBrowser(session);
  // Make sure the settings browser was opened.
  ASSERT_NE(settings_browser, nullptr);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);

  // Close `settings_browser` and ensure it is closed.
  settings_browser->window()->Close();
  TestBrowserClosedWaiter settings_browser_closed_waiter{settings_browser};
  ASSERT_TRUE(settings_browser_closed_waiter.WaitUntilClosed());

  // The initial browsers should still be opened and so the kiosk session should
  // not be terminated.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_FALSE(session->is_shutting_down());
}

IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       NewPopupBrowserInKioskNotAllowedByDefault) {
  InitializeRegularOnlineKiosk();
  // The initial browser should exist in the web kiosk session.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);
  EXPECT_FALSE(initial_browser->profile()->GetPrefs()->GetBoolean(
      prefs::kNewWindowsInKioskAllowed));

  Browser* new_popup_browser =
      OpenNewBrowser(initial_browser, /*is_popup_browser=*/true);

  TestBrowserClosedWaiter browser_closed_waiter{new_popup_browser};
  ASSERT_TRUE(browser_closed_waiter.WaitUntilClosed());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       NewRegularBrowserInKioskNotAllowedByDefault) {
  InitializeRegularOnlineKiosk();
  // The initial browser should exist in the web kiosk session.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);
  EXPECT_FALSE(initial_browser->profile()->GetPrefs()->GetBoolean(
      prefs::kNewWindowsInKioskAllowed));

  Browser* new_browser =
      OpenNewBrowser(initial_browser, /*is_popup_browser=*/false);

  TestBrowserClosedWaiter browser_closed_waiter{new_browser};
  ASSERT_TRUE(browser_closed_waiter.WaitUntilClosed());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

IN_PROC_BROWSER_TEST_F(WebKioskTest, NewPopupBrowserInKioskAllowedByPolicy) {
  InitializeRegularOnlineKiosk();
  // The initial browser should exist in the web kiosk session.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);
  KioskSystemSession* session = KioskController::Get().GetKioskSystemSession();

  initial_browser->profile()->GetPrefs()->SetBoolean(
      prefs::kNewWindowsInKioskAllowed, true);
  Browser* new_popup_browser =
      OpenNewBrowser(initial_browser, /*is_popup_browser=*/true);

  EXPECT_FALSE(DidSessionCloseNewWindow(session));
  ASSERT_NE(new_popup_browser, nullptr);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
}

IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       NewRegularBrowserInKioskNotAllowedEvenByPolicy) {
  InitializeRegularOnlineKiosk();
  // The initial browser should exist in the web kiosk session.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);

  initial_browser->profile()->GetPrefs()->SetBoolean(
      prefs::kNewWindowsInKioskAllowed, true);
  Browser* new_browser =
      OpenNewBrowser(initial_browser, /*is_popup_browser=*/false);

  TestBrowserClosedWaiter browser_closed_waiter{new_browser};
  ASSERT_TRUE(browser_closed_waiter.WaitUntilClosed());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

}  // namespace
}  // namespace ash
