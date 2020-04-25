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
  NOTIMPLEMENTED();
  return base::FilePath();
}

}  // namespace vivaldi