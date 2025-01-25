// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_SERVICE_H_
#define COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

#include "mail_client_backend.h"
#include "mail_client_database_params.h"
#include "mail_client_model_observer.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace mail_client {

struct MailClientDatabaseParams;
class MailClientBackend;

class MailClientService : public KeyedService {
 public:
  MailClientService();
  ~MailClientService() override;
  MailClientService(const MailClientService&) = delete;
  MailClientService& operator=(const MailClientService&) = delete;

  bool Init(bool no_db,
            const MailClientDatabaseParams& mail_client_database_params);

  // Called from shutdown service before shutting down the browser
  void Shutdown() override;

  void AddObserver(MailClientModelObserver* observer);
  void RemoveObserver(MailClientModelObserver* observer);

  // Call to schedule a given task for running on the MailClient thread with the
  // specified priority. The task will have ownership taken.
  void ScheduleTask(base::OnceClosure task);

  // Returns true if this MailClient service is currently in a mode where
  // extensive changes might happen, such as for import and sync. This is
  // helpful for observers that are created after the service has started, and
  // want to check state during their own initializer.
  bool IsDoingExtensiveChanges() const { return extensive_changes_ > 0; }

  typedef base::OnceCallback<void(MessageResult)> MessageCallback;

  typedef base::OnceCallback<void(SearchListIDs)> EmailSearchCallback;

  typedef base::OnceCallback<void(bool)> ResultCallback;
  typedef base::OnceCallback<void(Migration)> VersionCallback;

  base::CancelableTaskTracker::TaskId CreateMessages(
      mail_client::MessageRows rows,
      ResultCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteMessages(
      mail_client::SearchListIDs ids,
      ResultCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateMessage(
      MessageRow message,
      MessageCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId SearchEmail(
      std::u16string search,
      EmailSearchCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId MatchMessage(
      SearchListID search_list_id,
      std::u16string search,
      ResultCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetDBVersion(
      VersionCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId MigrateSearchDB(
      ResultCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteMailSearchDB(
      ResultCallback callback,
      base::CancelableTaskTracker* tracker);

 private:
  class MailClientBackendDelegate;
  friend class base::RefCountedThreadSafe<MailClientService>;
  friend class MailClientBackendDelegate;
  friend class MailClientBackend;

  void OnDBLoaded();

  // Notify all MailClientServiceObservers registered that the
  // MailClientService has finished loading.
  void NotifyMailClientServiceLoaded();
  void NotifyMailClientServiceBeingDeleted();
  void OnMigrationChanges(int progress, int total, std::string msg);
  void OnDeleteMessageChange(int delete_progress_count);

  void Cleanup();

  SEQUENCE_CHECKER(sequence_checker_);

  // The TaskRunner to which MailClientBackend tasks are posted. Nullptr once
  // Cleanup() is called.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // This pointer will be null once Cleanup() has been called, meaning no
  // more calls should be made to the MailClient thread.
  scoped_refptr<MailClientBackend> mail_client_backend_;

  // Has the backend finished loading? The backend is loaded once Init has
  // completed.
  bool backend_loaded_;

  // The observers.
  base::ObserverList<MailClientModelObserver> observers_;

  // See description of IsDoingExtensiveChanges above.
  int extensive_changes_;

  // All vended weak pointers are invalidated in Cleanup().
  base::WeakPtrFactory<MailClientService> weak_ptr_factory_;
};

}  // namespace mail_client

#endif  // COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_SERVICE_H_
