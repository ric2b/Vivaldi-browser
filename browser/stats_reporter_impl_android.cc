// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//

#include "browser/stats_reporter_impl.h"

#include "base/files/file_path.h"
#include "base/logging.h"

namespace vivaldi {
// static
std::string StatsReporterImpl::GetUserIdFromLegacyStorage() {
  return std::string();
}

// static
base::FilePath StatsReporterImpl::GetReportingDataFileDir() {
  // We can't easily have a shared folder between multiple vivaldi installations
  // on Android as it would require extra permissions, so just don't try to
  // keep track of multiple installations on the same system.
  return base::FilePath();
}

}  // namespace vivaldi