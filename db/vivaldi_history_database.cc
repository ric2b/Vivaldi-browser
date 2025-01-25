// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved
#include "db/vivaldi_history_database.h"
#include "db/vivaldi_history_types.h"

#include <limits>
#include <string>

#include "sql/statement.h"

namespace history {

VivaldiHistoryDatabase::VivaldiHistoryDatabase() {}

VivaldiHistoryDatabase::~VivaldiHistoryDatabase() {}

UrlVisitCount::TopUrlsPerDayList VivaldiHistoryDatabase::TopUrlsPerDay(
    size_t num_hosts) {
  sql::Statement url_sql(GetDB().GetUniqueStatement(
      "SELECT date, url, visit_count, id FROM "
      "  ( SELECT v.id, u.url, count(*) AS visit_count, "
      "    strftime('%Y-%m-%d', datetime(v.visit_time / 1000000 + "
      "      (strftime('%s', '1601-01-01')), 'unixepoch')) AS date "
      "  FROM visits v "
      "    JOIN urls u ON (v.url = u.id) "
      "  GROUP BY date, u.url "
      "  ORDER BY date DESC, visit_count DESC) g "
      "  WHERE ( "
      "    SELECT count(*) "
      "      FROM (SELECT v.id, u.url, COUNT(*) AS visit_count, "
      "        strftime('%Y-%m-%d', datetime(v.visit_time / 1000000 + "
      "        (strftime('%s', '1601-01-01')), 'unixepoch')) AS date "
      "          FROM visits v "
      "            JOIN urls u ON (v.url = u.id) "
      "              GROUP BY date, u.url) AS f "
      "   WHERE g.id <= f.id AND f.date = g.date ) <= ? "
      "    ORDER BY date DESC, visit_count DESC "));
  url_sql.BindInt64(0, num_hosts);
  UrlVisitCount::TopUrlsPerDayList hosts;
  while (url_sql.Step()) {
    std::string date = url_sql.ColumnString(0);
    GURL url(url_sql.ColumnString(1));
    int64_t visit_count = url_sql.ColumnInt64(2);
    hosts.push_back(UrlVisitCount(date, url, visit_count));
  }
  return hosts;
}

Visit::VisitsList VivaldiHistoryDatabase::VisitSearch(
    const QueryOptions& options) {
  base::Time begin_time = options.begin_time;
  base::Time end_time = options.end_time;

  const std::string query =
      "SELECT "
      "  v.id, "
      "  v.visit_time, "
      "  u.url, "
      "  u.title, "
      "  v.transition "
      " FROM urls u "
      "    JOIN visits v on (u.id = v.url) "
      " WHERE v.visit_time >= ? "
      "  AND v.visit_time < ? "
      " ORDER BY v.visit_time DESC"
      ;

  sql::Statement url_sql(GetDB().GetUniqueStatement(query));
  int64_t begin = begin_time.ToInternalValue();
  int64_t end = end_time.ToInternalValue();
  url_sql.BindInt64(0, begin ? begin : std::numeric_limits<int64_t>::min());
  url_sql.BindInt64(1, end ? end : std::numeric_limits<int64_t>::max());

  Visit::VisitsList hosts;
  while (url_sql.Step()) {
    std::string id = url_sql.ColumnString(0);
    base::Time visit_time =
        base::Time::FromInternalValue(url_sql.ColumnInt64(1));
    GURL url(url_sql.ColumnString(2));
    std::u16string title = url_sql.ColumnString16(3);
    ui::PageTransition transitionType =
        ui::PageTransitionFromInt(url_sql.ColumnInt(4));

    bool has_chain_start = ui::PAGE_TRANSITION_CHAIN_START &
                        ui::PageTransitionGetQualifier(transitionType);
    bool has_chain_end = ui::PAGE_TRANSITION_CHAIN_END &
                        ui::PageTransitionGetQualifier(transitionType);
    bool is_redirect = ui::PageTransitionIsRedirect(transitionType) &&
                        !(has_chain_start || has_chain_end);
    if (!is_redirect && url.is_valid()) {
      hosts.push_back(
        Visit(id, visit_time, url, title, transitionType));
    }
  }

  Visit::VisitsList filteredHosts;
  // VB-98940: Don't copy http items if their https equivalent is present in history.
  std::copy_if(hosts.begin(), hosts.end(), std::back_inserter(filteredHosts),
    [&](Visit curr) {
      std::string curr_url_no_scheme = curr.url.GetContent();
      if (curr.url.SchemeIs("http")) {
        for (size_t i = 0; i < hosts.size(); i++) {
          if (hosts[i].url.GetContent() == curr_url_no_scheme &&
            curr.url.scheme() != hosts[i].url.scheme()) {
              return false;
          }
        }
      }
      return true;
  });
  return filteredHosts;
}

}  // namespace history
