// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "browser/init_sparkle.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/vivaldi_switches.h"

#if defined(OS_MAC)
#include "browser/mac/sparkle_util.h"
#endif

#define UPDATE_SOURCE_WIN_normal 0
#define UPDATE_SOURCE_WIN_snapshot 0
#define UPDATE_SOURCE_WIN_preview 1
#define UPDATE_SOURCE_WIN_beta 1
#define UPDATE_SOURCE_WIN_final 1

#define S(s) UPDATE_SOURCE_WIN_##s
#define UPDATE_SOURCE_WIN(s) S(s)

#define UPDATE_PREVIEW_SOURCE_WINDOWS 1

namespace init_sparkle {

namespace {

#if defined(OS_WIN) || defined(OS_MAC)
const char kVivaldiAppCastUrl[] =
#if defined(_DEBUG) && defined(_DEBUG_AUTOUPDATE)
// This is for debugging/testing in debug builds
#if defined(OS_MAC)
    "http://mirror.viv.osl/~jarle/sparkle/mac_appcast.xml";
#elif defined(_WIN64)
    "http://mirror.viv.osl/~jarle/sparkle/appcast.x64.xml";
#else
    "http://mirror.viv.osl/~jarle/sparkle/appcast.xml";
#endif
#elif defined(OFFICIAL_BUILD) && \
    (UPDATE_SOURCE_WIN(VIVALDI_RELEASE) == UPDATE_PREVIEW_SOURCE_WINDOWS)
// This is the public TP/Beta/Final release channel
#if defined(OS_MAC)
    "https://update.vivaldi.com/update/1.0/public/mac/appcast.xml";
#elif defined(_WIN64)
    "https://update.vivaldi.com/update/1.0/public/appcast.x64.xml";
#else
    "https://update.vivaldi.com/update/1.0/public/appcast.xml";
#endif
#elif defined(OFFICIAL_BUILD)
// This is the public snapshot release channel
#if defined(OS_MAC)
    "https://update.vivaldi.com/update/1.0/snapshot/mac/appcast.xml";
#elif defined(_WIN64)
    "https://update.vivaldi.com/update/1.0/win/appcast.x64.xml";
#else
    "https://update.vivaldi.com/update/1.0/win/appcast.xml";
#endif
#else
// This is the internal sopranos release channel
#if defined(OS_MAC)
    "https://update.vivaldi.com/update/1.0/sopranos_new/mac/appcast.xml";
#elif defined(_WIN64)
    "https://update.vivaldi.com/update/1.0/sopranos_new/appcast.x64.xml";
#else
    "https://update.vivaldi.com/update/1.0/sopranos_new/appcast.xml";
#endif
#endif
#endif
}  // anonymous namespace

Config GetConfig(const base::CommandLine& command_line) {
  Config config;
#if defined(OS_WIN) || defined(OS_MAC)
  config.appcast_url = GURL(kVivaldiAppCastUrl);
  DCHECK(config.appcast_url.is_valid());

  if (command_line.HasSwitch(switches::kVivaldiUpdateURL)) {
    // Ref. VB-7983: If the --vuu switch is specified,
    // show the update url in stdout
    std::string update_url =
        command_line.GetSwitchValueASCII(switches::kVivaldiUpdateURL);
    if (!update_url.empty()) {
      GURL gurl(update_url);
      if (gurl.is_valid()) {
        // Use the supplied url for update.
        config.appcast_url = std::move(gurl);
        config.with_custom_url = true;
        LOG(INFO) << "Vivaldi Update URL: " << config.appcast_url.spec();
      }
    }
  }
#endif
  return config;
}

#if defined(OS_MAC)
void Initialize(const base::CommandLine& command_line) {
  static bool vivaldi_updater_initialized = false;
  if (!vivaldi_updater_initialized) {
    Config config = GetConfig(command_line);
    SparkleUtil::SetFeedURL(config.appcast_url.spec().c_str());
    vivaldi_updater_initialized = true;
  }
}
#endif

}  // namespace init_sparkle
