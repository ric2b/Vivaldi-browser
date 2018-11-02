URLDatabase::TypedUrlResult::TypedUrlResult() : keyword_id(-1) {}

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
