// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "app/vivaldi_apptools.h"
#include "base/path_service.h"
#include "base/vivaldi_paths.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "prefs/vivaldi_gen_prefs.h"

#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"

#include "chrome/test/base/ui_test_utils.h"

#include "extensions/browser/api/extensions_api_client.h"

#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"

#include "components/prefs/pref_service.h"

#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

#include "chrome/browser/ui/extensions/application_launch.h"
#include "net/base/filename_util.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

using base::PathService;
using extensions::PlatformAppBrowserTest;
using vivaldiprefs::TabsAutoMutingValues;

namespace extensions {

class VivaldiExtensionApiTest : public extensions::PlatformAppBrowserTest {
 public:
  VivaldiExtensionApiTest() {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_content_settings_map_ =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    vivaldi::RegisterVivaldiPaths();

    ExtensionApiTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII("autoplay-policy",
                                    "no-user-gesture-required");

    vivaldi::ForceVivaldiRunning(true);

    PathService::Get(vivaldi::DIR_VIVALDI_TEST_DATA, &test_data_dir_);
    test_data_dir_ = test_data_dir_.AppendASCII("extensions");
  }

  // Enable or disable sound.
  void SetSound(ContentSetting setting) {
    host_content_settings_map_->SetContentSettingCustomScope(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        ContentSettingsType::SOUND, setting);
  }

 private:
  raw_ptr<HostContentSettingsMap> host_content_settings_map_ =nullptr;
};

// Testing that automuting does not interfere with sound sitesettings.
IN_PROC_BROWSER_TEST_F(VivaldiExtensionApiTest, WebviewMediastate_1) {
  SetSound(CONTENT_SETTING_ALLOW);

  Profile* profile = browser()->profile();

  profile->GetPrefs()->SetInteger(vivaldiprefs::kTabsAutoMuting,
                                  static_cast<int>(TabsAutoMutingValues::kOff));

  ASSERT_TRUE(RunExtensionTest("automuting-expect-sound", {},
                               {.allow_in_incognito = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(VivaldiExtensionApiTest, WebviewMediastate_2) {
  SetSound(CONTENT_SETTING_ALLOW);

  Profile* profile = browser()->profile();

  profile->GetPrefs()->SetInteger(
      vivaldiprefs::kTabsAutoMuting,
      static_cast<int>(TabsAutoMutingValues::kOnlyactive));

  ASSERT_TRUE(RunExtensionTest("automuting-expect-sound", {},
                               {.allow_in_incognito = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(VivaldiExtensionApiTest, WebviewMediastate_3) {
  SetSound(CONTENT_SETTING_ALLOW);

  Profile* profile = browser()->profile();

  profile->GetPrefs()->SetInteger(
      vivaldiprefs::kTabsAutoMuting,
      static_cast<int>(TabsAutoMutingValues::kPrioritizeactive));

  ASSERT_TRUE(RunExtensionTest("automuting-expect-sound", {},
                               {.allow_in_incognito = true}))
      << message_;
}

// Testing that automuting does not interfere with blocked sound sitesetting.
IN_PROC_BROWSER_TEST_F(VivaldiExtensionApiTest, WebviewMediastateMuted) {
  SetSound(CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(RunExtensionTest("automuting-expect-muting", {},
                               {.allow_in_incognito = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(VivaldiExtensionApiTest, WebviewMediastateMuted_1) {
  SetSound(CONTENT_SETTING_BLOCK);

  Profile* profile = browser()->profile();

  profile->GetPrefs()->SetInteger(
      vivaldiprefs::kTabsAutoMuting,
      static_cast<int>(TabsAutoMutingValues::kOnlyactive));

  ASSERT_TRUE(RunExtensionTest("automuting-expect-muting", {},
                               {.allow_in_incognito = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(VivaldiExtensionApiTest, WebviewMediastateMuted_2) {
  SetSound(CONTENT_SETTING_BLOCK);

  Profile* profile = browser()->profile();

  profile->GetPrefs()->SetInteger(
      vivaldiprefs::kTabsAutoMuting,
      static_cast<int>(TabsAutoMutingValues::kPrioritizeactive));

  ASSERT_TRUE(RunExtensionTest("automuting-expect-muting", {},
                               {.allow_in_incognito = true}))
      << message_;
}

}  // namespace extensions
