// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "browser/startup_vivaldi_browser.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/vivaldi_switches.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/grit/locale_settings.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/l10n/l10n_util.h"


#if defined(OS_MACOSX)
#include <iostream>
#include "browser/mac/sparkle_util.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "third_party/_winsparkle_lib/include/winsparkle.h"
#include "base/win/registry.h"
#include "base/i18n/rtl.h"
#endif

using extensions::Extension;


#define UPDATE_SOURCE_WIN_normal 0
#define UPDATE_SOURCE_WIN_snapshot 0
#define UPDATE_SOURCE_WIN_preview 1
#define UPDATE_SOURCE_WIN_beta 1
#define UPDATE_SOURCE_WIN_final 1

#define S(s) UPDATE_SOURCE_WIN_ ## s
#define UPDATE_SOURCE_WIN(s) S(s)

#define UPDATE_PREVIEW_SOURCE_WINDOWS 1

namespace vivaldi {

namespace {
// Start copied from chrome\browser\ui\startup\startup_browser_creator_impl.cc

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility functions ----------------------------------------------------------

// TODO(koz): Consolidate this function and remove the special casing.
const Extension* GetPlatformApp(Profile* profile,
                                const std::string& extension_id) {
  const Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::EVERYTHING);
  return extension && extension->is_platform_app() ? extension : NULL;
}

void RecordCmdLineAppHistogram(extensions::Manifest::Type app_type) {
  extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_CMD_LINE_APP,
                                  app_type);
}
// End copied
}

GURL GetVivaldiNewTabURL() {
  return GURL(vivaldi::kVivaldiNewTabURL);
}

#if defined(OS_WIN)
const int kWinSparkleInitDelaySecs = 20;

void WinSparkleCheckForUpdates() {
  win_sparkle_check_update_without_ui();

  int interval_secs = win_sparkle_get_update_check_interval();
  base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&WinSparkleCheckForUpdates),
      base::TimeDelta::FromSeconds(interval_secs));
}

#endif

//#define _DEBUG_AUTOUPDATE

bool LaunchVivaldi(const base::CommandLine &command_line,
                   const base::FilePath &cur_dir,
                   Profile *profile) {
  if (!(IsVivaldiRunning(command_line) && !IsDebuggingVivaldi(command_line)))
    return false;
  // Default is now launching the Vivaldi App.
  // TODO(sverrir@vivaldi.com): Remove
  const Extension* extension = GetPlatformApp(profile, vivaldi::kVivaldiAppId);
  // If |vivaldi::kVivaldiAppId| is a disabled platform app we handle it
  // specially here, otherwise it will be handled below.
  if (extension) {
    RecordCmdLineAppHistogram(extensions::Manifest::TYPE_PLATFORM_APP);
#if defined(OS_WIN) || defined(OS_MACOSX)
    static bool vivaldi_updater_initialized = false;
    if (!vivaldi_updater_initialized) {
      const char kVivaldiAppCastUrl[] =
#if defined(_DEBUG) && defined(_DEBUG_AUTOUPDATE)
        // This is for debugging/testing in debug builds
#if defined(OS_MACOSX)
        "http://mirror.viv.osl/~jarle/sparkle/mac_appcast.xml";
#elif defined(_WIN64)
        "http://mirror.viv.osl/~jarle/sparkle/appcast.x64.xml";
#else
        "http://mirror.viv.osl/~jarle/sparkle/appcast.xml";
#endif
#elif defined(OFFICIAL_BUILD) && \
      (UPDATE_SOURCE_WIN(VIVALDI_RELEASE) == UPDATE_PREVIEW_SOURCE_WINDOWS)
        // This is the public TP/Beta/Final release channel
#if defined(OS_MACOSX)
        "https://update.vivaldi.com/update/1.0/public/mac/appcast.xml";
#elif defined(_WIN64)
        "https://update.vivaldi.com/update/1.0/public/appcast.x64.xml";
#else
        "https://update.vivaldi.com/update/1.0/public/appcast.xml";
#endif
#elif defined(OFFICIAL_BUILD)
        // This is the public snapshot release channel
#if defined(OS_MACOSX)
        "https://update.vivaldi.com/update/1.0/mac/appcast.xml";
#elif defined(_WIN64)
        "https://update.vivaldi.com/update/1.0/win/appcast.x64.xml";
#else
        "https://update.vivaldi.com/update/1.0/win/appcast.xml";
#endif
#else
        // This is the internal sopranos release channel
#if defined(OS_MACOSX)
        "https://update.vivaldi.com/update/1.0/sopranos_new/mac/appcast.xml";
#elif defined(_WIN64)
        "https://update.vivaldi.com/update/1.0/sopranos_new/appcast.x64.xml";
#else
        "https://update.vivaldi.com/update/1.0/sopranos_new/appcast.xml";
#endif
#endif
      GURL appcast_url(kVivaldiAppCastUrl);
      DCHECK(appcast_url.is_valid());
      bool show_vuu = false;

      if (command_line.HasSwitch(switches::kVivaldiUpdateURL)) {
        // Ref. VB-7983: If the --vuu switch is specified,
        // show the update url in a messagebox/stdout
        show_vuu = true;
        std::string vuu =
          command_line.GetSwitchValueASCII(switches::kVivaldiUpdateURL);
        if (!vuu.empty()) {
          GURL gurl(vuu);
          if (gurl.is_valid()) {
            // Use the supplied url for update.
            appcast_url = gurl;
          }
        }
      }
#if defined(OS_WIN)
      if (show_vuu) {
        ::MessageBoxA(NULL, appcast_url.spec().c_str(), "Vivaldi Update URL:",
          MB_ICONINFORMATION | MB_SETFOREGROUND);
      }

      const char kUpdateRegPath[] = "Software\\Vivaldi\\AutoUpdate";
      const wchar_t kVivaldiKey[] = L"Software\\Vivaldi\\AutoUpdate";
      const wchar_t kEnabled[] = L"Enabled";

      win_sparkle_set_appcast_url(appcast_url.spec().c_str());
      win_sparkle_set_registry_path(kUpdateRegPath);
      win_sparkle_set_automatic_check_for_updates(0);

      std::string locale = base::i18n::GetConfiguredLocale();
      win_sparkle_set_lang(locale.c_str());

      win_sparkle_init();

      base::win::RegKey key(HKEY_CURRENT_USER, kVivaldiKey, KEY_QUERY_VALUE);
      std::wstring enabled;
      if (key.Valid())
        key.ReadValue(kEnabled, &enabled);
      if (enabled == L"1") {
        base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
          base::Bind(&WinSparkleCheckForUpdates),
          base::TimeDelta::FromSeconds(kWinSparkleInitDelaySecs));
      }
#elif defined(OS_MACOSX)
      SparkleUtil::SetFeedURL(appcast_url.spec().c_str());
      if (show_vuu) {
        std::string feed_url = SparkleUtil::GetFeedURL();
        std::cout << "Vivaldi Update URL = " << feed_url.c_str() << std::endl;
      }
#endif
      vivaldi_updater_initialized = true;
    }
#endif
  }

  AppLaunchParams params(profile, extension,
                          extensions::LAUNCH_CONTAINER_NONE, NEW_WINDOW,
                          extensions::SOURCE_EXTENSIONS_PAGE);
  params.command_line = command_line;
  params.current_directory = cur_dir;

  ::OpenApplicationWithReenablePrompt(params);

  return true;
}


bool AddVivaldiNewPage(bool  welcome_run_none,
                       std::vector<GURL>* startup_urls) {
  if (!IsVivaldiRunning())
    return false;

  if (welcome_run_none) {
    // Don't open speed dial if we will open first run
    startup_urls->push_back(GetVivaldiNewTabURL());
  }

  return true;
}

} // namespace vivaldi
