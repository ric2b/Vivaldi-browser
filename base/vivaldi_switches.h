// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

// Defines all the command-line switches used by Vivaldi.

#ifndef BASE_VIVALDI_SWITCHES_H_
#define BASE_VIVALDI_SWITCHES_H_

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.

extern const char kDebugVivaldi[];
extern const char kDisableVivaldi[];
extern const char kRunningVivaldi[];

extern const char kVivaldiUpdateURL[];
#if defined(COMPONENT_BUILD)
extern const char kLaunchUpdater[];
#endif
extern const char kTestAlreadyRunningDialog[];

}  // namespace switches

#endif  // BASE_VIVALDI_SWITCHES_H_
