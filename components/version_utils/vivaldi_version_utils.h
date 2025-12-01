// Copyright (c) 2024 Vivaldi. All rights reserved.

#ifndef COMPONENTS_VERSION_UTILS_VIVALDI_VERSION_UTILS_H_
#define COMPONENTS_VERSION_UTILS_VIVALDI_VERSION_UTILS_H_

class PrefService;

namespace base {
class Version;
}

namespace vivaldi {

namespace version {

// Compare Vivaldi major versions. Returns -1, 0, 1 for <, ==, >.
// Both versions MUST be valid.
int CompareVivaldiMajorVersions(base::Version lhs, base::Version rhs);

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

#endif  // COMPONENTS_VERSION_UTILS_VIVALDI_VERSION_UTILS_H_
