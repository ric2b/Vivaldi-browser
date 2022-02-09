// Copyright (c) 2019 Vivaldi. All rights reserved.

#ifndef BROWSER_STATS_REPORTER_H_
#define BROWSER_STATS_REPORTER_H_

#include <memory>

namespace vivaldi {

class StatsReporter {
 public:
  StatsReporter() = default;

  StatsReporter(const StatsReporter&) = delete;
  StatsReporter& operator=(const StatsReporter&) = delete;

  virtual ~StatsReporter() = default;

  static std::unique_ptr<StatsReporter> CreateInstance();
};

}  // namespace vivaldi

#endif  // BROWSER_STATS_REPORTER_H_
