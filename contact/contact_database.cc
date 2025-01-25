// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact_database.h"

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

namespace contact {

namespace {

// Current version number. We write databases at the "current" version number,
// but any previous version that can read the "compatible" one can make do with
// our database without *too* many bad effects.
const int kCurrentVersionNumber = 4;
const int kCompatibleVersionNumber = 4;
const int kDeprecatedVersionNumber = 1;

sql::InitStatus LogMigrationFailure(int from_version) {
  LOG(ERROR) << "Contacts DB failed to migrate from version " << from_version
             << ". Contacts API will be disabled.";
  return sql::INIT_FAILURE;
}

}  // namespace

ContactDatabase::ContactDatabase()
    : db_({// Note that we don't set exclusive locking here. That's done by
           // BeginExclusiveMode below which is called later (we have to be in
           // shared mode to start out for the in-memory backend to read the
           // data).
           // TODO(1153459) Remove this dependency on normal locking mode.
           .exclusive_locking = false,
           // Set the database page size to something a little larger to give us
           // better performance (we're typically seek rather than bandwidth
           // limited). Must be a power of 2 and a max of 65536.
           .page_size = 4096,
           // Set the cache size. The page size, plus a little extra, times this
           // value, tells us how much memory the cache will use maximum.
           // 1000 * 4kB = 4MB
           .cache_size = 1000}) {}

ContactDatabase::~ContactDatabase() {}

sql::InitStatus ContactDatabase::Init(const base::FilePath& contact_db_name) {
  db_.set_histogram_tag("Contact");

  // Note that we don't set exclusive locking here. That's done by
  // BeginExclusiveMode below which is called later (we have to be in shared
  // mode to start out for the in-memory backend to read the data).

  if (!db_.Open(contact_db_name))
    return sql::INIT_FAILURE;

  // Clear the database if too old for upgrade.
  DCHECK_LT(kDeprecatedVersionNumber, GetCurrentVersion());
  if (sql::MetaTable::RazeIfIncompatible(&db_, kDeprecatedVersionNumber,
                                         GetCurrentVersion()) ==
          sql::RazeIfIncompatibleResult::kFailed)
    return sql::INIT_FAILURE;

  // Wrap the rest of init in a tranaction. This will prevent the database from
  // getting corrupted if we crash in the middle of initialization or migration.
  sql::Transaction committer(&db_);
  if (!committer.Begin())
    return sql::INIT_FAILURE;

#if BUILDFLAG(IS_APPLE)
  // Exclude the contact file from backups.
  base::apple::SetBackupExclusion(contact_db_name);
#endif

  // Prime the cache.
  db_.Preload();

  // Create the tables and indices.
  // NOTE: If you add something here, also add it to
  //       RecreateAllButStarAndURLTables.
  if (!meta_table_.Init(&db_, GetCurrentVersion(), kCompatibleVersionNumber))
    return sql::INIT_FAILURE;

  if (!CreateContactTable() || !CreateEmailTable() ||
      !CreatePhonenumberTable() || !CreatePostalAddressTable())
    return sql::INIT_FAILURE;

  // Version check.
  sql::InitStatus version_status = EnsureCurrentVersion();
  if (version_status != sql::INIT_OK) {
    return version_status;
  }

  return committer.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
}

void ContactDatabase::BeginExclusiveMode() {
  // We can't use set_exclusive_locking() since that only has an effect before
  // the DB is opened.
  std::ignore = db_.Execute("PRAGMA locking_mode=EXCLUSIVE");
}

// static
int ContactDatabase::GetCurrentVersion() {
  return kCurrentVersionNumber;
}

void ContactDatabase::BeginTransaction() {
  db_.BeginTransactionDeprecated();
}

void ContactDatabase::CommitTransaction() {
  db_.CommitTransactionDeprecated();
}

// Migration -------------------------------------------------------------------

sql::InitStatus ContactDatabase::EnsureCurrentVersion() {
  // We can't read databases newer than we were designed for.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Contact database is too new.";
    return sql::INIT_TOO_NEW;
  }

  int cur_version = meta_table_.GetVersionNumber();

  if (cur_version == 2) {
    if (!db_.DoesColumnExist("contacts", "trusted"))
      if (!db_.Execute("ALTER TABLE contacts ADD COLUMN trusted INT"))
        return LogMigrationFailure(cur_version);
    ++cur_version;
    std::ignore = meta_table_.SetVersionNumber(cur_version);
    std::ignore = meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
  }

  if (cur_version == 3) {
    if (db_.DoesColumnExist("email_addresses", "trusted")) {
      if (!db_.Execute("UPDATE contacts "
                       " SET trusted = (select max(email_addresses.trusted) "
                       "  FROM email_addresses"
                       " WHERE contacts.id = email_addresses.contact_id)"))
        return LogMigrationFailure(cur_version);

      if (db_.Execute(
              "ALTER TABLE email_addresses RENAME TO email_addresses_old")) {
        CreateEmailTable();
      }
      if (!db_.Execute(
              "INSERT INTO email_addresses "
              " (contact_id, email, type, favorite, obsolete, "
              " created, last_modified)"
              "  SELECT contact_id, email, type, is_default, obsolete, "
              "    created, last_modified FROM email_addresses_old"))
        return LogMigrationFailure(cur_version);

      if (!db_.Execute("DROP TABLE email_addresses_old"))
        return LogMigrationFailure(cur_version);

      ++cur_version;
      std::ignore = meta_table_.SetVersionNumber(cur_version);
      std::ignore = meta_table_.SetCompatibleVersionNumber(
          std::min(cur_version, kCompatibleVersionNumber));
    }
  }

  LOG_IF(WARNING, cur_version < GetCurrentVersion())
      << "Contact database version " << cur_version << " is too old to handle.";

  return sql::INIT_OK;
}

void ContactDatabase::RollbackTransaction() {
  // If Init() returns with a failure status, the Transaction created there will
  // be destructed and rolled back. ContactBackend might try to kill the
  // database after that, at which point it will try to roll back a non-existing
  // transaction. This will crash on a DCHECK. So transaction_nesting() is
  // checked first.
  if (db_.transaction_nesting())
    db_.RollbackTransactionDeprecated();
}

void ContactDatabase::Vacuum() {
  DCHECK_EQ(0, db_.transaction_nesting())
      << "Can not have a transaction when vacuuming.";
  std::ignore = db_.Execute("VACUUM");
}

void ContactDatabase::TrimMemory(bool aggressively) {
  db_.TrimMemory();
}

bool ContactDatabase::Raze() {
  return db_.Raze();
}

sql::Database& ContactDatabase::GetDB() {
  return db_;
}

}  // namespace contact
