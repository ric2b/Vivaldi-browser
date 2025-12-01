#include "components/ad_blocker/adblock_stats_store_impl.h"

#include "base/files/file_path.h"
#include "base/task/thread_pool.h"
#include "components/ad_blocker/adblock_stats_db.h"

namespace adblock_filter {

namespace {

const base::FilePath::CharType kAdBlockStatsDatabaseFilename[] =
    FILE_PATH_LITERAL("adblock_stats.sqlite");

}  // namespace

StatsStoreImpl::StatsStoreImpl(const base::FilePath& profile_path) {
  // Get the background thread to run SQLite on.
  db_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  const auto stats_db_path = profile_path.Append(kAdBlockStatsDatabaseFilename);

  stats_database_ =
      base::SequenceBound<StatsDatabase>(db_task_runner_, stats_db_path);
}

StatsStoreImpl::~StatsStoreImpl() = default;

void StatsStoreImpl::AddEntry(const GURL& url,
                              const std::string& origin_host,
                              base::Time now,
                              RuleGroup group) {
  stats_database_.AsyncCall(&StatsDatabase::AddEntry)
      .WithArgs(url, origin_host, now, group);
}

void StatsStoreImpl::ClearStatsData(base::Time begin_time,
                                    base::Time end_time) {
  stats_database_.AsyncCall(&StatsDatabase::ClearStatsData)
      .WithArgs(begin_time, end_time);
}

void StatsStoreImpl::ImportData(StatsData data) {
  stats_database_.AsyncCall(&StatsDatabase::ImportData).WithArgs(std::move(data));
}

void StatsStoreImpl::GetStatsData(base::Time begin_time,
                                  base::Time end_time,
                                  GetStatsDataCallback callback) const {
  stats_database_.AsyncCall(&StatsDatabase::GetStatsData)
      .WithArgs(begin_time, end_time)
      .Then(std::move(callback));
}

}  // namespace adblock_filter
