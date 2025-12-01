// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"

namespace history {

UrlVisitCount::TopUrlsPerDayList HistoryBackend::TopUrlsPerDay(
    size_t num_hosts) const {
  if (!db_)
    return UrlVisitCount::TopUrlsPerDayList();

  auto top_urls = db_->TopUrlsPerDay(num_hosts);
  return top_urls;
}

Visit::VisitsList HistoryBackend::VisitSearch(
    const QueryOptions& options) const {
  if (!db_)
    return Visit::VisitsList();

  auto visits = db_->VisitSearch(options);
  return visits;
}

void HistoryBackend::DropHistoryTables() {
  db_->DropHistoryTables();
}

}  // namespace history
