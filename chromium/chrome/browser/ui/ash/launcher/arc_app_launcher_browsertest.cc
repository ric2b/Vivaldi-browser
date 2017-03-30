// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_delegate.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_deferred_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/common/common_type_converters.h"

namespace mojo {

template <>
struct TypeConverter<arc::mojom::AppInfoPtr, arc::mojom::AppInfo> {
  static arc::mojom::AppInfoPtr Convert(const arc::mojom::AppInfo& app_info) {
    return app_info.Clone();
  }
};

template <>
struct TypeConverter<arc::mojom::ArcPackageInfoPtr,
                     arc::mojom::ArcPackageInfo> {
  static arc::mojom::ArcPackageInfoPtr Convert(
      const arc::mojom::ArcPackageInfo& package_info) {
    return package_info.Clone();
  }
};

}  // namespace mojo

namespace {

const char kTestAppName[] = "Test Arc App";
const char kTestAppName2[] = "Test Arc App 2";
const char kTestAppPackage[] = "test.arc.app.package";
const char kTestAppActivity[] = "test.arc.app.package.activity";
const char kTestAppActivity2[] = "test.arc.app.package.activity2";

std::string GetTestApp1Id() {
  return ArcAppListPrefs::GetAppId(kTestAppPackage, kTestAppActivity);
}

std::string GetTestApp2Id() {
  return ArcAppListPrefs::GetAppId(kTestAppPackage, kTestAppActivity2);
}

mojo::Array<arc::mojom::AppInfoPtr> GetTestAppsList(bool multi_app) {
  std::vector<arc::mojom::AppInfo> apps;

  arc::mojom::AppInfo app;
  app.name = kTestAppName;
  app.package_name = kTestAppPackage;
  app.activity = kTestAppActivity;
  app.sticky = false;
  apps.push_back(app);

  if (multi_app) {
    app.name = kTestAppName2;
    app.package_name = kTestAppPackage;
    app.activity = kTestAppActivity2;
    app.sticky = false;
    apps.push_back(app);
  }

  return mojo::Array<arc::mojom::AppInfoPtr>::From(apps);
}

ash::ShelfDelegate* shelf_delegate() {
  return ash::Shell::GetInstance()->GetShelfDelegate();
}

}  // namespace

class ArcAppLauncherBrowserTest : public ExtensionBrowserTest {
 public:
  ArcAppLauncherBrowserTest() {}
  ~ArcAppLauncherBrowserTest() override {}

 protected:
  // content::BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kEnableArc);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
    arc::ArcAuthService::DisableUIForTesting();
  }

  void SetUpOnMainThread() override { arc::ArcAuthService::Get()->EnableArc(); }

  void InstallTestApps(bool multi_app) {
    app_host()->OnAppListRefreshed(GetTestAppsList(multi_app));

    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        app_prefs()->GetApp(GetTestApp1Id());
    ASSERT_TRUE(app_info);
    EXPECT_TRUE(app_info->ready);
    if (multi_app) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info2 =
          app_prefs()->GetApp(GetTestApp2Id());
      ASSERT_TRUE(app_info2);
      EXPECT_TRUE(app_info2->ready);
    }
  }

  void SendPackageAdded(bool package_synced) {
    arc::mojom::ArcPackageInfo package_info;
    package_info.package_name = kTestAppPackage;
    package_info.package_version = 1;
    package_info.last_backup_android_id = 1;
    package_info.last_backup_time = 1;
    package_info.sync = package_synced;
    package_info.system = false;
    app_host()->OnPackageAdded(arc::mojom::ArcPackageInfo::From(package_info));

    base::RunLoop().RunUntilIdle();
  }

  void SendPackageUpdated(bool multi_app) {
    app_host()->OnPackageAppListRefreshed(kTestAppPackage,
                                          GetTestAppsList(multi_app));
  }

  void SendPackageRemoved() { app_host()->OnPackageRemoved(kTestAppPackage); }

  void StartInstance() {
    if (auth_service()->profile() != profile())
      auth_service()->OnPrimaryUserProfilePrepared(profile());
    app_instance_observer()->OnInstanceReady();
  }

  void StopInstance() {
    auth_service()->Shutdown();
    app_instance_observer()->OnInstanceClosed();
  }

  ArcAppListPrefs* app_prefs() { return ArcAppListPrefs::Get(profile()); }

  // Returns as AppHost interface in order to access to private implementation
  // of the interface.
  arc::mojom::AppHost* app_host() { return app_prefs(); }

  // Returns as AppInstance observer interface in order to access to private
  // implementation of the interface.
  arc::InstanceHolder<arc::mojom::AppInstance>::Observer*
  app_instance_observer() {
    return app_prefs();
  }

  arc::ArcAuthService* auth_service() { return arc::ArcAuthService::Get(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppLauncherBrowserTest);
};

// This tests validates pin state on package update and remove.
IN_PROC_BROWSER_TEST_F(ArcAppLauncherBrowserTest, PinOnPackageUpdateAndRemove) {
  StartInstance();

  // Make use app list sync service is started. Normally it is started when
  // sycing is initialized.
  app_list::AppListSyncableServiceFactory::GetForProfile(profile())->GetModel();

  InstallTestApps(true);
  SendPackageAdded(false);

  const std::string app_id1 = GetTestApp1Id();
  const std::string app_id2 = GetTestApp2Id();
  shelf_delegate()->PinAppWithID(app_id1);
  shelf_delegate()->PinAppWithID(app_id2);
  const ash::ShelfID shelf_id1_before =
      shelf_delegate()->GetShelfIDForAppID(app_id1);
  EXPECT_TRUE(shelf_id1_before);
  EXPECT_TRUE(shelf_delegate()->GetShelfIDForAppID(app_id2));

  // Package contains only one app. App list is not shown for updated package.
  SendPackageUpdated(false);
  // Second pin should gone.
  EXPECT_EQ(shelf_id1_before, shelf_delegate()->GetShelfIDForAppID(app_id1));
  EXPECT_FALSE(shelf_delegate()->GetShelfIDForAppID(app_id2));

  // Package contains two apps. App list is not shown for updated package.
  SendPackageUpdated(true);
  // Second pin should not appear.
  EXPECT_EQ(shelf_id1_before, shelf_delegate()->GetShelfIDForAppID(app_id1));
  EXPECT_FALSE(shelf_delegate()->GetShelfIDForAppID(app_id2));

  // Package removed.
  SendPackageRemoved();
  // No pin is expected.
  EXPECT_FALSE(shelf_delegate()->GetShelfIDForAppID(app_id1));
  EXPECT_FALSE(shelf_delegate()->GetShelfIDForAppID(app_id2));
}

// This test validates that app list is shown on new package and not shown
// on package update.
IN_PROC_BROWSER_TEST_F(ArcAppLauncherBrowserTest, AppListShown) {
  StartInstance();
  AppListService* app_list_service = AppListService::Get();
  ASSERT_TRUE(app_list_service);

  EXPECT_FALSE(app_list_service->IsAppListVisible());

  // New package is available. Show app list.
  InstallTestApps(false);
  SendPackageAdded(true);
  EXPECT_TRUE(app_list_service->IsAppListVisible());

  app_list_service->DismissAppList();
  EXPECT_FALSE(app_list_service->IsAppListVisible());

  // Send package update event. App list is not shown.
  SendPackageAdded(true);
  EXPECT_FALSE(app_list_service->IsAppListVisible());
}
