// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/history/core/browser/history_service.h"

#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "components/history/core/browser/history_backend.h"

namespace history {

void HistoryService::TopUrlsPerDay(
    size_t num_hosts,
    const UrlVisitCount::TopUrlsPerDayCallback& callback) const {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  PostTaskAndReplyWithResult(thread_->task_runner().get(), FROM_HERE,
                             base::Bind(&HistoryBackend::TopUrlsPerDay,
                                        history_backend_.get(), num_hosts),
                             callback);
}

void HistoryService::VisitSearch(const QueryOptions& options,
                                 const Visit::VisitsCallback& callback) const {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  PostTaskAndReplyWithResult(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::VisitSearch, history_backend_.get(), options),
      callback);
}

base::CancelableTaskTracker::TaskId HistoryService::QueryHistoryWStatement(
    const char* sql_statement,
    const std::string& search_string,
    int max_hits,
    QueryHistoryCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  return tracker->PostTaskAndReplyWithResult(
      thread_->task_runner().get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryHistoryWStatement,
                     history_backend_.get(), sql_statement, search_string,
                     max_hits),
      std::move(callback));
}

}  // namespace history
