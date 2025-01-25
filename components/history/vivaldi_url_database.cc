// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/history/core/browser/url_database.h"

#include <string>

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "db/vivaldi_history_types.h"
#include "base/strings/string_split.h"

namespace history {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
std::string scoring("  visit_count "
                    "+ 200 * (url LIKE '%/') "
                    "- 100 * (LENGTH(url)-LENGTH(REPLACE(url,'/',''))-4) "
);
history::DetailedUrlResults URLDatabase::GetVivaldiDetailedHistory(
    const std::string query,
    int max_results) {
  history::DetailedUrlResults results;

  results.clear();

  std::string sql("SELECT id, url, title, typed_count, ");
  sql.append("visit_count, last_visit_time, ");
  sql.append(scoring);
  sql.append("as score ");
  sql.append("FROM urls ");
  sql.append("WHERE hidden = 0 ");
  // Adds a search clause for every word of the input.
  std::string word;
  int nb_word = 0;
  for (std::istringstream iterator(query); iterator; iterator >> word) {
    sql.append("AND (urls.url LIKE ? OR urls.title LIKE ?) ");
    // Limit at 100. A search query will most likely not exceed that much words.
    if (nb_word++ > 100)
      break;
  };
  sql.append("AND LENGTH(urls.url) < 2048 ");
  sql.append("AND NOT (urls.last_visit_time = 0) ");

  sql.append("AND ( SUBSTR(urls.url,0,5) LIKE 'ftp:' "
             "OR  SUBSTR(urls.url,0,6) LIKE 'file:' "
             "OR  SUBSTR(urls.url,0,6) LIKE 'http:' "
             "OR  SUBSTR(urls.url,0,7) LIKE 'https:') ");
  sql.append(
      "AND EXISTS ("
      "SELECT * FROM visits "
      "WHERE visits.url = urls.id "
      "AND visits.visit_time = urls.last_visit_time ");
  // TRANSITION_IS_REDIRECT_MASK = 0xC00000000
  // PAGE_TRANSITION_CHAIN_START | PAGE_TRANSITON_PAGE_END = 0x30000000
  // The following line excludes the middle of a redirect chain.
  sql.append(
      "AND NOT ((visits.transition & 0xC0000000) "
      "AND NOT (visits.transition & 0x30000000 )) )");
  sql.append("ORDER BY ");
  sql.append(scoring);
  sql.append("DESC limit ?");

  sql::Statement statement(GetDB().GetUniqueStatement(sql));

  int var = 0;
  for (std::istringstream iterator(query); iterator; iterator >> word) {
    std::string wild_wrapped = "%" + std::string(word) +"%";
    // This needs to be repeated, the first binding is for url search
    // the second binding is for title search.
    statement.BindString(var++, wild_wrapped );
    statement.BindString(var++, wild_wrapped );
    if (--nb_word == 0)
      break;
  };
  statement.BindInt(var, max_results);

  while (statement.Step()) {
    DetailedUrlResult result;
    result.id = statement.ColumnString(0);
    result.url = GURL(statement.ColumnString(1));
    result.title = statement.ColumnString(2);
    result.typed_count = statement.ColumnInt(3);
    result.visit_count = statement.ColumnInt(4);
    result.last_visit_time = statement.ColumnTime(5);
    result.score = statement.ColumnInt(6);
    results.push_back(result);
  }

  return results;
}

history::TypedUrlResults URLDatabase::GetVivaldiTypedHistory(
    const std::string query,
    int max_results) {
  history::TypedUrlResults results;
  results.clear();

  std::string sql("SELECT u.url, u.title, u.visit_count, ");
  sql.append("k.url_id IS NOT NULL, k.normalized_term ");
  sql.append("FROM urls AS u ");
  sql.append("LEFT JOIN keyword_search_terms AS k ON u.id = k.url_id ");
  sql.append("WHERE ((u.typed_count > 0 AND u.url LIKE ?) ");
  sql.append("OR k.normalized_term LIKE ? )");
  sql.append("AND LENGTH(u.url) < 2048 ");
  sql.append("ORDER BY u.last_visit_time DESC LIMIT ?");

  std::u16string lower_query(base::i18n::ToLower(base::UTF8ToUTF16(query)));
  sql::Statement statement(GetDB().GetUniqueStatement(sql));
  const std::u16string wild(u"%");
  const std::string wild8("%");
  statement.BindString(0, wild8 + query + wild8);
  statement.BindString16(1, wild + lower_query + wild);
  statement.BindInt(2, max_results);

  while (statement.Step()) {
    TypedUrlResult result;
    result.url = GURL(statement.ColumnString(0));
    result.title = statement.ColumnString(1);
    result.visit_count = statement.ColumnInt(2);
    bool is_keyword = statement.ColumnBool(3);
    if (is_keyword) {
      result.terms = statement.ColumnString(4);
    }
    results.push_back(result);
  }

  return results;
}

bool URLDatabase::CreateVivaldiURLsLastVisitIndex() {
  return GetDB().Execute(
      "CREATE INDEX IF NOT EXISTS urls_idx_last_visit_time ON "
      "urls(last_visit_time desc);");
}

bool URLDatabase::CreateVivaldiURLScoreIndex() {
  sql::Statement statement(GetDB().GetUniqueStatement(
  "SELECT sql FROM sqlite_master WHERE type = 'index' AND name ='urls_score_index';"));
  std::string sql("CREATE INDEX urls_score_index ON urls( ");
  sql.append(scoring);
  sql.append(")");
  if(!statement.Step()){
  return GetDB().Execute(sql);
  }
  if(sql.compare(statement.ColumnString(0))){
    statement.Step();//Need to step beyond the last result to unlock the DB
    std::ignore = GetDB().Execute("DROP INDEX IF EXISTS urls_score_index;");
    return GetDB().Execute(sql);
  }
  return 0;

}

#endif

}  // namespace history
