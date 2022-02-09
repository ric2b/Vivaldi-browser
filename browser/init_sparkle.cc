// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "browser/init_sparkle.h"

#include <string>

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
#if defined(OFFICIAL_BUILD) && \
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

GURL GetAppcastUrl() {
  GURL url;
#if defined(OS_WIN) || defined(OS_MAC)
  url = GURL(kVivaldiAppCastUrl);
  DCHECK(url.is_valid());

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kVivaldiUpdateURL)) {
    // Ref. VB-7983: If the --vuu switch is specified,
    // show the update url in stdout
    std::string url_string =
        command_line.GetSwitchValueASCII(switches::kVivaldiUpdateURL);
    if (!url_string.empty()) {
      GURL commnad_line_url(url_string);
      if (commnad_line_url.is_valid()) {
        url = std::move(commnad_line_url);
        LOG(INFO) << "Vivaldi Update URL: " << url.spec();
      }
    }
  }
#endif
  return url;
}

#if defined(OS_MAC)
void Initialize() {
  static bool vivaldi_updater_initialized = false;
  if (!vivaldi_updater_initialized) {
    GURL url = GetAppcastUrl();
    SparkleUtil::SetFeedURL(url.spec().c_str());
    vivaldi_updater_initialized = true;
  }
}
#endif

}  // namespace init_sparkle
