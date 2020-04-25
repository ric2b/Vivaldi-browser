// Copyright (c) 2019 Vivaldi. All rights reserved.

#ifndef BROWSER_STATS_REPORTER_H_
#define BROWSER_STATS_REPORTER_H_

#include <memory>

#include "base/macros.h"

namespace vivaldi {

class StatsReporter {
 public:
  StatsReporter() = default;
  virtual ~StatsReporter() = default;

  static std::unique_ptr<StatsReporter> CreateInstance();

 private:
  DISALLOW_COPY_AND_ASSIGN(StatsReporter);
};

}  // namespace vivaldi

#endif  // BROWSER_STATS_REPORTER_H_
