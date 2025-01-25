// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

// Defines all the command-line switches used by Vivaldi.

#ifndef BASE_VIVALDI_SWITCHES_H_
#define BASE_VIVALDI_SWITCHES_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX)
// Needed for
// chromium/services/service_manager/zygote/host/zygote_communication_linux.cc
#include "base/base_export.h"

#define SWITCHES_EXPORT BASE_EXPORT
#else
#define SWITCHES_EXPORT
#endif

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.

SWITCHES_EXPORT extern const char kAutoTestMode[];
SWITCHES_EXPORT extern const char kDisableVivaldi[];

#if defined(COMPONENT_BUILD)
SWITCHES_EXPORT extern const char kLaunchUpdater[];
#endif

SWITCHES_EXPORT extern const char kOverrideStatsReporterPingUrl[];

SWITCHES_EXPORT extern const char kRunningVivaldi[];

SWITCHES_EXPORT extern const char kTestAlreadyRunningDialog[];
SWITCHES_EXPORT extern const char kTranslateLanguageListUrl[];
SWITCHES_EXPORT extern const char kTranslateServerUrl[];

SWITCHES_EXPORT extern const char kVivaldiSilentUpdate[];
SWITCHES_EXPORT extern const char kVivaldiUpdateURL[];

#if BUILDFLAG(IS_WIN)
// Only used from the Windows installer to do a clean shutdown.
SWITCHES_EXPORT extern const char kCleanShutdown[];
#endif //IS_WIN

}  // namespace switches

#endif  // BASE_VIVALDI_SWITCHES_H_
