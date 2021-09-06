// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef APP_VIVALDI_VERSION_INFO_H_
#define APP_VIVALDI_VERSION_INFO_H_

#include <string>

// Defines used when determining build versions.
#define BUILD_VERSION_normal 0
#define BUILD_VERSION_snapshot 0
#define BUILD_VERSION_preview 0
#define BUILD_VERSION_beta 1
#define BUILD_VERSION_final 1

#define S(s) BUILD_VERSION_##s
#define BUILD_VERSION(s) S(s)

#define VIVALDI_BUILD_PUBLIC_RELEASE 1

namespace base {
class Version;
}

namespace vivaldi {

// Returns a version string to be displayed in "About Vivaldi" dialog.
std::string GetVivaldiVersionString();

// Return the Vivaldi build version as base::Version instance.
const base::Version& GetVivaldiVersion();

// Returns true when running a build that is a beta or a final.
bool IsBetaOrFinal();

}  // namespace vivaldi

#endif  // APP_VIVALDI_VERSION_INFO_H_
