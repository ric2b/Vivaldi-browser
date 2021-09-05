// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/bookmarks/browser/bookmark_node.h"

#include "app/vivaldi_resources.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "ui/base/l10n/l10n_util.h"

namespace bookmarks {

// BookmarkNode ---------------------------------------------------------------

const base::string16 BookmarkNode::GetTitledUrlNodeNickName() const {
  return base::UTF8ToUTF16(vivaldi_bookmark_kit::GetNickname(this));
}

const base::string16 BookmarkNode::GetTitledUrlNodeDescription() const {
  return base::UTF8ToUTF16(vivaldi_bookmark_kit::GetDescription(this));
}

// static
std::unique_ptr<BookmarkPermanentNode> BookmarkPermanentNode::CreateTrashFolder(
    int64_t id,
    bool visible_when_empty) {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new BookmarkPermanentNode(
      id, TRASH, kVivaldiTrashNodeGuid,
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_TRASH_FOLDER_NAME),
      visible_when_empty));
}

}  // namespace bookmarks
