// included by the class definition in
// "chromium/components/bookmarks/browser/titled_url_index.h"

public:
  // Returns up to `max_count` of matches containing each term from the text
  // `query` in the nickname of ancestor nodes.
  // `matching_algorithm` determines the algorithm used by QueryParser
  // internally to parse `query`.
  std::vector<TitledUrlMatch> GetResultsNicknameMatching(
    const std::u16string& query,
    size_t max_count);

private:

  // A map of terms and the nodes containing those terms in their nickname
  Index nickname_index_;

  // For each node, calls `MatchNicknameNodeWithQuery()` and returns the
  // aggregated `TitledUrlMatch`s.
  std::vector<TitledUrlMatch> MatchNicknameNodesWithQuery(
      const TitledUrlNodes& nodes,
      const query_parser::QueryNodeVector& query_nodes,
      const std::vector<std::u16string>& query_terms,
      size_t max_count);

  // Finds |query_nodes| matches in |node| and returns a TitledUrlMatch
  // containing |node| and the matches.
  std::optional<TitledUrlMatch> MatchNicknameNodeWithQuery(
      const TitledUrlNode* node,
      const query_parser::QueryNodeVector& query_nodes,
      const std::vector<std::u16string>& query_terms);

  // Removes |node| from |nickname_index_|.
  void UnregisterNicknameNode(const std::u16string& term,
                              const TitledUrlNode* node);


  TitledUrlNodeSet RetrieveNicknameNodesMatchingAnyTerms(
      const std::vector<std::u16string>& terms) const;