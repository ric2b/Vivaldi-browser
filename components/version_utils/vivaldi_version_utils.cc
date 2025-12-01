// Copyright (c) 2024 Vivaldi. All rights reserved.
#include "components/version_utils/vivaldi_version_utils.h"

#include "app/vivaldi_version_info.h"

#include "base/check.h"
#include "base/version.h"

#include "components/prefs/pref_service.h"
#include "prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

namespace version {

int CompareVivaldiMajorVersions(base::Version lhs, base::Version rhs) {
  DCHECK(lhs.IsValid());
  DCHECK(rhs.IsValid());
  auto lhs_components = lhs.components();
  auto rhs_components = rhs.components();
  lhs_components.resize(2);
  rhs_components.resize(2);

  base::Version major_lhs(lhs_components);
  base::Version major_rhs(rhs_components);

  return major_lhs.CompareTo(major_rhs);
}

bool HasMajorVersionChanged(PrefService* prefs) {
  DCHECK(prefs);
  static std::optional<bool> version_changed;
  if (!version_changed) {
    const base::Version version = ::vivaldi::GetVivaldiVersion();
    const base::Version last_seen_version =
        base::Version(prefs->GetString(vivaldiprefs::kStartupLastSeenVersion));
    version_changed =
        (version.IsValid() && last_seen_version.IsValid())
            ? CompareVivaldiMajorVersions(last_seen_version, version) != 0
            : false;
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
