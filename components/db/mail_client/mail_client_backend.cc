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
    FILE_PATH_LITERAL("MailDB");

MailClientBackend::MailClientBackend(
    MailClientDelegate* delegate,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : delegate_(delegate) {}

MailClientBackend::MailClientBackend(MailClientDelegate* delegate) {}

MailClientBackend::~MailClientBackend() {}

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

void MailClientBackend::CreateMessages(
    std::vector<mail_client::MessageRow> messages,
    std::shared_ptr<bool> result) {
  bool res = db_->CreateMessages(messages);
  *result = res;
}

void MailClientBackend::DeleteMessages(
    std::vector<SearchListID> search_list_ids,
    std::shared_ptr<bool> result) {
  *result = db_->DeleteMessages(search_list_ids);
}

void MailClientBackend::AddMessageBody(SearchListID search_list_id,
                                       std::u16string body,
                                       std::shared_ptr<MessageResult> result) {
  if (!db_) {
    result->success = false;
    result->message = "Database error";
    return;
  }

  result->success = db_->AddMessageBody(search_list_id, body);
  if (!result->success) {
    result->message = "Error adding message body";
  }
}

void MailClientBackend::EmailSearch(std::u16string searchValue,
                                    std::shared_ptr<SearchListIdRows> results) {
  SearchListIdRows rows;
  db_->SearchMessages(searchValue, &rows);

  for (SearchListIdRows::iterator it = rows.begin(); it != rows.end(); ++it) {
    results->push_back(*it);
  }
}

void MailClientBackend::MatchMessage(SearchListID search_list_id,
                                     std::u16string searchValue,
                                     std::shared_ptr<bool> results) {
  bool found = db_->MatchMessage(search_list_id, searchValue);
  *results = found;
}

void MailClientBackend::RebuildAndVacuumDatabase(std::shared_ptr<bool> result) {
  bool rebuilt = db_->RebuildDatabase();
  db_->Vacuum();
  *result = rebuilt;
}

}  // namespace mail_client
