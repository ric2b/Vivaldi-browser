// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_STATS_STORE_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_STATS_STORE_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/ad_blocker/adblock_stats_data.h"
#include "components/ad_blocker/adblock_types.h"

namespace adblock_filter {

using GetStatsDataCallback =
    base::OnceCallback<void(std::unique_ptr<StatsData>)>;

class StatsStore {
 public:
  virtual ~StatsStore();

  virtual void AddEntry(const GURL& url,
                        const std::string& origin_host,
                        base::Time now,
                        RuleGroup group) = 0;
  // Deletes all ad blocking stats in the store between |begin_time| and
  // |end_time|.
  virtual void ClearStatsData(base::Time begin_time, base::Time end_time) = 0;

  virtual void GetStatsData(base::Time begin_time,
                            base::Time end_time,
                            GetStatsDataCallback callback) const = 0;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_STATS_STORE_H_
