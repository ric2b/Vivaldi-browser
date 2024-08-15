// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/bookmarks/browser/bookmark_node.h"

#include "app/vivaldi_resources.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "ui/base/l10n/l10n_util.h"

namespace bookmarks {

// Vivaldi: Returns the nickname for the node.
const std::u16string TitledUrlNode::GetTitledUrlNodeNickName() const {
  return std::u16string();
}

// Vivaldi: Returns the description for the node.
const std::u16string TitledUrlNode::GetTitledUrlNodeDescription() const {
  return std::u16string();
}

// BookmarkNode ---------------------------------------------------------------

// Below predefined UUIDs for permanent bookmark folders, determined via named
// UUIDs. Do NOT modify them as they may be exposed via Sync. For
// reference, here's the python script to produce them:
// > import uuid
// > vivaldi_namespace = uuid.uuid5(uuid.NAMESPACE_DNS, "vivaldi.com")
// > bookmarks_namespace = uuid.uuid5(vivaldi_namespace, "bookmarks")
// > trash_uuid = uuid.uuid5(bookmarks_namespace, "trash")
const char kVivaldiTrashNodeUuid[] = "9f32a0fb-bfd9-5032-be46-07afe4a25400";

const std::u16string BookmarkNode::GetTitledUrlNodeNickName() const {
  return base::UTF8ToUTF16(vivaldi_bookmark_kit::GetNickname(this));
}

const std::u16string BookmarkNode::GetTitledUrlNodeDescription() const {
  return base::UTF8ToUTF16(vivaldi_bookmark_kit::GetDescription(this));
}

// static
std::unique_ptr<BookmarkPermanentNode> BookmarkPermanentNode::CreateTrashFolder(
    int64_t id) {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new BookmarkPermanentNode(
      id, TRASH, base::Uuid::ParseLowercase(kVivaldiTrashNodeUuid),
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_TRASH_FOLDER_NAME)));
}

}  // namespace bookmarks
