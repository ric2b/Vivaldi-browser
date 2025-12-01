// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "browser/init_sparkle.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/vivaldi_switches.h"
#include "base/win/windows_version.h"

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
const char kVivaldiAppCastUrl[] =
#if defined(OFFICIAL_BUILD) && \
    (UPDATE_SOURCE_WIN(VIVALDI_RELEASE) == UPDATE_PREVIEW_SOURCE_WINDOWS)
#define WIN_ARM64_APPCAST_URL "https://update.vivaldi.com/update/1.0/public/appcast.arm64.xml"
#define WIN_X64_APPCAST_URL "https://update.vivaldi.com/update/1.0/public/appcast.x64.xml"
// This is the public TP/Beta/Final release channel
#if BUILDFLAG(IS_MAC)
    "https://update.vivaldi.com/update/1.0/public/mac/appcast.xml";
#elif defined(_WIN64)  && defined(_M_ARM64)
    WIN_ARM64_APPCAST_URL;
#elif defined(_WIN64)
    WIN_X64_APPCAST_URL;
#else
    "https://update.vivaldi.com/update/1.0/public/appcast.xml";
#endif
#elif defined(OFFICIAL_BUILD)
// This is the public snapshot release channel
#define WIN_ARM64_APPCAST_URL "https://update.vivaldi.com/update/1.0/win/appcast.arm64.xml"
#define WIN_X64_APPCAST_URL "https://update.vivaldi.com/update/1.0/win/appcast.x64.xml"
#if BUILDFLAG(IS_MAC)
    "https://update.vivaldi.com/update/1.0/snapshot/mac/appcast.xml";
#elif defined(_WIN64) && defined(_M_ARM64)
    WIN_ARM64_APPCAST_URL;
#elif defined(_WIN64)
    WIN_X64_APPCAST_URL;
#else
    "https://update.vivaldi.com/update/1.0/win/appcast.xml";
#endif
#else
// This is the internal sopranos release channel
#define WIN_ARM64_APPCAST_URL "https://update.vivaldi.com/update/1.0/sopranos_new/appcast.arm64.xml"
#define WIN_X64_APPCAST_URL "https://update.vivaldi.com/update/1.0/sopranos_new/appcast.x64.xml"
#if BUILDFLAG(IS_MAC)
    "https://update.vivaldi.com/update/1.0/sopranos_new/mac/appcast.xml";
#elif defined(_WIN64) && defined(_M_ARM64)
    WIN_ARM64_APPCAST_URL;
#elif defined(_WIN64)
    WIN_X64_APPCAST_URL;
#else
    "https://update.vivaldi.com/update/1.0/sopranos_new/appcast.xml";
#endif
#endif
#endif
}  // anonymous namespace

GURL GetAppcastUrl() {
  GURL url;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  url = GURL(kVivaldiAppCastUrl);

#if BUILDFLAG(IS_WIN) && !defined(_M_ARM64)
#if defined(_WIN64)
  // Check if win64 is running on arm64
  if (base::win::OSInfo::GetInstance()->IsWowAMD64OnARM64()) {
    url = GURL(WIN_ARM64_APPCAST_URL);
    LOG(ERROR) << "Sparkle: x64 is running on arm64: Appcast changed from win64 to arm64 variant " << url.possibly_invalid_spec();
  }
#else // End Win x64
  // Check if win32 is running in Win64 or on an Arm64 device
  if (base::win::OSInfo::GetInstance()->IsWowX86OnAMD64()) {
    url = GURL(WIN_X64_APPCAST_URL);
    LOG(ERROR) << "Sparkle: x86 is running on arm64: Appcast changed from win32 to win64 variant "
               << url.possibly_invalid_spec();
  } else if (base::win::OSInfo::GetInstance()->IsWowX86OnARM64()) {
    url = GURL(WIN_ARM64_APPCAST_URL);
    LOG(ERROR) << "Sparkle: x86 is running on arm64: Appcast changed from win32 to win-arm64 variant "
               << url.possibly_invalid_spec();
  }
#endif // End Win32
#endif // End Windows mode check

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

}  // namespace init_sparkle
