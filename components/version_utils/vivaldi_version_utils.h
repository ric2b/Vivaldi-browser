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
int CompareVivaldiMajorVersions(const base::Version& lhs,
                                const base::Version& rhs);

// Compare the Vivaldi major version (major + minor revision) to the static
// version string.
// Version MUST be valid.
bool HasMajorVersionChanged(const base::Version& version);

// Compare the version, to the static version string.
// Version MUST be valid.
bool HasVersionChanged(const base::Version& version);

// Compare the static version to crash loop detection version pref.
// Returns true, if the reporting version changed.
bool HasCrashDetectionVersionChanged(PrefService* prefs);
// Save the static version to crash loop detection version pref.
void UpdateCrashDetectionVersion(PrefService* prefs);

}  // namespace version

}  // namespace vivaldi

#endif  // COMPONENTS_VERSION_UTILS_VIVALDI_VERSION_UTILS_H_
