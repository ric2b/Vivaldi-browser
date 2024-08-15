// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/bookmarks/browser/bookmark_model.h"

#include "components/bookmarks/browser/titled_url_index.h"
#include "components/bookmarks/browser/titled_url_match.h"

namespace bookmarks {

std::vector<TitledUrlMatch> BookmarkModel::GetNicknameMatching(
    const std::u16string& query,
    size_t max_count_hint) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!loaded_) {
    return {};
  }

  return titled_url_index_->GetResultsNicknameMatching(query, max_count_hint);
}

}  // namespace bookmarks
