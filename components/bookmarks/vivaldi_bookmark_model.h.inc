
  // Returns up to `max_count_hint` bookmarks containing each term from `query`
  // in the Nickname `matching_algorithm`
  // determines the algorithm used by QueryParser internally to parse `query`.
  [[nodiscard]] std::vector<TitledUrlMatch> GetNicknameMatching(
      const std::u16string& query,
      size_t max_count_hint) const;

  void RemoveNodeFromSearchIndexRecursive(BookmarkNode* node);
  void AddNodeToSearchIndexRecursive(const BookmarkNode* node);
