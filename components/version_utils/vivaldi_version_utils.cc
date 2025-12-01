// Copyright (c) 2024 Vivaldi. All rights reserved.
#include "components/version_utils/vivaldi_version_utils.h"

#include "app/vivaldi_version_info.h"

#include "base/check.h"
#include "base/version.h"

#include "components/prefs/pref_service.h"
#include "prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

namespace version {

int CompareVivaldiMajorVersions(const base::Version& lhs,
                                const base::Version& rhs) {
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

bool HasMajorVersionChanged(const base::Version& version) {
  DCHECK(version.IsValid());
  const base::Version static_version = ::vivaldi::GetVivaldiVersion();
  return CompareVivaldiMajorVersions(version, version) != 0;
}

bool HasVersionChanged(const base::Version& version) {
  DCHECK(version.IsValid());
  const base::Version static_version = ::vivaldi::GetVivaldiVersion();
  // Version changed when the version is lower that the
  // static version.
  return version.CompareTo(version) < 0;
}

bool HasCrashDetectionVersionChanged(PrefService* prefs) {
  DCHECK(prefs);
  const base::Version last_seen_version =
      prefs->HasPrefPath(vivaldiprefs::kStartupCrashDetectionLastSeenVersion)
          ? base::Version(prefs->GetString(
                vivaldiprefs::kStartupCrashDetectionLastSeenVersion))
          : base::Version();

  return !last_seen_version.IsValid() || HasVersionChanged(last_seen_version);
}

void UpdateCrashDetectionVersion(PrefService* prefs) {
  DCHECK(prefs);
  prefs->SetString(vivaldiprefs::kStartupCrashDetectionLastSeenVersion,
                   ::vivaldi::GetVivaldiVersionString());
  // Make sure that the version gets written to disk before the browser is
  // killed.
  prefs->CommitPendingWrite();
}

}  // namespace version

}  // namespace vivaldi
