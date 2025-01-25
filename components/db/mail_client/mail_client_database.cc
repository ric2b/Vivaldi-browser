// Copyright (c) 2017 Vivaldi Technologies AS. All rights
// reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mail_client_database.h"

#include <stdint.h>

#include <algorithm>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

#if BUILDFLAG(IS_APPLE)
#include "base/apple/backup_util.h"
#endif

namespace mail_client {

namespace {

// Current version number. We write databases at the "current" version number,
// but any previous version that can read the "compatible" one can make do with
// our database without *too* many bad effects.
const int kCurrentVersionNumber = 3;
const int kCompatibleVersionNumber = 3;

sql::InitStatus LogMigrationFailure(int from_version) {
  LOG(ERROR) << "Mail Client DB failed to migrate from version "
             << from_version;
  return sql::INIT_FAILURE;
}

enum class InitStep {
  OPEN = 0,
  TRANSACTION_BEGIN = 1,
  META_TABLE_INIT = 2,
  CREATE_TABLES = 3,
  VERSION = 4,
  COMMIT = 5,
};

sql::InitStatus LogInitFailure(InitStep what) {
  base::UmaHistogramSparse("MailClient.InitializationFailureStep",
                           static_cast<int>(what));
  return sql::INIT_FAILURE;
}

}  // namespace

MailClientDatabase::MailClientDatabase()
    : db_({
          // Set the database page size to something a little larger to give us
           // better performance (we're typically seek rather than bandwidth
           // limited). Must be a power of 2 and a max of 65536.
           .page_size = 4096,
           // Set the cache size. The page size, plus a little extra, times this
           // value, tells us how much memory the cache will use maximum.
           // 1000 * 4kB = 4MB
           .cache_size = 1000,
           }) {}

MailClientDatabase::~MailClientDatabase() {}

sql::InitStatus MailClientDatabase::Init(
    const base::FilePath& mail_client_name) {
  db_.set_histogram_tag("mail");

  if (!db_.Open(mail_client_name))
    return sql::INIT_FAILURE;

  // Wrap the rest of init in a tranaction. This will prevent the database from
  // getting corrupted if we crash in the middle of initialization or migration.
  sql::Transaction committer(&db_);
  if (!committer.Begin())
    return sql::INIT_FAILURE;

#if BUILDFLAG(IS_APPLE)
  // Exclude the mail db file from backups.
  base::apple::SetBackupExclusion(mail_client_name);
#endif

  // Prime the cache.
  db_.Preload();

  if (!meta_table_.Init(&db_, GetCurrentVersion(), kCompatibleVersionNumber))
    return sql::INIT_FAILURE;

  if (!CreateMessageTable())
    return sql::INIT_FAILURE;

  // Version check.
  sql::InitStatus version_status = EnsureCurrentVersion();
  if (version_status != sql::INIT_OK) {
    LogInitFailure(InitStep::VERSION);
    return version_status;
  }

  return committer.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
}

void MailClientDatabase::Close() {
  db_.Close();
}

// static
int MailClientDatabase::GetCurrentVersion() {
  return kCurrentVersionNumber;
}

void MailClientDatabase::BeginTransaction() {
  db_.BeginTransactionDeprecated();
}

void MailClientDatabase::CommitTransaction() {
  db_.CommitTransactionDeprecated();
}

void MailClientDatabase::RollbackTransaction() {
  // If Init() returns with a failure status, the Transaction created there will
  // be destructed and rolled back. MailClientBackend might try to kill the
  // database after that, at which point it will try to roll back a non-existing
  // transaction. This will crash on a DCHECK. So transaction_nesting() is
  // checked first.
  if (db_.transaction_nesting())
    db_.RollbackTransactionDeprecated();
}

void MailClientDatabase::TrimMemory(bool aggressively) {
  db_.TrimMemory();
}

bool MailClientDatabase::Raze() {
  return db_.Raze();
}

sql::Database& MailClientDatabase::GetDB() {
  return db_;
}

sql::InitStatus MailClientDatabase::EnsureCurrentVersion() {
  // We can't read databases newer than we were designed for.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Mail database is too new.";
    return sql::INIT_TOO_NEW;
  }

  int cur_version = meta_table_.GetVersionNumber();

  if (cur_version == 1) {
    if (!UpdateToVersion2()) {
      return LogMigrationFailure(cur_version);
    }
    cur_version = 2;
    std::ignore = meta_table_.SetVersionNumber(cur_version);
    std::ignore = meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
  }

  if (cur_version == 2) {
    if (!UpdateToVersion3()) {
      return LogMigrationFailure(cur_version);
    }
    cur_version = 3;
    std::ignore = meta_table_.SetVersionNumber(cur_version);
    std::ignore = meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
  }

  return sql::INIT_OK;
}

}  // namespace mail_client
