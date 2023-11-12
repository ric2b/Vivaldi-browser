// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_BOOKMARKS_VIVALDI_BOOKMARK_KIT_H_
#define COMPONENTS_BOOKMARKS_VIVALDI_BOOKMARK_KIT_H_

#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "third_party/skia/include/core/SkColor.h"

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

const std::string& ThumbnailString();

// Wrapper around BookmarkNode::MetaInfoMap to set Vivaldi-specific properties.
class CustomMetaInfo {
 public:
  CustomMetaInfo();
  ~CustomMetaInfo();
  CustomMetaInfo(const CustomMetaInfo&) = delete;
  CustomMetaInfo& operator=(const CustomMetaInfo&) = delete;

  const BookmarkNode::MetaInfoMap* map() const { return &map_; }
  void SetMap(const BookmarkNode::MetaInfoMap& map);

  void Clear();

  void SetSpeeddial(bool speeddial);
  void SetBookmarkbar(bool bookmarkbar);
  void SetNickname(const std::string& nickname);
  void SetDescription(const std::string& description);
  void SetPartner(const base::Uuid& partner);
  void SetThumbnail(const std::string& thumbnail);

 private:
  BookmarkNode::MetaInfoMap map_;
};

bool GetSpeeddial(const BookmarkNode* node);
bool GetBookmarkbar(const BookmarkNode* node);
const std::string& GetNickname(const BookmarkNode* node);
const std::string& GetDescription(const BookmarkNode* node);
const base::Uuid GetPartner(const BookmarkNode::MetaInfoMap& meta_info_map);
const base::Uuid GetPartner(const BookmarkNode* node);
const std::string& GetThumbnail(const BookmarkNode* node);
const std::string& GetThumbnail(const BookmarkNode::MetaInfoMap& meta_info_map);
SkColor GetThemeColor(const BookmarkNode* node);
std::string GetThemeColorForCSS(const BookmarkNode* node);

bool IsSeparator(const BookmarkNode* node);
bool IsTrash(const BookmarkNode* node);

void InitModelNonClonedKeys(BookmarkModel* bookmark_model);

// Returns true if the nickname exists in the bookmark model, false otherwise
// If updated_node is null then the bookmark is being created, otherwise the
// nickname is being updated.
bool DoesNickExists(const BookmarkModel* model,
                    const std::string& nickname,
                    const BookmarkNode* updated_node);

bool SetBookmarkThumbnail(BookmarkModel* model,
                          int64_t bookmark_id,
                          const std::string& url);

void RemovePartnerId(BookmarkModel* model, const BookmarkNode* node);

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
void SetNodeThumbnail(BookmarkModel* model,
                      const BookmarkNode* node,
                      const std::string& path);
void SetNodeThemeColor(BookmarkModel* model,
                       const BookmarkNode* node,
                       SkColor theme_color);

typedef base::RepeatingCallback<bool(const std::string&)> BookmarkWriteFunc;
bool WriteBookmarkData(const base::Value::Dict& value,
                       BookmarkWriteFunc write_func,
                       BookmarkWriteFunc write_func_att);

typedef base::RepeatingCallback<bool(const std::string&, std::string*)>
    BookmarkAttributeReadFunc;
typedef base::FunctionRef<void(const std::string&, std::u16string*)>
    CodePagetoUTF16Func;
void ReadBookmarkAttributes(BookmarkAttributeReadFunc GetAttribute,
                            CodePagetoUTF16Func CodePagetoUTF16,
                            std::u16string* nickname,
                            std::u16string* description,
                            bool* is_speeddial_folder);

}  // namespace vivaldi_bookmark_kit

#endif  // COMPONENTS_BOOKMARKS_VIVALDI_BOOKMARK_KIT_H_
