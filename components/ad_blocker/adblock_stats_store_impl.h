// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_STATS_STORE_IMPL_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_STATS_STORE_IMPL_H_

#include "base/threading/sequence_bound.h"
#include "components/ad_blocker/adblock_stats_db.h"
#include "components/ad_blocker/adblock_stats_store.h"
#include "components/ad_blocker/adblock_types.h"

namespace base {
class FilePath;
}

namespace adblock_filter {

class StatsStoreImpl : public StatsStore {
 public:
  explicit StatsStoreImpl(const base::FilePath& profile_path);
  ~StatsStoreImpl() override;

  // StatsStore:
  void AddEntry(const GURL& url,
                const std::string& origin_host,
                base::Time now,
                RuleGroup group) override;
  void ClearStatsData(base::Time begin_time, base::Time end_time) override;

  void GetStatsData(base::Time begin_time,
                    base::Time end_time,
                    GetStatsDataCallback callback) const override;

  // Used only for migration.
  void ImportData(StatsData data);

 private:
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  base::SequenceBound<StatsDatabase> stats_database_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_STATS_STORE_IMPL_H_
