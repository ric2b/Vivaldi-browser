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

}  // namespace switches
