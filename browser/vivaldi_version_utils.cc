// Copyright (c) 2024 Vivaldi. All rights reserved.
#include "browser/vivaldi_version_utils.h"

#include "app/vivaldi_version_info.h"

#include "base/check.h"
#include "base/version.h"

#include "components/prefs/pref_service.h"
#include "prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

namespace version {

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
