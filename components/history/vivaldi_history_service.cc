// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/history/core/browser/history_service.h"

#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "components/history/core/browser/history_backend.h"

namespace history {

base::CancelableTaskTracker::TaskId HistoryService::TopUrlsPerDay(
    size_t num_hosts,
    UrlVisitCount::TopUrlsPerDayCallback callback,
    base::CancelableTaskTracker* tracker) const {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::TopUrlsPerDay, history_backend_.get(),
                     num_hosts),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::VisitSearch(
    const QueryOptions& options,
    Visit::VisitsCallback callback,
    base::CancelableTaskTracker* tracker) const {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::VisitSearch, history_backend_.get(),
                     options),
      std::move(callback));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
base::CancelableTaskTracker::TaskId HistoryService::GetVivaldiTypedHistory(
    const std::string query,
    int max_results,
    TypedHistoryCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryTypedHistory, history_backend_.get(),
                     query, max_results),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId HistoryService::GetVivaldiDetailedHistory(
    const std::string query,
    int max_results,
    DetailUrlResultsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetVivaldiDetailedHistory,
                     history_backend_.get(), query, max_results),
      std::move(callback));
}

#endif

}  // namespace history
