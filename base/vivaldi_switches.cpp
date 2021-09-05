// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

// Defines all the command-line switches used by Vivaldi.

#include "base/vivaldi_switches.h"

namespace switches {

// All switches in alphabetical order. The switches should be documented

// To be used to disable code that interferes automatic tests.
const char kAutoTestMode[] = "auto-test-mode";
const char kDebugVivaldi[] = "debug-vivaldi";
const char kDisableVivaldi[] = "disable-vivaldi";
const char kRunningVivaldi[] = "running-vivaldi";

// Use this on Mac to force a particular media pipeline implementation. The
// alloweed values are player to force older AVPlayer based code and reader to
// select newer AVAssetReader.
const char kVivaldiPlatformMedia[] = "platform-media";

// The installer should perform updates completely silently and should not
// terminate running browser instances. The name is criptic as it is not
// intended to be used by the end-user.
const char kVivaldiSilentUpdate[] = "vsu";

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
