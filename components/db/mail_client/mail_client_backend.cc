// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mail_client_backend.h"

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "mail_client_database_params.h"
#include "sql/error_delegate_util.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace mail_client {

const base::FilePath::CharType kMailClientFilename[] =
    FILE_PATH_LITERAL("MailSearchDB");

MailClientBackend::MailClientBackend(
    MailClientDelegate* delegate,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : delegate_(delegate) {}

MailClientBackend::MailClientBackend(MailClientDelegate* delegate) {}

MailClientBackend::~MailClientBackend() {}

void MailClientBackend::NotifyMigrationProgress(int progress,
                                                int total,
                                                std::string msg) {
  if (delegate_)
    delegate_->NotifyMigrationProgress(progress, total, msg);
}

void MailClientBackend::NotifyDeleteMessages(int total) {
  if (delegate_)
    delegate_->NotifyDeleteMessages(total);
}

void MailClientBackend::Init(
    bool force_fail,
    const MailClientDatabaseParams& mail_client_database_params) {
  // MailClientBackend is created on the UI thread by MailClientService, then
  // the MailClientBackend::Init() method is called on the DB thread. Create the
  // base::SupportsUserData on the DB thread since it is not thread-safe.

  if (!force_fail)
    InitImpl(mail_client_database_params);
  delegate_->DBLoaded();
}

void MailClientBackend::Closing() {
  CancelScheduledCommit();

  // Release our reference to the delegate, this reference will be keeping the
  // MailClient service alive.
  delegate_.reset();
}

void MailClientBackend::InitImpl(
    const MailClientDatabaseParams& mail_client_database_params) {
  DCHECK(!db_) << "Initializing MailClientBackend twice";
  // In the rare case where the db fails to initialize a dialog may get shown
  // the blocks the caller, yet allows other messages through. For this reason
  // we only set db_ to the created database if creation is successful. That
  // way other methods won't do anything as db_ is still null.

  // Compute the file names.
  mail_client_database_dir_ =
      mail_client_database_params.mail_client_databse_dir;
  base::FilePath mail_client_db_name =
      mail_client_database_dir_.Append(kMailClientFilename);

  // MailClient database.
  db_.reset(new MailClientDatabase());

  sql::InitStatus status = db_->Init(mail_client_db_name);
  switch (status) {
    case sql::INIT_OK:
      break;
    case sql::INIT_FAILURE: {
      // A null db_ will cause all calls on this object to notice this error
      // and to not continue. If the error callback scheduled killing the
      // database, the task it posted has not executed yet. Try killing the
      // database now before we close it.
      [[fallthrough]];
    }  // Falls through.
    case sql::INIT_TOO_NEW: {
      LOG(ERROR) << "INIT_TOO_NEW";

      return;
    }
    default:
      NOTREACHED();
  }
}

void MailClientBackend::CreateFTSMessage() {}

void MailClientBackend::CloseAllDatabases() {
  if (db_) {
    // Commit the long-running transaction.
    db_->CommitTransaction();
    db_.reset();
  }
}

void MailClientBackend::CancelScheduledCommit() {
  scheduled_commit_.Cancel();
}

void MailClientBackend::Commit() {
  if (!db_)
    return;

#if BUILDFLAG(IS_IOS)
  // Attempts to get the application running long enough to commit the database
  // transaction if it is currently being backgrounded.
  base::ios::ScopedCriticalAction scoped_critical_action;
#endif

  // Note that a commit may not actually have been scheduled if a caller
  // explicitly calls this instead of using ScheduleCommit. Likewise, we
  // may reset the flag written by a pending commit. But this is OK! It
  // will merely cause extra commits (which is kind of the idea). We
  // could optimize more for this case (we may get two extra commits in
  // some cases) but it hasn't been important yet.
  CancelScheduledCommit();

  db_->CommitTransaction();
  DCHECK_EQ(db_->transaction_nesting(), 0)
      << "Somebody left a transaction open";
  db_->BeginTransaction();
}

bool MailClientBackend::CreateMessages(
    std::vector<mail_client::MessageRow> messages) {
  return db_->CreateMessages(messages);
}

bool MailClientBackend::DeleteMessages(SearchListIDs ids) {
  bool success = db_->DeleteMessages(ids);
  if (success) {
    NotifyDeleteMessages(ids.size());
  }

  return success;
}

MessageResult MailClientBackend::UpdateMessage(
    mail_client::MessageRow message) {
  MessageResult result;
  if (!db_) {
    result.success = false;
    result.message = "Database error";
    return result;
  }

  result.success = db_->UpdateMessage(message);
  if (!result.success) {
    result.message = "Error adding message body";
  }
  return result;
}

SearchListIDs MailClientBackend::EmailSearch(std::u16string searchValue) {
  SearchListIDs rows;
  db_->SearchMessages(searchValue, &rows);

  return rows;
}

bool MailClientBackend::MatchMessage(SearchListID search_list_id,
                                     std::u16string searchValue) {
  return db_->MatchMessage(search_list_id, searchValue);
}

Migration MailClientBackend::GetDBVersion() {
  Migration res;
  res.db_version = MailClientDatabase::GetCurrentVersion();
  bool mail_db_file_exists = base::PathExists(
      mail_client_database_dir_.Append(FILE_PATH_LITERAL("MailDB")));
  res.migration_needed = mail_db_file_exists;
  return res;
}

void MailClientBackend::DeleteMailDB() {
  base::DeleteFile(
      mail_client_database_dir_.Append(FILE_PATH_LITERAL("MailDB")));
  base::DeleteFile(
      mail_client_database_dir_.Append(FILE_PATH_LITERAL("MailDB-journal")));
}

bool MailClientBackend::DeleteMailSearchDB() {
  db_->Close();

  bool delete_result = base::DeleteFile(mail_client_database_dir_.Append(
                           FILE_PATH_LITERAL("MailSearchDB"))) &&
                       base::DeleteFile(mail_client_database_dir_.Append(
                           FILE_PATH_LITERAL("MailSearchDB-journal")));

  if (delete_result) {
    sql::InitStatus status = db_->Init(
        mail_client_database_dir_.Append(FILE_PATH_LITERAL("MailSearchDB")));
    if (sql::INIT_OK == status) {
      return true;
    }
  }
  return false;
}

bool MailClientBackend::MigrateSearchDB() {
  bool mail_db_exists = base::PathExists(
      mail_client_database_dir_.Append(FILE_PATH_LITERAL("MailDB")));
  if (!mail_db_exists) {
    return true;
  }
  db_->AttachDBForMigrate(mail_client_database_dir_);

  if (!db_->DoesAttachedMessageTableExists()) {
    DeleteMailDB();
    return false;
  }

  NotifyMigrationProgress(0, 0, "Migration starting...");

  const int messagesTotalCount = db_->CountRows("old.messages");

  if (messagesTotalCount == -1) {
    return false;
  }

  NotifyMigrationProgress(0, 0, "Creating table...");
  bool migration_table = db_->CreateMigrationTable();
  if (!migration_table) {
    return false;
  }

  const int BATCH_SIZE = 5000;
  int offset = db_->SelectMaxOffsetFromMigration();

  while (messagesTotalCount >= offset) {
    NotifyMigrationProgress(std::min(offset, messagesTotalCount),
                            messagesTotalCount, "Migrating search database...");
    db_->CopyMessagesToContentless(BATCH_SIZE, offset);
    offset = offset + BATCH_SIZE;
  }

  NotifyMigrationProgress(0, 0, "Starting cleanup...");
  bool detached = db_->DetachDBAfterMigrate();

  if (detached) {
    DeleteMailDB();
  }
  NotifyMigrationProgress(0, 0, "Migration finished");
  return true;
}

}  // namespace mail_client
