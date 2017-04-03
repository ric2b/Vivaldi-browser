// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved
#include "db/vivaldi_history_database.h"
#include "db/vivaldi_history_types.h"

#include <string>

#include "sql/statement.h"

namespace history {

VivaldiHistoryDatabase::VivaldiHistoryDatabase() {}

VivaldiHistoryDatabase::~VivaldiHistoryDatabase() {}

UrlVisitCount::TopUrlsPerDayList VivaldiHistoryDatabase::TopUrlsPerDay(size_t num_hosts) {
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

Visit::VisitsList VivaldiHistoryDatabase::VisitSearch(const std::string& text_query,
                                               const QueryOptions& options) {
  base::Time begin_time = options.begin_time;
  base::Time end_time = options.end_time;

  sql::Statement url_sql(
      GetDB().GetUniqueStatement("SELECT "
                                 "  v.id as id, "
                                 "  v.visit_time, "
                                 "  u.url, "
                                 "  u.title, "
                                 "  CASE v.transition "
                                 "    WHEN '805306368' THEN 'link' "
                                 "    WHEN '805306376' THEN 'typed' "
                                 "    WHEN '805306369' THEN 'reload' "
                                 "    ELSE 'other' "
                                 "  END as transition "
                                 "  FROM urls u "
                                 "    JOIN visits v on (u.id = v.url) "
                                 " WHERE v.visit_time >= ? "
                                 "  AND v.visit_time < ? "
                                 " AND u.url LIKE ? OR u.title LIKE ? "
                                 "      ORDER BY v.visit_time DESC"));

  const std::string search_text = "%" + text_query + "%";
  int64_t end = end_time.ToInternalValue();
  url_sql.BindInt64(0, begin_time.ToInternalValue());
  url_sql.BindInt64(1, end ? end : std::numeric_limits<int64_t>::max());
  url_sql.BindString(2, search_text);
  url_sql.BindString(3, search_text);

  Visit::VisitsList hosts;
  while (url_sql.Step()) {
    std::string id = url_sql.ColumnString(0);

    base::Time visit_time =
        base::Time::FromInternalValue(url_sql.ColumnInt64(1));

    GURL url(url_sql.ColumnString(2));
    base::string16 title = url_sql.ColumnString16(3);
    std::string transition = url_sql.ColumnString(4);
    hosts.push_back(Visit(id, visit_time, url, title, transition));
  }
  return hosts;
}

}  // namespace history
