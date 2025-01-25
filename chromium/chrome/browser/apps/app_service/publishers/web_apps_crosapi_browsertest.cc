// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crosapi/ash_requires_lacros_browsertestbase.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/apps/app_dialog/app_uninstall_dialog_view.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

class AppInstanceWaiter : public apps::InstanceRegistry::Observer {
 public:
  AppInstanceWaiter(apps::InstanceRegistry& instance_registry,
                    const std::string& app_id,
                    apps::InstanceState state =
                        apps::InstanceState(apps::kVisible | apps::kActive |
                                            apps::kRunning | apps::kStarted))
      : app_id_(app_id), state_(state) {
    observation_.Observe(&instance_registry);
  }
  ~AppInstanceWaiter() override = default;

  void Await() { run_loop_.Run(); }

 private:
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override {
    if (update.AppId() == app_id_ && update.State() == state_) {
      run_loop_.Quit();
    }
  }

  void OnInstanceRegistryWillBeDestroyed(apps::InstanceRegistry*) override {
    NOTREACHED_IN_MIGRATION();
  }

  const std::string app_id_;
  const apps::InstanceState state_;
  base::RunLoop run_loop_;
  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      observation_{this};
};

std::vector<std::string> GetContextMenuForApp(const std::string& app_id) {
  ash::ShelfItemDelegate* delegate =
      ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(app_id));

  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> future;
  delegate->GetContextMenu(
      /*display_id=*/0, future.GetCallback());

  auto model = future.Take();
  std::vector<std::string> items;
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    items.push_back(base::UTF16ToUTF8(model->GetLabelAt(i)));
  }
  return items;
}

void SelectContextMenuForApp(const std::string& app_id, size_t index) {
  ash::ShelfItemDelegate* delegate =
      ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(app_id));

  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> future;
  delegate->GetContextMenu(
      /*display_id=*/0, future.GetCallback());
  auto model = future.Take();
  model->ActivatedAt(index, /*event_flags=*/0);
}

}  // namespace

class WebAppsCrosapiBrowserTest
    : public crosapi::AshRequiresLacrosBrowserTestBase {
 public:
  WebAppsCrosapiBrowserTest() = default;
  ~WebAppsCrosapiBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    crosapi::AshRequiresLacrosBrowserTestBase::SetUpOnMainThread();
    if (!HasLacrosArgument()) {
      return;
    }

    apps::AppTypeInitializationWaiter(GetAshProfile(), apps::AppType::kWeb)
        .Await();
  }

  std::string InstallWebApp(const std::string& start_url,
                            apps::WindowMode mode) {
    base::test::TestFuture<const std::string&> app_id_future;
    GetStandaloneBrowserTestController()->InstallWebApp(
        start_url, mode, app_id_future.GetCallback());
    std::string app_id = app_id_future.Take();
    CHECK(!app_id.empty());
    apps::AppReadinessWaiter(GetAshProfile(), app_id).Await();
    return app_id;
  }

  apps::AppServiceProxy* AppServiceProxy() {
    return apps::AppServiceProxyFactory::GetForProfile(GetAshProfile());
  }
};

IN_PROC_BROWSER_TEST_F(WebAppsCrosapiBrowserTest, PinUsingContextMenu) {
  if (!HasLacrosArgument()) {
    return;
  }

  const size_t kNewWindowIndex = 0;
  const size_t kPinIndex = 1;
  const size_t kUnpinIndex = 1;
  const size_t kCloseIndex = 2;

  const webapps::AppId app_id =
      InstallWebApp("https://example.org/", apps::WindowMode::kWindow);

  EXPECT_FALSE(ash::ShelfModel::Get()->ItemByID(ash::ShelfID(app_id)));

  {
    AppInstanceWaiter waiter(AppServiceProxy()->InstanceRegistry(), app_id);
    AppServiceProxy()->Launch(app_id, /*event_flags=*/0,
                              apps::LaunchSource::kFromAppListGrid);
    waiter.Await();
  }

  EXPECT_TRUE(ash::ShelfModel::Get()->ItemByID(ash::ShelfID(app_id)));
  {
    std::vector<std::string> items = GetContextMenuForApp(app_id);
    ASSERT_EQ(5u, items.size());
    EXPECT_EQ(items[0], "New window");
    EXPECT_EQ(items[1], "Pin");
    EXPECT_EQ(items[2], "Close");
    EXPECT_EQ(items[3], "Uninstall");
    EXPECT_EQ(items[4], "App info");
  }

  SelectContextMenuForApp(app_id, kPinIndex);

  // Note that Close sends an asynchronous command from Ash to Lacros, without
  // waiting for the Ash InstanceRegistry to be updated.
  {
    AppInstanceWaiter waiter(AppServiceProxy()->InstanceRegistry(), app_id,
                             apps::kDestroyed);
    SelectContextMenuForApp(app_id, kCloseIndex);
    waiter.Await();
  }

  EXPECT_TRUE(ash::ShelfModel::Get()->ItemByID(ash::ShelfID(app_id)));
  {
    std::vector<std::string> items = GetContextMenuForApp(app_id);
    // Close is absent as there are no open windows.
    ASSERT_EQ(4u, items.size());
    EXPECT_EQ(items[0], "New window");
    EXPECT_EQ(items[1], "Unpin");
    EXPECT_EQ(items[2], "Uninstall");
    EXPECT_EQ(items[3], "App info");
  }

  {
    AppInstanceWaiter waiter(AppServiceProxy()->InstanceRegistry(), app_id);
    SelectContextMenuForApp(app_id, kNewWindowIndex);
    SelectContextMenuForApp(app_id, kUnpinIndex);
    waiter.Await();
  }

  EXPECT_TRUE(ash::ShelfModel::Get()->ItemByID(ash::ShelfID(app_id)));
  {
    std::vector<std::string> items = GetContextMenuForApp(app_id);
    ASSERT_EQ(5u, items.size());
    EXPECT_EQ(items[0], "New window");
    EXPECT_EQ(items[1], "Pin");
    EXPECT_EQ(items[2], "Close");
    EXPECT_EQ(items[3], "Uninstall");
    EXPECT_EQ(items[4], "App info");
  }

  {
    AppInstanceWaiter waiter(AppServiceProxy()->InstanceRegistry(), app_id,
                             apps::kDestroyed);
    SelectContextMenuForApp(app_id, kCloseIndex);
    waiter.Await();
  }

  EXPECT_FALSE(ash::ShelfModel::Get()->ItemByID(ash::ShelfID(app_id)));
}

// TODO(crbug.com/41485689): Re-enable when timeouts are fixed.
IN_PROC_BROWSER_TEST_F(WebAppsCrosapiBrowserTest, DISABLED_Uninstall) {
  if (!HasLacrosArgument()) {
    return;
  }

  const size_t kPinIndex = 1;
  const size_t kUninstallIndex = 3;

  const webapps::AppId app_id =
      InstallWebApp("https://example.org/", apps::WindowMode::kWindow);

  {
    AppInstanceWaiter waiter(AppServiceProxy()->InstanceRegistry(), app_id);
    AppServiceProxy()->Launch(app_id, /*event_flags=*/0,
                              apps::LaunchSource::kFromAppListGrid);
    waiter.Await();
  }

  EXPECT_TRUE(ash::ShelfModel::Get()->ItemByID(ash::ShelfID(app_id)));

  {
    base::test::TestFuture<void> signal;
    views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
    observer.set_initialized_callback(
        base::BindLambdaForTesting([&](views::Widget* widget) {
          if (widget->GetName() == "AppDialogView") {
            signal.GetCallback().Run();
          }
        }));

    SelectContextMenuForApp(app_id, kUninstallIndex);
    EXPECT_TRUE(signal.Wait());
  }

  AppUninstallDialogView::GetActiveViewForTesting()->CancelDialog();
  EXPECT_TRUE(ash::ShelfModel::Get()->ItemByID(ash::ShelfID(app_id)));

  SelectContextMenuForApp(app_id, kPinIndex);

  {
    base::test::TestFuture<void> signal;
    views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
    observer.set_initialized_callback(
        base::BindLambdaForTesting([&](views::Widget* widget) {
          if (widget->GetName() == "AppDialogView") {
            signal.GetCallback().Run();
          }
        }));

    SelectContextMenuForApp(app_id, kUninstallIndex);
    EXPECT_TRUE(signal.Wait());
  }

  {
    AppInstanceWaiter app_instance_waiter(AppServiceProxy()->InstanceRegistry(),
                                          app_id, apps::kDestroyed);
    AppUninstallDialogView::GetActiveViewForTesting()->AcceptDialog();
    apps::AppReadinessWaiter(GetAshProfile(), app_id,
                             apps::Readiness::kUninstalledByUser)
        .Await();
    app_instance_waiter.Await();
  }

  EXPECT_FALSE(ash::ShelfModel::Get()->ItemByID(ash::ShelfID(app_id)));
}

namespace {

constexpr char kPreventCloseForCalculatorTemplate[] = R"([
  {
    "manifest_id": "https://calculator.apps.chrome/",
    "run_on_os_login": "run_windowed",
    "prevent_close_after_run_on_os_login": %s
  }
])";

constexpr char kCalculatorForceInstalled[] = R"([
  {
    "url": "https://calculator.apps.chrome/",
    "default_launch_container": "window"
  }
])";

}  // namespace

class WebAppsPreventCloseCrosapiBrowserTest
    : public WebAppsCrosapiBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  WebAppsPreventCloseCrosapiBrowserTest() = default;

  WebAppsPreventCloseCrosapiBrowserTest(
      const WebAppsPreventCloseCrosapiBrowserTest&) = delete;
  WebAppsPreventCloseCrosapiBrowserTest& operator=(
      const WebAppsPreventCloseCrosapiBrowserTest&) = delete;

  ~WebAppsPreventCloseCrosapiBrowserTest() override = default;

  bool IsPreventCloseEnabled() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(WebAppsPreventCloseCrosapiBrowserTest,
                       CheckContextShelfMenu) {
  if (!HasLacrosArgument()) {
    return;
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // TODO(b/324499540): Disable this test variant on
  // ci/linux-chromeos-chrome-with-lacros since it fails.
  if (!IsPreventCloseEnabled()) {
    GTEST_SKIP();
  }
#endif

  {
    base::test::TestFuture<bool> waiter;
    GetStandaloneBrowserTestController()->SetWebAppInstallForceListPref(
        kCalculatorForceInstalled, waiter.GetCallback());
    EXPECT_TRUE(waiter.Wait());
  }

  {
    base::test::TestFuture<bool> waiter;
    GetStandaloneBrowserTestController()->SetWebAppSettingsPref(
        base::StringPrintf(kPreventCloseForCalculatorTemplate,
                           IsPreventCloseEnabled() ? "true" : "false"),
        waiter.GetCallback());
    EXPECT_TRUE(waiter.Wait());
  }

  apps::AppReadinessWaiter(GetAshProfile(), web_app::kCalculatorAppId).Await();

  EXPECT_FALSE(ash::ShelfModel::Get()->ItemByID(
      ash::ShelfID(web_app::kCalculatorAppId)));

  {
    AppInstanceWaiter waiter(AppServiceProxy()->InstanceRegistry(),
                             web_app::kCalculatorAppId);
    AppServiceProxy()->Launch(web_app::kCalculatorAppId, /*event_flags=*/0,
                              apps::LaunchSource::kFromAppListGrid);
    waiter.Await();
  }

  bool can_close = true;
  AppServiceProxy()->AppRegistryCache().ForOneApp(
      web_app::kCalculatorAppId, [&can_close](const apps::AppUpdate& update) {
        can_close = update.AllowClose().value_or(true);
      });

  // Wait until prefs are propagated and App `allow_close` field is updated to
  // expected value.
  if (can_close == IsPreventCloseEnabled()) {
    apps::AppUpdateWaiter waiter(
        GetAshProfile(), web_app::kCalculatorAppId,
        base::BindRepeating(
            [](bool expected_allow_close, const apps::AppUpdate& update) {
              return update.AllowClose().has_value() &&
                     update.AllowClose().value() == expected_allow_close;
            },
            !IsPreventCloseEnabled()));
    waiter.Await();
  }

  EXPECT_TRUE(ash::ShelfModel::Get()->ItemByID(
      ash::ShelfID(web_app::kCalculatorAppId)));

  const std::vector<std::string> items =
      GetContextMenuForApp(web_app::kCalculatorAppId);

  if (!IsPreventCloseEnabled()) {
    ASSERT_EQ(4u, items.size());
    EXPECT_EQ(items[0], "New window");
    EXPECT_EQ(items[1], "Pin");
    EXPECT_EQ(items[2], "Close");
    EXPECT_EQ(items[3], "App info");
  } else {
    ASSERT_EQ(2u, items.size());
    EXPECT_EQ(items[0], "Pin");
    EXPECT_EQ(items[1], "App info");
  }

  {
    base::test::TestFuture<bool> waiter;
    GetStandaloneBrowserTestController()->SetWebAppSettingsPref(
        "", waiter.GetCallback());
    EXPECT_TRUE(waiter.Wait());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppsPreventCloseCrosapiBrowserTest,
                         ::testing::Bool());

class IsolatedWebAppCrosapiBrowserTest : public WebAppsCrosapiBrowserTest {
 public:
  void SetUp() override {
    app_ = web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder())
               .BuildBundle();
    crosapi::AshRequiresLacrosBrowserTestBase::SetUp();
  }

  webapps::AppId InstallIsolatedWebAppOnLacros() {
    base::test::TestFuture<crosapi::mojom::InstallWebAppResultPtr>
        result_future;
    GetStandaloneBrowserTestController()->InstallIsolatedWebApp(
        crosapi::mojom::IsolatedWebAppLocation::NewBundlePath(app()->path()),
        /*dev_mode=*/false, result_future.GetCallback());

    crosapi::mojom::InstallWebAppResultPtr install_restult =
        result_future.Take();

    CHECK(install_restult->is_app_id())
        << "Isolated web app installation failed with error: "
        << install_restult->get_error_message();
    webapps::AppId app_id = install_restult->get_app_id();
    apps::AppReadinessWaiter(GetAshProfile(), app_id).Await();
    return app_id;
  }

  std::unique_ptr<ui::SimpleMenuModel> GetContextMenuForAppId(
      const webapps::AppId& app_id) {
    ash::ShelfItemDelegate* delegate =
        ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(app_id));

    base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> future;
    delegate->GetContextMenu(
        /*display_id=*/0, future.GetCallback());

    return future.Take();
  }

  web_app::ScopedBundledIsolatedWebApp* app() { return app_.get(); }

 private:
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppCrosapiBrowserTest,
                       ContextMenuOnlyHasLaunchNew) {
  if (!HasLacrosArgument()) {
    return;
  }

  webapps::AppId app_id = InstallIsolatedWebAppOnLacros();

  EXPECT_FALSE(ash::ShelfModel::Get()->ItemByID(ash::ShelfID(app_id)));

  {
    AppInstanceWaiter waiter(AppServiceProxy()->InstanceRegistry(), app_id);
    AppServiceProxy()->Launch(app_id, /*event_flags=*/0,
                              apps::LaunchSource::kFromAppListGrid);
    waiter.Await();
  }

  EXPECT_TRUE(ash::ShelfModel::Get()->ItemByID(ash::ShelfID(app_id)));

  {
    std::unique_ptr<ui::SimpleMenuModel> menu_model =
        GetContextMenuForAppId(app_id);
    ASSERT_TRUE(menu_model);

    // Isolated web apps context menu should have an "Open in new Window"
    // command instead of a open mode selector submenu.
    EXPECT_NE(menu_model->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_SUBMENU);
    EXPECT_EQ(menu_model->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_COMMAND);
    EXPECT_EQ(menu_model->GetCommandIdAt(0), ash::LAUNCH_NEW);
  }
}
