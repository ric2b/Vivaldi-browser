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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
DetailedHistory::DetailedHistoryList
HistoryBackend::QueryDetailedHistoryWStatement(
    const char* sql_query,
    const std::string& search_string,
    int max_hits) {
  DetailedHistory::DetailedHistoryList query_results;
  if (db_) {
    db_->GetDetailedMatchesWStatement(
      sql_query, search_string, max_hits, &query_results);
  }
  return query_results;
}
#endif

}  // namespace history
