// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_stats_db.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "components/ad_blocker/adblock_stats_data.h"
#include "sql/database.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace adblock_filter {

namespace {

// Table names use a macro instead of a const, so they can be used inline in
// other SQL statements below.
static constexpr base::cstring_view kAdBlockStatsTableName = "adblock_stats";

// AdBlock Stats Metadata keys
constexpr std::string_view kStatsMetadataReportingStart = "reporting_start";

// Metadata table version - update when table schemas are changed.
const int kStatsDatabaseVersionNumber = 1;

void AddMetadataKeys(sql::MetaTable& meta_table) {
  std::string reporting_start;
  if (!meta_table.GetValue(kStatsMetadataReportingStart, &reporting_start)) {
    meta_table.SetValue(kStatsMetadataReportingStart,
                        base::ToString(base::Time::Now()));
  }
}

bool CreateSchema(sql::Database* db) {
  static constexpr base::cstring_view kSqlCreateStatsTable =
      "CREATE TABLE IF NOT EXISTS adblock_stats"
      " (host_name VARCHAR NOT NULL,"
      " origin_host VARCHAR NOT NULL,"
      " time INTEGER NOT NULL,"
      " type INTEGER NOT NULL)";
  if (!db->Execute(kSqlCreateStatsTable)) {
    LOG(ERROR) << "Failed to create schema for " << kAdBlockStatsTableName;
    return false;
  }
  static constexpr base::cstring_view kSqlCreateIndexHostOnStatsTable =
      "CREATE INDEX IF NOT EXISTS adblock_stats"
      "_host_index on adblock_stats (host_name)";
  if (!db->Execute(kSqlCreateIndexHostOnStatsTable)) {
    LOG(ERROR) << "Failed to create index for " << kAdBlockStatsTableName;
    return false;
  }

  static constexpr base::cstring_view kSqlCreateIndexOriginOnStatsTable =
      "CREATE INDEX IF NOT EXISTS adblock_stats"
      "_origin_index on adblock_stats (origin_host)";
  if (!db->Execute(kSqlCreateIndexOriginOnStatsTable)) {
    LOG(ERROR) << "Failed to create index for " << kAdBlockStatsTableName;
    return false;
  }

  return true;
}

bool InitTables(sql::Database* db, sql::MetaTable& meta_table) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  if (!meta_table.Init(db, kStatsDatabaseVersionNumber,
                       kStatsDatabaseVersionNumber)) {
    return false;
  }

  if (!CreateSchema(db))
    return false;

  // Fail init when the compatable version number is bigger that we're
  // expecting. This should not happen unless Vivaldi is downgraded in the
  // future.
  if (meta_table.GetCompatibleVersionNumber() > kStatsDatabaseVersionNumber)
    return false;

  if (!transaction.Commit())
    return false;

  AddMetadataKeys(meta_table);
  return true;
}

// Adds a new AdBlock entry to the data base.
void AddEntryToDatabase(sql::Database* db,
                        std::string_view host_name,
                        std::string_view origin_host,
                        base::Time now,
                        RuleGroup group) {
  // Adds the new entry.
  static constexpr base::cstring_view kSqlInsert =
      "INSERT INTO adblock_stats"
      " (host_name, origin_host, time, type)"
      " VALUES (?, ?, ?, ?)";

  sql::Statement statement_insert(
      db->GetCachedStatement(SQL_FROM_HERE, kSqlInsert));
  statement_insert.BindString(0, host_name);
  statement_insert.BindString(1, origin_host);
  statement_insert.BindInt64(2, (now - base::Time()).InMicroseconds());
  statement_insert.BindInt(
      3, static_cast<std::underlying_type_t<RuleGroup>>(group));
  statement_insert.Run();
}

void LoadMetadata(sql::MetaTable& meta_table, StatsData* stats_data) {
  std::string reporting_start;
  if (meta_table.GetValue(kStatsMetadataReportingStart, &reporting_start)) {
    base::Time time;
    if (base::Time::FromString(reporting_start.c_str(), &time)) {
      stats_data->SetReportingStart(time);
    }
  }
}

template <StatsData::EntryType type>
constexpr base::cstring_view GetStatsDataSqlForType() {
  if constexpr (type == StatsData::EntryType::TRACKER_AND_ADS) {
    return "SELECT host_name, COUNT(*) AS total, SUM(CASE WHEN type = 1 THEN 1 "
           "ELSE 0 END) AS ad_count, SUM(CASE WHEN type = 0 THEN 1 ELSE 0 END) "
           "AS tracker_count FROM adblock_stats WHERE time >= ? AND time <= ? "
           "AND host_name != '' GROUP BY "
           "host_name ORDER BY total DESC";
  }
  if constexpr (type == StatsData::EntryType::WEBSITE) {
    return "SELECT origin_host, COUNT(*) AS total, SUM(CASE WHEN type = 1 THEN "
           "1 ELSE 0 END) AS ad_count, SUM(CASE WHEN type = 0 THEN 1 ELSE 0 "
           "END) AS "
           "tracker_count FROM adblock_stats WHERE time >= ? AND time <= ? AND "
           "origin_host != '' GROUP BY "
           "origin_host ORDER BY total DESC";
  }
  static_assert(true, "Unknown EntryType");
}

template <StatsData::EntryType type>
void GetStatsDataFromDatabase(sql::Database* db,
                              StatsData* stats_data,
                              base::Time begin_time,
                              base::Time end_time) {
  constexpr auto query = GetStatsDataSqlForType<type>();

  sql::Statement statement(db->GetUniqueStatement(query));
  statement.BindInt64(0, (begin_time - base::Time()).InMicroseconds());
  statement.BindInt64(1, (end_time - base::Time()).InMicroseconds());

  while (statement.Step()) {
    const auto entry =
        StatsData::Entry{statement.ColumnString(0), statement.ColumnInt64(2),
                         statement.ColumnInt64(3)};
    stats_data->AddEntry(entry, type);
  }
}

}  // namespace

StatsDatabase::StatsDatabase(const base::FilePath& path) : db_file_path_(path) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

StatsDatabase::~StatsDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool StatsDatabase::InitDatabase() {
  if (db_init_status_ == InitStatus::kSuccess)
    return true;

  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions(),
                                        sql::Database::Tag("AdBlockStats"));

  if (!db_->has_error_callback()) {
    db_->set_error_callback(base::BindRepeating(
        &StatsDatabase::DatabaseErrorCallback, base::Unretained(this)));
  }

  base::File::Error err;
  if (!base::CreateDirectoryAndGetError(db_file_path_.DirName(), &err)) {
    LOG(ERROR) << "Cannot init adblock stats data database dir: " << err;
    HandleInitializationFailure();
    return false;
  }

  if (!db_->Open(db_file_path_)) {
    LOG(ERROR) << "Failed to open adblock stats data database "
               << db_file_path_;
    HandleInitializationFailure();
    return false;
  }

  if (!InitTables(db_.get(), meta_table_)) {
    LOG(ERROR) << "Failed to create adblock stats tables";
    HandleInitializationFailure();
    return false;
  }

  db_init_status_ = InitStatus::kSuccess;
  return true;
}

void StatsDatabase::DatabaseErrorCallback(int extended_error,
                                          sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Attempt to recover a corrupt database, if it is eligible to be recovered.
  if (sql::Recovery::RecoverIfPossible(
          db_.get(), extended_error,
          sql::Recovery::Strategy::kRecoverWithMetaVersionOrRaze)) {
    // Signal the test-expectation framework that the
    // error was handled.
    std::ignore = sql::Database::IsExpectedSqliteError(extended_error);
    return;
  }

  if (!sql::Database::IsExpectedSqliteError(extended_error))
    LOG(ERROR) << db_->GetErrorMessage();

  // Consider the database closed if we did not attempt to recover so we did not
  // produce further errors.
  db_init_status_ = InitStatus::kFailure;
}

void StatsDatabase::HandleInitializationFailure() {
  db_.reset();
  db_init_status_ = InitStatus::kFailure;
}

void StatsDatabase::AddEntry(const GURL& url,
                             const std::string& origin_host,
                             base::Time now,
                             RuleGroup group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url.has_host());
  if (!InitDatabase()) {
    return;
  }

  AddEntryToDatabase(db_.get(), url.host_piece(), origin_host, now, group);
}

void StatsDatabase::ClearStatsData(base::Time begin_time, base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!InitDatabase()) {
    return;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    LOG(ERROR) << "Cannot begin transaction.";
    return;
  }

  static constexpr base::cstring_view kDeleteSql =
      "DELETE FROM adblock_stats WHERE time >= ? and time <= ?";

  sql::Statement deleteDataStatement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  deleteDataStatement.BindInt64(0,
                                (begin_time - base::Time()).InMicroseconds());
  deleteDataStatement.BindInt64(1, (end_time - base::Time()).InMicroseconds());
  deleteDataStatement.Run();

  static constexpr base::cstring_view kCheckForRowsSql =
      "SELECT * FROM adblock_stats LIMIT 1";

  sql::Statement checkForRowsStatement(
      db_->GetCachedStatement(SQL_FROM_HERE, kCheckForRowsSql));
  if (!checkForRowsStatement.Step() && checkForRowsStatement.Succeeded()) {
    meta_table_.SetValue(kStatsMetadataReportingStart,
                         base::ToString(base::Time::Now()));
  }
  transaction.Commit();
}

void StatsDatabase::ImportData(const StatsData& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!InitDatabase()) {
    return;
  }
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    LOG(ERROR) << "Cannot begin transaction.";
    return;
  }

  for (const auto& entry : data.TrackerEntries()) {
    for (int i = 0; i < entry.ad_count; ++i) {
      AddEntryToDatabase(db_.get(), entry.host, "", data.ReportingStart(),
                         RuleGroup::kAdBlockingRules);
    }
    for (int i = 0; i < entry.tracker_count; ++i) {
      AddEntryToDatabase(db_.get(), entry.host, "", data.ReportingStart(),
                         RuleGroup::kTrackingRules);
    }
  }

  for (const auto& entry : data.WebsiteEntries()) {
    const std::string origin = entry.host;
    for (int i = 0; i < entry.ad_count; ++i) {
      AddEntryToDatabase(db_.get(), "", entry.host, data.ReportingStart(),
                         RuleGroup::kAdBlockingRules);
    }
    for (int i = 0; i < entry.tracker_count; ++i) {
      AddEntryToDatabase(db_.get(), "", entry.host, data.ReportingStart(),
                         RuleGroup::kTrackingRules);
    }
  }

  meta_table_.SetValue(kStatsMetadataReportingStart,
                       base::ToString(data.ReportingStart()));
  transaction.Commit();
}

std::unique_ptr<StatsData> StatsDatabase::GetStatsData(base::Time begin_time,
                                                       base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!InitDatabase()) {
    return nullptr;
  }

  auto stats_data = std::make_unique<StatsData>();

  GetStatsDataFromDatabase<StatsData::EntryType::TRACKER_AND_ADS>(
      db_.get(), stats_data.get(), begin_time, end_time);
  GetStatsDataFromDatabase<StatsData::EntryType::WEBSITE>(
      db_.get(), stats_data.get(), begin_time, end_time);

  LoadMetadata(meta_table_, stats_data.get());

  return stats_data;
}

}  // namespace adblock_filter
