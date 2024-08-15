// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"

#include <utility>

#include "base/containers/contains.h"
#include "components/bookmarks/browser/bookmark_model.h"

// Vivaldi
#include "components/bookmarks/vivaldi_bookmark_kit.h"
// End Vivaldi

namespace {

// Removes all subnodes of `node`, including `node`, that are in `bookmarks`.
void RemoveBookmarksRecursive(
    const std::set<const bookmarks::BookmarkNode*>& bookmarks,
    bookmarks::BookmarkModel* model,
    bookmarks::metrics::BookmarkEditSource source,
    const bookmarks::BookmarkNode* node) {
  // Remove children in reverse order, so that the index remains valid.
  for (size_t i = node->children().size(); i > 0; --i) {
    RemoveBookmarksRecursive(bookmarks, model, source,
                             node->children()[i - 1].get());
  }

  if (base::Contains(bookmarks, node)) {
    model->Remove(node, source);
  }
}

}  // namespace

LegacyBookmarkModel::LegacyBookmarkModel() = default;

LegacyBookmarkModel::~LegacyBookmarkModel() = default;

const bookmarks::BookmarkNode*
LegacyBookmarkModel::subtle_root_node_with_unspecified_children() const {
  return underlying_model()->root_node();
}

void LegacyBookmarkModel::Shutdown() {
  underlying_model()->Shutdown();
}

bool LegacyBookmarkModel::loaded() const {
  return underlying_model()->loaded();
}

void LegacyBookmarkModel::Remove(
    const bookmarks::BookmarkNode* node,
    bookmarks::metrics::BookmarkEditSource source) {
  underlying_model()->Remove(node, source);
}

void LegacyBookmarkModel::Move(const bookmarks::BookmarkNode* node,
                               const bookmarks::BookmarkNode* new_parent,
                               size_t index) {
  underlying_model()->Move(node, new_parent, index);
}

void LegacyBookmarkModel::Copy(const bookmarks::BookmarkNode* node,
                               const bookmarks::BookmarkNode* new_parent,
                               size_t index) {
  underlying_model()->Copy(node, new_parent, index);
}

void LegacyBookmarkModel::SetTitle(
    const bookmarks::BookmarkNode* node,
    const std::u16string& title,
    bookmarks::metrics::BookmarkEditSource source) {
  underlying_model()->SetTitle(node, title, source);
}

void LegacyBookmarkModel::SetURL(
    const bookmarks::BookmarkNode* node,
    const GURL& url,
    bookmarks::metrics::BookmarkEditSource source) {
  underlying_model()->SetURL(node, url, source);
}

void LegacyBookmarkModel::SetDateAdded(const bookmarks::BookmarkNode* node,
                                       base::Time date_added) {
  underlying_model()->SetDateAdded(node, date_added);
}

bool LegacyBookmarkModel::HasNoUserCreatedBookmarksOrFolders() const {
  return (!bookmark_bar_node() || bookmark_bar_node()->children().empty()) &&
         (!other_node() || other_node()->children().empty()) &&
         (!mobile_node() || mobile_node()->children().empty());
}

#if defined(VIVALDI_BUILD)
const BookmarkNode* LegacyBookmarkModel::AddFolder(
      const BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const BookmarkNode::MetaInfoMap* meta_info,
      std::optional<base::Time> creation_time,
      std::optional<base::Uuid> uuid) {
  return underlying_model()->AddFolder(parent, index, title,
                                       meta_info, creation_time, uuid);
}

const BookmarkNode* LegacyBookmarkModel::AddNewURL(
      const BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url,
      const BookmarkNode::MetaInfoMap* meta_info) {
  return underlying_model()->AddNewURL(parent, index, title, url, meta_info);
}

const BookmarkNode* LegacyBookmarkModel::AddURL(
      const BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url,
      const BookmarkNode::MetaInfoMap* meta_info,
      std::optional<base::Time> creation_time,
      std::optional<base::Uuid> uuid,
      bool added_by_user) {
  return underlying_model()->AddURL(parent, index,
                                    title, url, meta_info,
                                    creation_time, uuid, added_by_user);
}
#else
const bookmarks::BookmarkNode* LegacyBookmarkModel::AddFolder(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title) {
  return underlying_model()->AddFolder(parent, index, title);
}

const bookmarks::BookmarkNode* LegacyBookmarkModel::AddNewURL(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url) {
  return underlying_model()->AddNewURL(parent, index, title, url);
}

const bookmarks::BookmarkNode* LegacyBookmarkModel::AddURL(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url) {
  return underlying_model()->AddURL(parent, index, title, url);
}
#endif // End Vivaldi

void LegacyBookmarkModel::RemoveMany(
    const std::set<const bookmarks::BookmarkNode*>& nodes,
    bookmarks::metrics::BookmarkEditSource source) {
  RemoveBookmarksRecursive(nodes, underlying_model(), source,
                           underlying_model()->root_node());
}

void LegacyBookmarkModel::CommitPendingWriteForTest() {
  underlying_model()->CommitPendingWriteForTest();
}

// Vivaldi
bool LegacyBookmarkModel::DoesNickExists(const std::string& nickname,
                                         const BookmarkNode* updated_node) {
  return vivaldi_bookmark_kit::DoesNickExists(underlying_model(),
                                              nickname,
                                              updated_node);
}

void LegacyBookmarkModel::SetNodeNickname(const BookmarkNode* node,
                                          const std::string& nickname) {
  vivaldi_bookmark_kit::SetNodeNickname(underlying_model(), node, nickname);
}

void LegacyBookmarkModel::SetNodeDescription(const BookmarkNode* node,
                                             const std::string& description) {
  vivaldi_bookmark_kit::SetNodeDescription(underlying_model(), node, description);
}

void LegacyBookmarkModel::SetNodeSpeeddial(const BookmarkNode* node,
                                           bool speeddial) {
  vivaldi_bookmark_kit::SetNodeSpeeddial(underlying_model(), node, speeddial);
}

void LegacyBookmarkModel::SetNodeThumbnail(const BookmarkNode* node,
                                           const std::string& path) {
  vivaldi_bookmark_kit::SetNodeThumbnail(underlying_model(), node, path);
}

void LegacyBookmarkModel::SetNodeMetaInfoMap(const BookmarkNode* node,
  const BookmarkNode::MetaInfoMap& meta_info_map) {
  underlying_model()->SetNodeMetaInfoMap(node, meta_info_map);
}

void LegacyBookmarkModel::RemovePartnerId(const BookmarkNode* node) {
  vivaldi_bookmark_kit::RemovePartnerId(underlying_model(), node);
}
// End Vivaldi
