// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"

namespace bookmarks {

namespace {

// Helper to get a mutable bookmark node.
// Copied from the Chromium BookmarkModel class (.cc) for convenience.
BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

} // namespace

void BookmarkModel::SetDescription(const BookmarkNode* node,
                                   const base::string16& desc) {
  DCHECK(node);

  if (node->GetDescription() == desc)
    return;

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillChangeBookmarkNode(this, node);

  if (node->is_url())
    index_->Remove(node);
  AsMutable(node)->set_description(desc);
  if (node->is_url())
    index_->Add(node);

  if (store_)
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeChanged(this, node);
}

void BookmarkModel::SetNickName(const BookmarkNode* node,
                                const base::string16& nick_name) {
  DCHECK(node);

  if (node->GetNickName() == nick_name)
    return;

  if (node->is_url())
    index_->Remove(node);
  AsMutable(node)->set_nickname(nick_name);
  if (node->is_url())
    index_->Add(node);

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillChangeBookmarkNode(this, node);

  if (store_)
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeChanged(this, node);
}

void BookmarkModel::SetFolderAsSpeedDial(const BookmarkNode* node,
                                         bool is_speeddial) {
  DCHECK(node);
  DCHECK(node->is_folder());

  if (node->GetSpeeddial() == is_speeddial)
    return;

  SetNodeMetaInfo(node, "Speeddial", (is_speeddial) ? "true" : "false");

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkSpeedDialNodeChanged(this, node);
}

}  // namespace bookmarks
