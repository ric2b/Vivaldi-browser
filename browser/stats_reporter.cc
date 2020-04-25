#include "browser/stats_reporter.h"

#include "browser/stats_reporter_impl.h"

namespace vivaldi {

// static
std::unique_ptr<StatsReporter> StatsReporter::CreateInstance() {
  return std::make_unique<StatsReporterImpl>();
}

}  // namespace vivaldi