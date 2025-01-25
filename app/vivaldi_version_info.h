// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef APP_VIVALDI_VERSION_INFO_H_
#define APP_VIVALDI_VERSION_INFO_H_

/*
* NOTE! NOTE! NOTE!
*
* Sources in targets using this header file MUST add the below
* in the relevant GN target:
*
*   configs += [
*     "//vivaldi/gn/config:release_kind"
*   ]
*
*/

#include <string>

// Defines used when determining build versions.
#define BUILD_VERSION_normal 0
#define BUILD_VERSION_snapshot 0
#define BUILD_VERSION_preview 0
#define BUILD_VERSION_beta 1
#define BUILD_VERSION_final 1

#define S(s) BUILD_VERSION_##s
#define BUILD_VERSION(s) S(s)

// Based on parsing the branch name; only official can be non-normal
#define BUILD_RELEASE_normal Release::kSopranos
#define BUILD_RELEASE_snapshot Release::kSnapshot
#define BUILD_RELEASE_preview Release::kTechnologyPreview
#define BUILD_RELEASE_beta Release::kBeta
#define BUILD_RELEASE_final Release::kFinal

#define S1(s) BUILD_RELEASE_##s
#define BUILD_RELEASE_KIND(s) S1(s)

#define VIVALDI_BUILD_PUBLIC_RELEASE 1

#define RELEASE_TYPE_ID_vivaldi_sopranos 0
#define RELEASE_TYPE_ID_vivaldi_snapshot 1
#define RELEASE_TYPE_ID_vivaldi_final 2

#define S2(s) RELEASE_TYPE_ID_##s
#define TO_RELEASE_TYPE_ID(s) S2(s)

namespace base {
class Version;
}
class PrefService;

namespace vivaldi {

enum class Release {
  // Internal dev builds are also sopranos builds, uses the green icon
  kInternal,
  kSopranos = kInternal,

  // Kinds listed below are only set for Official builds

  // Snapshots are Official builds, using the black icon
  kSnapshot,

  // Currently not used. Used for Technology previews. Official build
  kTechnologyPreview,

  // Currently not used Betas are Official builds
  kBeta,

  // Final are Official builds to be shipped to general users. Red Icon
  kFinal,
};

// Returns a version string to be displayed in "About Vivaldi" dialog.
std::string GetVivaldiVersionString();

// Return the Vivaldi build version as base::Version instance.
const base::Version& GetVivaldiVersion();

Release ReleaseKind();

}  // namespace vivaldi

#endif  // APP_VIVALDI_VERSION_INFO_H_
