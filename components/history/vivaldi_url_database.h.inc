 public:
  // Vivaldi
  struct TypedUrlResult {
    TypedUrlResult();
    TypedUrlResult& operator=(const TypedUrlResult&);
    TypedUrlResult(const TypedUrlResult&);

    GURL url;
    std::string title;
    KeywordID keyword_id = -1;
    std::string terms;
    int typed_count;
  };

  typedef std::vector<TypedUrlResult> TypedUrlResults;

  bool GetVivaldiTypedHistory(const std::string query,
                              KeywordID prefix_keyword,
                              int max_results,
                              TypedUrlResults* results);

  // History search ------------------------------------------------------------
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  bool GetDetailedMatchesWStatement(
                        const char* sql_statement,
                        const std::string& search_string,
                        int max_hits,
                        history::DetailedHistory::DetailedHistoryList* results);
#endif