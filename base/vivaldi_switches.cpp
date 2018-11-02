// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

// Defines all the command-line switches used by Vivaldi.

#include "base/vivaldi_switches.h"

namespace switches {

// All switches in alphabetical order. The switches should be documented

const char kDebugVivaldi[] = "debug-vivaldi";
const char kDisableVivaldi[] = "disable-vivaldi";
const char kRunningVivaldi[] = "running-vivaldi";

// Specifies a custom URL for an appcast.xml file to be used for testing
// auto updates. This switch is for internal use only and the switch name is
// pseudonymous and hard to guess due to security reasons.
const char kVivaldiUpdateURL[] = "vuu";

// Add this switch to launch the Update Notifier for component (local) builds.
#if defined(COMPONENT_BUILD)
const char kLaunchUpdater[] = "launch-updater";
#endif

// This will delay exit with a minute to be able to test the dialog that opens
// on startup if Vivaldi is already running.
const char kTestAlreadyRunningDialog[] = "test-already-running-dialog";

}  // namespace switches
