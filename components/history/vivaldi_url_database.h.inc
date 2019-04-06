 public:
  // Vivaldi
  struct TypedUrlResult {
    TypedUrlResult();
    TypedUrlResult& operator=(const TypedUrlResult&);
    TypedUrlResult(const TypedUrlResult&);

    GURL url;
    std::string title;
    int keyword_id = -1;
    std::string terms;
  };

  typedef std::vector<TypedUrlResult> TypedUrlResults;

  bool GetVivaldiTypedHistory(const std::string query,
                              int prefix_keyword,
                              int max_results,
                              TypedUrlResults* results);

  // History search ------------------------------------------------------------
  bool GetMatchesWStatement(const char* sql_statement,
                            const std::string& search_string,
                            int max_hits,
                            URLRows* results);
