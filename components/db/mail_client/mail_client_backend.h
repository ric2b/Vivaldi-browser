// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_BACKEND_H_
#define COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_BACKEND_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "mail_client_backend_notifier.h"
#include "mail_client_database.h"
#include "sql/init_status.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace mail_client {

class MailClientDatabase;
struct MailClientDatabaseParams;

// Internal MailClient implementation which does most of the work of the
// MailClient system. This runs on a custom created db thread (to not block the
// browser when we do expensive operations) and is NOT threadsafe, so it must
// only be called from message handlers on the background thread.
//
// Most functions here are just the implementations of the corresponding
// functions in the MailClient service. These functions are not documented
// here, see the MailClient service for behavior.
class MailClientBackend : public base::RefCountedThreadSafe<MailClientBackend>,
                          public MailClientBackendNotifier {
 public:
  // Interface implemented by the owner of the MailClient backend object.
  // Normally, the MailClient service implements this to send stuff back to the
  // main thread. The unit tests can provide a different implementation if they
  // don't have a MailClient service object.
  class MailClientDelegate {
   public:
    virtual ~MailClientDelegate() {}

    virtual void NotifyMigrationProgress(int progress,
                                         int total,
                                         std::string msg) = 0;

    // Reports status on Deleting messages from search db
    virtual void NotifyDeleteMessages(int delete_progress_count) = 0;

    // Invoked when the backend has finished loading the db.
    virtual void DBLoaded() = 0;
  };

  explicit MailClientBackend(MailClientDelegate* delegate);
  MailClientBackend(const MailClientBackend&) = delete;
  MailClientBackend& operator=(const MailClientBackend&) = delete;

  // This constructor is fast and does no I/O, so can be called at any time.
  MailClientBackend(MailClientDelegate* delegate,
                    scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Must be called after creation but before any objects are created. If this
  // fails, all other functions will fail as well. (Since this runs on another
  // thread, we don't bother returning failure.)
  //
  // |force_fail| can be set during unittests to unconditionally fail to init.
  void Init(bool force_fail,
            const MailClientDatabaseParams& mail_client_database_params);

  // Notification that the MailClient system is shutting down. This will break
  // the refs owned by the delegate and any pending transaction so it will
  // actually be deleted.
  void Closing();

  void CancelScheduledCommit();

  void Commit();

  // Creates an FTS Message
  void CreateFTSMessage();

  bool CreateMessages(std::vector<MessageRow> messages);

  bool DeleteMessages(SearchListIDs ids);
  bool DeleteMailSearchDB();

  MessageResult UpdateMessage(mail_client::MessageRow message);

  SearchListIDs EmailSearch(std::u16string searchValue);

  bool MatchMessage(SearchListID search_list_id, std::u16string searchValue);

  bool MigrateSearchDB();
  Migration GetDBVersion();

  void NotifyMigrationProgress(int progress,
                               int total,
                               std::string msg) override;

  void NotifyDeleteMessages(int delete_progress_count) override;

 protected:
  ~MailClientBackend() override;

 private:
  friend class base::RefCountedThreadSafe<MailClientBackend>;
  // Does the work of Init.
  void InitImpl(const MailClientDatabaseParams& mail_client_database_params);

  // Closes all databases managed by MailClientBackend. Commits any pending
  // transactions.
  void CloseAllDatabases();
  void DeleteMailDB();

  // Querying ------------------------------------------------------------------

  // Directory where database files will be stored, empty until Init is called.
  base::FilePath mail_client_database_dir_;

  // Delegate. See the class definition above for more information. This will
  // be null before Init is called and after Cleanup, but is guaranteed
  // non-null in between.
  std::unique_ptr<MailClientDelegate> delegate_;

  // A commit has been scheduled to occur sometime in the future. We can check
  // !IsCancelled() to see if there is a commit scheduled in the future (note
  // that CancelableClosure starts cancelled with the default constructor), and
  // we can use Cancel() to cancel the scheduled commit. There can be only one
  // scheduled commit at a time (see ScheduleCommit).
  base::CancelableOnceClosure scheduled_commit_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The MailClient database. Either may be null if the database could
  // not be opened, all users must first check for null and return immediately
  // if it is.
  std::unique_ptr<MailClientDatabase> db_;
};

}  // namespace mail_client

#endif  // COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_BACKEND_H_
