// Copyright (c) 2024 Vivaldi. All rights reserved.
#include "browser/vivaldi_version_utils.h"

#include "app/vivaldi_version_info.h"

#include "base/check.h"
#include "base/version.h"

#include "components/prefs/pref_service.h"
#include "prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

namespace version {

bool HasMajorVersionChanged(PrefService* prefs) {
  DCHECK(prefs);
  static std::optional<bool> version_changed;
  if (!version_changed) {
    const base::Version version = ::vivaldi::GetVivaldiVersion();
    const base::Version last_seen_version =
        base::Version(prefs->GetString(vivaldiprefs::kStartupLastSeenVersion));

    const auto version_components = version.components();
    const auto last_seen_version_components = last_seen_version.components();

    auto compare_vivaldi_major = [](const auto& rhs, const auto& lhs) {
      if (rhs.size() < 2 || lhs.size() < 2) {
        // Invalid version
        return false;
      }
      if (rhs.at(0) < lhs.at(0)) {
        // rhs major version < lhs major version, e.g. 6.X < 7.X.
        return true;
      }
      if (rhs.at(0) == lhs.at(0) && rhs.at(1) < lhs.at(1)) {
        // rhs minor version < lhs minor version, e.g. 7.1 < 7.2.
        return true;
      }
      return false;
    };
    // Major version changed when the last seen version from prefs is lower than
    // the static version - comparing only major and minor revision.
    version_changed =
        compare_vivaldi_major(last_seen_version_components, version_components);
  }
  return *version_changed;
}

bool HasVersionChanged(PrefService* prefs) {
  DCHECK(prefs);
  static std::optional<bool> version_changed;
  if (!version_changed) {
    const base::Version version = ::vivaldi::GetVivaldiVersion();
    const base::Version last_seen_version =
        base::Version(prefs->GetString(vivaldiprefs::kStartupLastSeenVersion));

    // Version changed when the last seen version from prefs is lower that the
    // static version, or pref version is invalid.
    version_changed = !last_seen_version.IsValid() ||
                      last_seen_version.CompareTo(version) < 0;
  }
  return *version_changed;
}

}  // namespace version

}  // namespace vivaldi
