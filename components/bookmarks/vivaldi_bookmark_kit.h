// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_BOOKMARKS_VIVALDI_BOOKMARK_KIT_H_
#define COMPONENTS_BOOKMARKS_VIVALDI_BOOKMARK_KIT_H_

#include <string>

#include "base/time/time.h"
#include "base/memory/weak_ptr.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace bookmarks {
class BookmarkModel;
}

// A collection of helper classes and utilities to extend Chromium BookmarkModel
// and BookmarkNode functionality.

namespace vivaldi_bookmark_kit {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

// Run the callback after the model is loaded. model argument to the callback
// will be null when the callback is called while the model was deleted when
// waiting.
using RunAfterModelLoadCallback =
    base::OnceCallback<void(BookmarkModel* model)>;
void RunAfterModelLoad(BookmarkModel* model,
                       RunAfterModelLoadCallback callback);

base::WeakPtr<BookmarkModel> GetModelWeakPtr(BookmarkModel* model);

const std::string& ThumbnailString();

// Wrapper around BookmarkNode::MetaInfoMap to set Vivaldi-specific properties.
class CustomMetaInfo {
 public:
  CustomMetaInfo();
  ~CustomMetaInfo();

  const BookmarkNode::MetaInfoMap* map() const { return &map_; }
  void SetMap(const BookmarkNode::MetaInfoMap& map);

  void Clear();

  void SetSpeeddial(bool speeddial);
  void SetBookmarkbar(bool bookmarkbar);
  void SetNickname(const std::string& nickname);
  void SetDescription(const std::string& description);
  void SetPartner(const std::string& partner);
  void SetThumbnail(const std::string& thumbnail);

 private:
  BookmarkNode::MetaInfoMap map_;
  DISALLOW_COPY_AND_ASSIGN(CustomMetaInfo);
};

bool GetSpeeddial(const BookmarkNode* node);
bool GetBookmarkbar(const BookmarkNode* node);
const std::string& GetNickname(const BookmarkNode* node);
const std::string& GetDescription(const BookmarkNode* node);
const std::string* GetPartner(const BookmarkNode::MetaInfoMap& meta_info_map);
const std::string& GetPartner(const BookmarkNode* node);
const std::string& GetThumbnail(const BookmarkNode* node);

bool IsSeparator(const BookmarkNode* node);

void InitModelNonClonedKeys(BookmarkModel* bookmark_model);

// Returns true if the nickname exists in the bookmark model, false otherwise
// If updated_node is null then the bookmark is being created, otherwise the
// nickname is being updated.
bool DoesNickExists(const BookmarkModel* model,
                    const std::string& nickname,
                    const BookmarkNode* updated_node);

// Android-specific functions

void SetNodeNickname(BookmarkModel* model,
                     const BookmarkNode* node,
                     const std::string& nickname);
void SetNodeDescription(BookmarkModel* model,
                        const BookmarkNode* node,
                        const std::string& description);
void SetNodeSpeeddial(BookmarkModel* model,
                      const BookmarkNode* node,
                      bool speeddial);

}  // namespace vivaldi_bookmark_kit

#endif  // COMPONENTS_BOOKMARKS_VIVALDI_BOOKMARK_KIT_H_
