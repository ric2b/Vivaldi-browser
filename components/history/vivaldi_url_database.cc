// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/history/core/browser/url_database.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

namespace history {

URLDatabase::TypedUrlResult::TypedUrlResult() {}

URLDatabase::TypedUrlResult& URLDatabase::TypedUrlResult::operator=(
    const TypedUrlResult&) = default;

URLDatabase::TypedUrlResult::TypedUrlResult(const TypedUrlResult&) = default;

bool URLDatabase::GetVivaldiTypedHistory(const std::string query,
                                         int prefix_keyword,
                                         int max_results,
                                         TypedUrlResults* results) {
  results->clear();

  std::string sql("SELECT u.url, u.title, ");
  sql.append("k.url_id IS NOT NULL, k.keyword_id, k.lower_term ");
  sql.append("FROM urls AS u ");
  sql.append("LEFT JOIN keyword_search_terms AS k ON u.id = k.url_id ");
  sql.append("WHERE (u.typed_count > 0 AND u.url LIKE ?) ");
  sql.append("OR k.lower_term LIKE ? ");
  if (prefix_keyword != -1)
    sql.append("OR (k.keyword_id = ? AND k.lower_term LIKE ?) ");
  sql.append("ORDER BY u.last_visit_time DESC LIMIT ?");

  base::string16 lower_query(base::i18n::ToLower(base::UTF8ToUTF16(query)));
  sql::Statement statement(GetDB().GetUniqueStatement(sql.c_str()));
  const base::string16 wild(base::UTF8ToUTF16("%"));
  const std::string wild8("%");
  statement.BindString(0, wild8 + query + wild8);
  statement.BindString16(1, wild + lower_query + wild);
  if (prefix_keyword != -1) {
    statement.BindInt(2, prefix_keyword);
    statement.BindString16(
        3, wild +
               lower_query.substr(
                   lower_query.find_first_of(base::UTF8ToUTF16(" ")) + 1) +
               wild);
    statement.BindInt(4, max_results);
  } else {
    statement.BindInt(2, max_results);
  }

  while (statement.Step()) {
    TypedUrlResult result;
    result.url = GURL(statement.ColumnString(0));
    result.title = statement.ColumnString(1);
    bool is_keyword = statement.ColumnBool(2);
    if (is_keyword) {
      result.keyword_id = statement.ColumnInt(3);
      result.terms = statement.ColumnString(4);
    } else {
      result.keyword_id = -1;
    }
    results->push_back(result);
  }

  return !results->empty();
}

bool URLDatabase::GetMatchesWStatement(const char* sql_statement,
  const std::string& search_string,
  int max_hits,
  URLRows* results) {
  results->clear();
  sql::Statement statement(
    GetDB().GetCachedStatement(SQL_FROM_HERE, sql_statement));

  statement.BindString(0, search_string);
  statement.BindString(1, search_string);
  statement.BindInt(2, max_hits);

  while (statement.Step()) {
    URLResult info;
    FillURLRow(statement, &info);
    if (info.url().is_valid())
      results->push_back(info);
  }
  return !results->empty();
}

}  // namespace history
