// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_STATS_DB_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_STATS_DB_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/ad_blocker/adblock_types.h"
#include "sql/meta_table.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace sql {
class Database;
class Statement;
}  // namespace sql

namespace adblock_filter {
class StatsData;

class StatsDatabase {
 public:
  StatsDatabase(const base::FilePath& database_dir);

  StatsDatabase(const StatsDatabase&) = delete;
  StatsDatabase& operator=(const StatsDatabase&) = delete;
  StatsDatabase(const StatsDatabase&&) = delete;
  StatsDatabase& operator=(const StatsDatabase&&) = delete;

  ~StatsDatabase();

  void AddEntry(const GURL& url,
                const std::string& origin_host,
                base::Time now,
                RuleGroup group);
  void ClearStatsData(base::Time begin_time, base::Time end_time);
  std::unique_ptr<StatsData> GetStatsData(base::Time begin_time,
                                          base::Time end_time);

  void ImportData(const StatsData& data);

 private:
  enum class InitStatus {
    kUnattempted = 0,
    kSuccess = 1,
    kFailure = 2,
  };

  bool InitDatabase() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void HandleInitializationFailure() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  // Path to the database on disk.
  const base::FilePath db_file_path_;

  InitStatus db_init_status_ GUARDED_BY_CONTEXT(sequence_checker_){
      InitStatus::kUnattempted};
  std::unique_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_STATS_DB_H_
