// Copyright (c) 2017-2021 Vivaldi Technologies AS. All rights reserved

// Defines all the command-line switches used by Vivaldi.

#include "base/vivaldi_switches.h"

namespace switches {

// All switches in alphabetical order. The switches should be documented

// To be used to disable code that interferes automatic tests.
const char kAutoTestMode[] = "auto-test-mode";

// To be used to disable code that interferes automatic tests.
const char kDisableVivaldi[] = "disable-vivaldi";

// Add this switch to launch the Update Notifier for component (local) builds.
#if defined(COMPONENT_BUILD)
const char kLaunchUpdater[] = "launch-updater";
#endif

// Overrides the url used to send pings by the stats reporter. Only active for
// non-official builds.
const char kOverrideStatsReporterPingUrl[] = "override-stats-reporter-ping-url";

// To be used to disable code that interferes automatic tests.
const char kRunningVivaldi[] = "running-vivaldi";

// This will delay exit with a minute to be able to test the dialog that opens
// on startup if Vivaldi is already running.
const char kTestAlreadyRunningDialog[] = "test-already-running-dialog";

// Alternative language list url for translations.
const char kTranslateLanguageListUrl[] = "translate-language-list-url";

// Alternative translation server url.
const char kTranslateServerUrl[] = "translate-server-url";

// The installer should perform updates completely silently and should not
// terminate running browser instances. The name is criptic as it is not
// intended to be used by the end-user.
const char kVivaldiSilentUpdate[] = "vsu";

// Specifies a custom URL for an appcast.xml file to be used for testing
// auto updates. This switch is for internal use only and the switch name is
// pseudonymous and hard to guess due to security reasons.
const char kVivaldiUpdateURL[] = "vuu";

#if BUILDFLAG(IS_WIN)
const char kCleanShutdown[] = "do-clean-shutdown";
#endif //IS_WIN

}  // namespace switches
