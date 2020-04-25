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

QueryResults HistoryBackend::QueryHistoryWStatement(
    const char* sql_query,
    const std::string& search_string,
    int max_hits) {
  QueryResults query_results;
  if (db_) {
    URLRows text_matches;
    db_->GetMatchesWStatement(sql_query, search_string, max_hits,
                              &text_matches);

    std::vector<URLResult> matching_results;
    // todo: Check if this can be further optimized.
    for (size_t i = 0; i < text_matches.size(); i++) {
      const URLRow& text_match = text_matches[i];
      // Get all visits for given URL match.
      URLResult url_result(text_match);
      matching_results.push_back(std::move(url_result));
    }
    query_results.SetURLResults(std::move(matching_results));
  }
  return query_results;
}

}  // namespace history
