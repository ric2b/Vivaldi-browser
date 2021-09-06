// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/bookmarks/browser/bookmark_node.h"

#include "app/vivaldi_resources.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "ui/base/l10n/l10n_util.h"

namespace bookmarks {

// Vivaldi: Returns the nickname for the node.
const base::string16 TitledUrlNode::GetTitledUrlNodeNickName() const {
  return base::string16();
}

// Vivaldi: Returns the description for the node.
const base::string16 TitledUrlNode::GetTitledUrlNodeDescription() const {
  return base::string16();
}

// BookmarkNode ---------------------------------------------------------------

const char BookmarkNode::kVivaldiTrashNodeGuid[] =
    "00000000-0000-4000-a000-000000040000";

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
      id, TRASH, base::GUID::ParseLowercase(kVivaldiTrashNodeGuid),
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_TRASH_FOLDER_NAME),
      visible_when_empty));
}

}  // namespace bookmarks
