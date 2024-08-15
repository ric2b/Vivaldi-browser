// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mail_client_service.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/i18n/string_compare.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/variations/variations_associated_data.h"
#include "mail_client_backend.h"
#include "mail_client_model_observer.h"

using base::Time;

using mail_client::MailClientBackend;

namespace mail_client {

// Sends messages from the db backend to us on the main thread. This must be a
// separate class from the mail_client service so that it can hold a reference
// to the history service (otherwise we would have to manually AddRef and
// Release when the Backend has a reference to us).
class MailClientService::MailClientBackendDelegate
    : public MailClientBackend::MailClientDelegate {
 public:
  MailClientBackendDelegate(
      const base::WeakPtr<MailClientService>& mail_client_service,
      const scoped_refptr<base::SequencedTaskRunner>& service_task_runner)
      : mail_client_service_(mail_client_service),
        service_task_runner_(service_task_runner) {}

  void NotifyMigrationProgress(int progress,
                               int total,
                               std::string msg) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MailClientService::OnMigrationChanges,
                                  mail_client_service_, progress, total, msg));
  }

  void NotifyDeleteMessages(int total) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MailClientService::OnDeleteMessageChange,
                                  mail_client_service_, total));
  }

  void DBLoaded() override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MailClientService::OnDBLoaded, mail_client_service_));
  }

 private:
  const base::WeakPtr<MailClientService> mail_client_service_;
  const scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
};

MailClientService::MailClientService()
    : backend_loaded_(false), weak_ptr_factory_(this) {}

MailClientService::~MailClientService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Shutdown the backend. This does nothing if Cleanup was already invoked.
  Cleanup();
}

void MailClientService::Shutdown() {
  Cleanup();
}

bool MailClientService::Init(
    bool no_db,
    const MailClientDatabaseParams& mail_client_database_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!backend_task_runner_);

  backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  // Create the MailClient backend.
  scoped_refptr<MailClientBackend> backend(new MailClientBackend(
      new MailClientBackendDelegate(
          weak_ptr_factory_.GetWeakPtr(),
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      backend_task_runner_));
  mail_client_backend_.swap(backend);

  ScheduleTask(base::BindOnce(&MailClientBackend::Init, mail_client_backend_,
                              no_db, mail_client_database_params));

  return true;
}

void MailClientService::ScheduleTask(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_task_runner_);

  backend_task_runner_->PostTask(FROM_HERE, std::move(task));
}

void MailClientService::AddObserver(MailClientModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void MailClientService::RemoveObserver(MailClientModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void MailClientService::OnDBLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_loaded_ = true;
  NotifyMailClientServiceLoaded();
}

void MailClientService::NotifyMailClientServiceLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (MailClientModelObserver& observer : observers_)
    observer.OnMailClientServiceLoaded(this);
}

void MailClientService::NotifyMailClientServiceBeingDeleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (MailClientModelObserver& observer : observers_)
    observer.OnMailClientModelBeingDeleted(this);
}

void MailClientService::OnMigrationChanges(int progress,
                                           int total,
                                           std::string msg) {
  for (MailClientModelObserver& observer : observers_) {
    observer.OnMigrationProgress(this, progress, total, msg);
  }
}

void MailClientService::OnDeleteMessageChange(int total) {
  for (MailClientModelObserver& observer : observers_) {
    observer.OnDeleteMessagesProgress(this, total);
  }
}

void MailClientService::Cleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_task_runner_) {
    // We've already cleaned up.
    return;
  }

  NotifyMailClientServiceBeingDeleted();

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Unload the backend.
  if (mail_client_backend_.get()) {
    ScheduleTask(base::BindOnce(&MailClientBackend::Closing,
                                std::move(mail_client_backend_)));
  }

  // Clear |backend_task_runner_| to make sure it's not used after Cleanup().
  backend_task_runner_ = nullptr;
}

base::CancelableTaskTracker::TaskId MailClientService::CreateMessages(
    mail_client::MessageRows rows,
    ResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MailClientBackend::CreateMessages, mail_client_backend_,
                     rows),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId MailClientService::DeleteMessages(
    mail_client::SearchListIDs ids,
    ResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MailClientBackend::DeleteMessages, mail_client_backend_,
                     ids),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId MailClientService::UpdateMessage(
    MessageRow message,
    MessageCallback callback,
    base::CancelableTaskTracker* tracker) {
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MailClientBackend::UpdateMessage, mail_client_backend_,
                     message),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId MailClientService::SearchEmail(
    std::u16string search,
    EmailSearchCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_)
      << "MailClient service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MailClientBackend::EmailSearch, mail_client_backend_,
                     search),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId MailClientService::MatchMessage(
    SearchListID search_list_id,
    std::u16string search,
    ResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_)
      << "MailClient service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MailClientBackend::MatchMessage, mail_client_backend_,
                     search_list_id, search),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId MailClientService::GetDBVersion(
    VersionCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_)
      << "MailClient service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MailClientBackend::GetDBVersion, mail_client_backend_),
      base::BindOnce(std::move(callback)));
}

base::CancelableTaskTracker::TaskId MailClientService::MigrateSearchDB(
    ResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_)
      << "MailClient service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MailClientBackend::MigrateSearchDB, mail_client_backend_),
      base::BindOnce(std::move(callback)));
}

base::CancelableTaskTracker::TaskId MailClientService::DeleteMailSearchDB(
    ResultCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_)
      << "MailClient service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MailClientBackend::DeleteMailSearchDB,
                     mail_client_backend_),
      base::BindOnce(std::move(callback)));
}

}  // namespace mail_client
