// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "browser/init_sparkle.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/vivaldi_switches.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include <iostream>
#include "browser/mac/sparkle_util.h"
#endif

#if defined(OS_WIN)
#include "base/i18n/rtl.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "third_party/_winsparkle_lib/include/winsparkle.h"
#include "update_notifier/update_notifier_switches.h"
#endif

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
#if defined(OS_WIN)

const char kUpdateRegPath[] = "Software\\Vivaldi\\AutoUpdate";
const wchar_t kVivaldiKey[] = L"Software\\Vivaldi\\AutoUpdate";
const wchar_t kEnabled[] = L"Enabled";

const int kWinSparkleInitDelaySecs = 20;

void WinSparkleCheckForUpdates(base::Callback<bool()> should_check_update_cb,
                               bool force_check) {
  const int interval_secs = win_sparkle_get_update_check_interval();
  const time_t last_check_time = win_sparkle_get_last_check_time();
  const time_t current_time = time(NULL);

  // Only check for updates in reasonable intervals.
  // If |force_check| is true, an update request will be sent regardless of
  // the last check time.
  if ((force_check || current_time - last_check_time >= interval_secs) &&
      (should_check_update_cb.is_null() ||
       should_check_update_cb.Run())) {
    win_sparkle_check_update_without_ui();
  }

  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WinSparkleCheckForUpdates, should_check_update_cb, false),
      base::TimeDelta::FromSeconds(interval_secs));
}
#endif

// #define _DEBUG_AUTOUPDATE

#if defined(OS_WIN) || defined(OS_MACOSX)
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
#endif
}  // anonymous namespace

void InitializeSparkle(const base::CommandLine& command_line,
                       base::Callback<bool()> should_check_update_callback) {
#if defined(OS_WIN) || defined(OS_MACOSX)
  static bool vivaldi_updater_initialized = false;
  if (!vivaldi_updater_initialized) {
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

    win_sparkle_set_appcast_url(appcast_url.spec().c_str());
    win_sparkle_set_registry_path(kUpdateRegPath);
    win_sparkle_set_automatic_check_for_updates(0);

    std::string locale = base::i18n::GetConfiguredLocale();
    win_sparkle_set_lang(locale.c_str());

    win_sparkle_init();

    // Do not force a delayed check for updates on init if we are launched with
    // the check-for-updates command line parameter, "--c".
    bool force_update_on_init =
        !command_line.HasSwitch(vivaldi_update_notifier::kCheckForUpdates);

    base::win::RegKey key(HKEY_CURRENT_USER, kVivaldiKey, KEY_QUERY_VALUE);
    std::wstring enabled;
    if (key.Valid())
      key.ReadValue(kEnabled, &enabled);
    if (enabled == L"1") {
      base::MessageLoop::current()->task_runner()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&WinSparkleCheckForUpdates,
                     should_check_update_callback,
                     force_update_on_init),
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
}  // namespace vivaldi
