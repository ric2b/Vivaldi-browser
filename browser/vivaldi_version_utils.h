// Copyright (c) 2024 Vivaldi. All rights reserved.

#ifndef BROWSER_VIVALDI_VERSION_UTILS_H_
#define BROWSER_VIVALDI_VERSION_UTILS_H_

class PrefService;

namespace vivaldi {

namespace version {

// Compare the Vivaldi major version (major + minor revision) to the static
// version string. This function must be called before pref version update to
// return a valid result.
bool HasMajorVersionChanged(PrefService* prefs);

// Compare the version stored in prefs, to the static version string.
// This function must be called before pref version update to return a valid
// result.
bool HasVersionChanged(PrefService* prefs);

}  // namespace version

}  // namespace vivaldi

#endif  // BROWSER_VIVALDI_VERSION_UTILS_H_
