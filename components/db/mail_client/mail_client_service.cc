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

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/i18n/string_compare.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/variations/variations_associated_data.h"
#include "mail_client_backend.h"
#include "mail_client_model_observer.h"

using base::Time;

using mail_client::MailClientBackend;

namespace mail_client {
namespace {

static const char* kMailClientThreadName = "Vivaldi_MailClientThread";
}
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

  void DBLoaded() override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MailClientService::OnDBLoaded, mail_client_service_));
  }

 private:
  const base::WeakPtr<MailClientService> mail_client_service_;
  const scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
};

MailClientService::MailClientService()
    : thread_(variations::GetVariationParamValue("BrowserScheduler",
                                                 "RedirectMailClientService") ==
                      "true"
                  ? nullptr
                  : new base::Thread(kMailClientThreadName)),
      backend_loaded_(false),
      weak_ptr_factory_(this) {}

MailClientService::~MailClientService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Shutdown the backend. This does nothing if Cleanup was already invoked.
}

void MailClientService::Shutdown() {}

bool MailClientService::Init(
    bool no_db,
    const MailClientDatabaseParams& mail_client_database_params) {
  TRACE_EVENT0("browser,startup", "MailClientService::Init")
  SCOPED_UMA_HISTOGRAM_TIMER("MailClient.MailClientServiceInitTime");
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!backend_task_runner_);

  if (thread_) {
    base::Thread::Options options;
    options.timer_slack = base::TIMER_SLACK_MAXIMUM;
    if (!thread_->StartWithOptions(options)) {
      Cleanup();
      return false;
    }
    backend_task_runner_ = thread_->task_runner();
  } else {
    backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        base::TaskTraits(base::TaskPriority::USER_BLOCKING,
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
                         base::MayBlock(), base::WithBaseSyncPrimitives()));
  }

  // Create the MailClient backend.
  scoped_refptr<MailClientBackend> backend(new MailClientBackend(
      new MailClientBackendDelegate(weak_ptr_factory_.GetWeakPtr(),
                                    base::ThreadTaskRunnerHandle::Get()),
      backend_task_runner_));
  mail_client_backend_.swap(backend);

  ScheduleTask(base::Bind(&MailClientBackend::Init, mail_client_backend_, no_db,
                          mail_client_database_params));

  return true;
}

void MailClientService::ScheduleTask(base::OnceClosure task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(backend_task_runner_);

  backend_task_runner_->PostTask(FROM_HERE, std::move(task));
}

void MailClientService::AddObserver(MailClientModelObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void MailClientService::RemoveObserver(MailClientModelObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void MailClientService::OnDBLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  backend_loaded_ = true;
  NotifyMailClientServiceLoaded();
}

void MailClientService::NotifyMailClientServiceLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (MailClientModelObserver& observer : observers_)
    observer.OnMailClientServiceLoaded(this);
}

void MailClientService::NotifyMailClientServiceBeingDeleted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (MailClientModelObserver& observer : observers_)
    observer.OnMailClientModelBeingDeleted(this);
}

void MailClientService::Cleanup() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!backend_task_runner_) {
    // We've already cleaned up.
    return;
  }

  NotifyMailClientServiceBeingDeleted();

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Unload the backend.
  if (mail_client_backend_.get()) {
    // The backend's destructor must run on the mail_client thread since it is
    // not threadsafe. So this thread must not be the last thread holding a
    // reference to the backend, or a crash could happen.
    //
    // We have a reference to the MailClient backend. There is also an extra
    // reference held by our delegate installed in the backend, which
    // MailClientBackend::Closing will release. This means if we scheduled a
    // call to MailClientBackend::Closing and *then* released our backend
    // reference, there will be a race between us and the backend's Closing
    // function to see who is the last holder of a reference. If the backend
    // thread's Closing manages to run before we release our backend refptr, the
    // last reference will be held by this thread and the destructor will be
    // called from here.
    //
    // Therefore, we create a closure to run the Closing operation first. This
    // holds a reference to the backend. Then we release our reference, then we
    // schedule the task to run. After the task runs, it will delete its
    // reference from the MailClient thread, ensuring everything works properly.
    //
    mail_client_backend_->AddRef();
    base::Closure closing_task =
        base::Bind(&MailClientBackend::Closing, mail_client_backend_);
    ScheduleTask(closing_task);
    closing_task.Reset();
    backend_task_runner_->ReleaseSoon(FROM_HERE,
                                      std::move(mail_client_backend_));
  }

  // Clear |backend_task_runner_| to make sure it's not used after Cleanup().
  backend_task_runner_ = nullptr;

  // Join the background thread, if any.
  thread_.reset();
}

base::CancelableTaskTracker::TaskId MailClientService::CreateMessages(
    mail_client::MessageRows rows,
    const ResultCallback& callback,
    base::CancelableTaskTracker* tracker) {
  std::shared_ptr<bool> email_rows = std::shared_ptr<bool>(new bool());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&MailClientBackend::CreateMessages, mail_client_backend_, rows,
                 email_rows),
      base::Bind(callback, email_rows));
}

base::CancelableTaskTracker::TaskId MailClientService::DeleteMessages(
    std::vector<SearchListID> search_list_ids,
    const ResultCallback& callback,
    base::CancelableTaskTracker* tracker) {
  std::shared_ptr<bool> delete_messages_result =
      std::shared_ptr<bool>(new bool());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&MailClientBackend::DeleteMessages, mail_client_backend_,
                 search_list_ids, delete_messages_result),
      base::Bind(callback, delete_messages_result));
}

base::CancelableTaskTracker::TaskId MailClientService::AddMessageBody(
    SearchListID search_list_id,
    std::u16string body,
    const MessageCallback& callback,
    base::CancelableTaskTracker* tracker) {
  std::shared_ptr<MessageResult> add_message_body_result =
      std::shared_ptr<MessageResult>(new MessageResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&MailClientBackend::AddMessageBody, mail_client_backend_,
                 search_list_id, body, add_message_body_result),
      base::Bind(callback, add_message_body_result));
}

base::CancelableTaskTracker::TaskId MailClientService::SearchEmail(
    std::u16string search,
    const EmailSearchCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_)
      << "MailClient service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<SearchListIdRows> email_rows =
      std::shared_ptr<SearchListIdRows>(new SearchListIdRows());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&MailClientBackend::EmailSearch, mail_client_backend_, search,
                 email_rows),
      base::Bind(callback, email_rows));
}

base::CancelableTaskTracker::TaskId MailClientService::MatchMessage(
    SearchListID search_list_id,
    std::u16string search,
    const ResultCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_)
      << "MailClient service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<bool> email_rows = std::shared_ptr<bool>(new bool());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&MailClientBackend::MatchMessage, mail_client_backend_,
                 search_list_id, search, email_rows),
      base::Bind(callback, email_rows));
}

}  // namespace mail_client
