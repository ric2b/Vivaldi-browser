// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/bookmarks/browser/bookmark_node.h"

#include "base/strings/utf_string_conversions.h"

#include "components/bookmarks/vivaldi_bookmark_kit.h"

namespace bookmarks {

// BookmarkNode ---------------------------------------------------------------

const base::string16 BookmarkNode::GetTitledUrlNodeNickName() const {
  return base::UTF8ToUTF16(vivaldi_bookmark_kit::GetNickname(this));
}

const base::string16 BookmarkNode::GetTitledUrlNodeDescription() const {
  return base::UTF8ToUTF16(vivaldi_bookmark_kit::GetDescription(this));
}

}  // namespace bookmarks
