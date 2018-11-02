 public:
  // Vivaldi
  struct TypedUrlResult {
    TypedUrlResult();
    TypedUrlResult& operator=(const TypedUrlResult&);
    TypedUrlResult(const TypedUrlResult&);

    GURL url;
    std::string title;
    int keyword_id;
    std::string terms;
  };
  typedef std::vector<TypedUrlResult> TypedUrlResults;

  bool GetVivaldiTypedHistory(const std::string query,
                              int prefix_keyword,
                              int max_results,
                              TypedUrlResults* results);
