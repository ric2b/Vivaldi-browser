// included by the class definition in
// chromium/components/bookmarks/browser/core_bookmark_model.h

// Returns up bookmarks containing each term from `query` in the nickname.
// `matching_algorithm` determines the
// algorithm used by QueryParser internally to parse `query`. `max_count_hint`
// is used as approximate maximum count, but implementations are allowed to
// exceed this maximum.
[[nodiscard]] virtual std::vector<TitledUrlMatch> GetNicknameMatching(
    const std::u16string& query,
    size_t max_count_hint) const = 0;