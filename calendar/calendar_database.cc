// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar_database.h"

#include <stdint.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif

namespace calendar {

namespace {

// Current version number. We write databases at the "current" version number,
// but any previous version that can read the "compatible" one can make do with
// our database without *too* many bad effects.
const int kCurrentVersionNumber = 1;
const int kCompatibleVersionNumber = 1;

}  // namespace

CalendarDatabase::~CalendarDatabase() {}

sql::InitStatus CalendarDatabase::Init(const base::FilePath& calendar_name) {
  db_.set_histogram_tag("Calendar");

  // Set the database page size to something a little larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192.
  db_.set_page_size(4096);

  // Set the cache size. The page size, plus a little extra, times this
  // value, tells us how much memory the cache will use maximum.
  // 1000 * 4kB = 4MB
  // TODO(brettw) scale this value to the amount of available memory.
  db_.set_cache_size(1000);

  // Note that we don't set exclusive locking here. That's done by
  // BeginExclusiveMode below which is called later (we have to be in shared
  // mode to start out for the in-memory backend to read the data).

  if (!db_.Open(calendar_name))
    return sql::INIT_FAILURE;

  // Wrap the rest of init in a tranaction. This will prevent the database from
  // getting corrupted if we crash in the middle of initialization or migration.
  sql::Transaction committer(&db_);
  if (!committer.Begin())
    return sql::INIT_FAILURE;

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Exclude the calendar file from backups.
  base::mac::SetFileBackupExclusion(calendar_name);
#endif

  // Prime the cache.
  db_.Preload();

  // Create the tables and indices.
  // NOTE: If you add something here, also add it to
  //       RecreateAllButStarAndURLTables.
  if (!meta_table_.Init(&db_, GetCurrentVersion(), kCompatibleVersionNumber))
    return sql::INIT_FAILURE;

  if (!CreateCalendarTable() || !CreateEventTable() || !CreateRecurringTable())
    return sql::INIT_FAILURE;

  CreateDefaultCalendar();

  return committer.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
}

void CalendarDatabase::BeginExclusiveMode() {
  // We can't use set_exclusive_locking() since that only has an effect before
  // the DB is opened.
  ignore_result(db_.Execute("PRAGMA locking_mode=EXCLUSIVE"));
}

// static
int CalendarDatabase::GetCurrentVersion() {
  return kCurrentVersionNumber;
}

void CalendarDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void CalendarDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

void CalendarDatabase::RollbackTransaction() {
  // If Init() returns with a failure status, the Transaction created there will
  // be destructed and rolled back. CalendarBackend might try to kill the
  // database after that, at which point it will try to roll back a non-existing
  // transaction. This will crash on a DCHECK. So transaction_nesting() is
  // checked first.
  if (db_.transaction_nesting())
    db_.RollbackTransaction();
}

void CalendarDatabase::Vacuum() {
  DCHECK_EQ(0, db_.transaction_nesting())
      << "Can not have a transaction when vacuuming.";
  ignore_result(db_.Execute("VACUUM"));
}

void CalendarDatabase::TrimMemory(bool aggressively) {
  db_.TrimMemory(aggressively);
}

bool CalendarDatabase::Raze() {
  return db_.Raze();
}

sql::Connection& CalendarDatabase::GetDB() {
  return db_;
}

}  // namespace calendar
