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

void BookmarkModel::RemoveNodeFromSearchIndexRecursive(BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(!is_permanent_node(node));

  if (node->is_url()) {
    titled_url_index_->Remove(node);
  } else {
    titled_url_index_->RemovePath(node);
  }

  for (size_t i = node->children().size(); i > 0; --i) {
    RemoveNodeFromSearchIndexRecursive(node->children()[i - 1].get());
  }
}

void BookmarkModel::AddNodeToSearchIndexRecursive(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (node->is_url()) {
    titled_url_index_->Add(node);
  } else {
    titled_url_index_->AddPath(node);
  }

  for (const auto& child : node->children()) {
    AddNodeToSearchIndexRecursive(child.get());
  }
}
}  // namespace bookmarks
