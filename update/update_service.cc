// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_service.h"

#include <string>
#include <utility>

#include "base/task/thread_pool.h"
#include "update/update_backend.h"
#include "update/update_model_observer.h"

using update::UpdateBackend;

namespace update {
class UpdateService::UpdateBackendDelegate
    : public UpdateBackend::UpdateDelegate {
 public:
  UpdateBackendDelegate(
      const base::WeakPtr<UpdateService>& update_service,
      const scoped_refptr<base::SequencedTaskRunner>& service_task_runner)
      : update_service_(update_service),
        service_task_runner_(service_task_runner) {}

  void NotifyUpdateProgress(const AutoUpdateStatus& status,
                            const std::string& reason,
                            const int progress) {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UpdateService::OnUpdgradeProgress,
                                  update_service_, status, reason, progress));
  }

 private:
  const base::WeakPtr<UpdateService> update_service_;
  const scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
};

UpdateService::UpdateService() : weak_ptr_factory_(this) {}

UpdateService::~UpdateService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Shutdown the backend. This does nothing if Cleanup was already invoked.
  Cleanup();
}

void UpdateService::Shutdown() {
  Cleanup();
}

bool UpdateService::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!backend_task_runner_);
  backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  // Create the update backend.
  scoped_refptr<UpdateBackend> backend(
      new UpdateBackend(new UpdateBackendDelegate(
                            weak_ptr_factory_.GetWeakPtr(),
                            base::SingleThreadTaskRunner::GetCurrentDefault()),
                        backend_task_runner_));
  update_backend_.swap(backend);
  ScheduleTask(base::BindOnce(&UpdateBackend::Init, update_backend_));

  return true;
}

base::CancelableTaskTracker::TaskId UpdateService::StartUpdate(
    bool should_install_update,
    base::OnceCallback<void()> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Update service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&UpdateBackend::StartUpdate, update_backend_,
                     should_install_update),
      base::BindOnce(std::move(callback)));
}

void UpdateService::ScheduleTask(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_task_runner_);

  backend_task_runner_->PostTask(FROM_HERE, std::move(task));
}

void UpdateService::AddObserver(UpdateModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void UpdateService::RemoveObserver(UpdateModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void UpdateService::Cleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_task_runner_) {
    // We've already cleaned up.
    return;
  }

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Unload the backend.
  if (update_backend_) {
    ScheduleTask(
        base::BindOnce(&UpdateBackend::Closing, std::move(update_backend_)));
  }

  backend_task_runner_ = nullptr;
}

void UpdateService::OnUpdgradeProgress(const AutoUpdateStatus& status,
                                       const std::string& reason,
                                       const int progress) {
  for (UpdateModelObserver& observer : observers_) {
    observer.OnUpdateProgress(this, status, reason, progress);
  }
}

}  // namespace update
