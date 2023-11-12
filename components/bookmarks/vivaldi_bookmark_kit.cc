// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/bookmarks/vivaldi_bookmark_kit.h"

#include <string>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_codec.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "ui/base/models/tree_node_iterator.h"

namespace bookmarks {

namespace {

// Copied from the Chromium BookmarkModel class (.cc) for convenience.
BookmarkNode* AsMutableNode(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

}  // namespace

// Helper to access BookmarkModel private members
class VivaldiBookmarkModelFriend {
 public:
  // Android-specific method to change meta that also affect url index
  static void SetNodeMetaInfoWithIndexChange(BookmarkModel* model,
                                             const BookmarkNode* node,
                                             const std::string& key,
                                             const std::string& value) {
    // NOTE(igor@vivaldi.com): Follow BookmarkModel::SetTitle().
    DCHECK(node);

    std::string old_value;
    node->GetMetaInfo(key, &old_value);
    if (value == old_value)
      return;

    for (BookmarkModelObserver& observer : model->observers_) {
      observer.OnWillChangeBookmarkNode(model, node);
    }

    if (node->is_url()) {
      model->titled_url_index_->Remove(node);
    }
    if (!value.empty()) {
      AsMutableNode(node)->SetMetaInfo(key, value);
    } else {
      AsMutableNode(node)->DeleteMetaInfo(key);
    }
    if (node->is_url()) {
      model->titled_url_index_->Add(node);
    }

    if (model->store_)
      model->store_->ScheduleSave();

    for (BookmarkModelObserver& observer : model->observers_) {
      observer.BookmarkNodeChanged(model, node);
    }
  }
};

}  // namespace bookmarks

namespace vivaldi_bookmark_kit {

using bookmarks::VivaldiBookmarkModelFriend;

namespace {

class BookmarkModelLoadWaiter : public bookmarks::BaseBookmarkModelObserver {
 public:
  BookmarkModelLoadWaiter(RunAfterModelLoadCallback callback)
      : callback_(std::move(callback)) {}
  BookmarkModelLoadWaiter(const BookmarkModelLoadWaiter&) = delete;
  BookmarkModelLoadWaiter& operator=(const BookmarkModelLoadWaiter&) = delete;

 private:
  ~BookmarkModelLoadWaiter() override = default;

  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override {}

  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override {
    model->RemoveObserver(this);
    RunAfterModelLoadCallback callback = std::move(callback_);
    delete this;
    std::move(callback).Run(model);
  }

  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override {
    // The model can be deleted before AfterBookmarkModelLoaded is called.
    // Ensure that we do not leak this and prevent AfterBookmarkModelLoaded from
    // being called.
    RunAfterModelLoadCallback callback = std::move(callback_);
    delete this;
    LOG(ERROR) << "Model was deleted";
    std::move(callback).Run(nullptr);
  }

  RunAfterModelLoadCallback callback_;
};

// Struct holding std::string constants with names that we use to get/set values
// from bookmark meta data. This avoids running string destructors at shutdown.
struct VivaldiMetaNames {
  // meta data keys
  const std::string speeddial = "Speeddial";
  const std::string bookmarkbar = "Bookmarkbar";
  const std::string nickname = "Nickname";
  const std::string description = "Description";
  const std::string partner = "Partner";
  const std::string thumbnail = "Thumbnail";
  const std::string theme_color = "ThemeColor";

  // values stored in the node
  const std::string true_value = "true";
  const std::u16string tripple_dash = u"---";
  const std::string separator = "separator";
};

const VivaldiMetaNames& GetMetaNames() {
  static base::NoDestructor<VivaldiMetaNames> instance;
  return *instance;
}

const std::string& GetMetaString(const BookmarkNode::MetaInfoMap& meta_info_map,
                                 const std::string& key) {
  auto i = meta_info_map.find(key);
  if (i == meta_info_map.end())
    return base::EmptyString();
  return i->second;
}

const std::string& GetMetaString(const BookmarkNode* node,
                                 const std::string& key) {
  DCHECK(node);
  if (const BookmarkNode::MetaInfoMap* meta_info_map = node->GetMetaInfoMap()) {
    return GetMetaString(*meta_info_map, key);
  }
  return base::EmptyString();
}

bool GetMetaBool(const BookmarkNode* node, const std::string& key) {
  const std::string& value = GetMetaString(node, key);
  return value == GetMetaNames().true_value;
}

void SetMetaBool(BookmarkNode::MetaInfoMap* map,
                 const std::string& key,
                 bool value) {
  if (value) {
    (*map)[key] = GetMetaNames().true_value;
  } else {
    map->erase(key);
  }
}

void SetMetaString(BookmarkNode::MetaInfoMap* map,
                   const std::string& key,
                   const std::string& value) {
  if (!value.empty()) {
    (*map)[key] = value;
  } else {
    map->erase(key);
  }
}

}  // namespace

void RunAfterModelLoad(bookmarks::BookmarkModel* model,
                       RunAfterModelLoadCallback callback) {
  if (!model || model->loaded()) {
    std::move(callback).Run(model);
    return;
  }
  // waiter will be deleted after receiving BookmarkModelLoaded or
  // BookmarkModelBeingDeleted notifications.
  BookmarkModelLoadWaiter* waiter =
      new BookmarkModelLoadWaiter(std::move(callback));
  model->AddObserver(waiter);
}

const std::string& ThumbnailString() {
  return GetMetaNames().thumbnail;
}

CustomMetaInfo::CustomMetaInfo() = default;
CustomMetaInfo::~CustomMetaInfo() = default;

void CustomMetaInfo::SetMap(const BookmarkNode::MetaInfoMap& map) {
  map_ = map;
}

void CustomMetaInfo::Clear() {
  map_.clear();
}

void CustomMetaInfo::SetSpeeddial(bool speeddial) {
  SetMetaBool(&map_, GetMetaNames().speeddial, speeddial);
}

void CustomMetaInfo::SetBookmarkbar(bool bookmarkbar) {
  SetMetaBool(&map_, GetMetaNames().bookmarkbar, bookmarkbar);
}

void CustomMetaInfo::SetNickname(const std::string& nickname) {
  SetMetaString(&map_, GetMetaNames().nickname, nickname);
}

void CustomMetaInfo::SetDescription(const std::string& description) {
  SetMetaString(&map_, GetMetaNames().description, description);
}

void CustomMetaInfo::SetPartner(const base::Uuid& partner) {
  SetMetaString(&map_, GetMetaNames().partner, partner.AsLowercaseString());
}

void CustomMetaInfo::SetThumbnail(const std::string& thumbnail) {
  SetMetaString(&map_, GetMetaNames().thumbnail, thumbnail);
}

bool GetSpeeddial(const BookmarkNode* node) {
  return GetMetaBool(node, GetMetaNames().speeddial);
}

bool GetBookmarkbar(const BookmarkNode* node) {
  return GetMetaBool(node, GetMetaNames().bookmarkbar);
}

const std::string& GetNickname(const BookmarkNode* node) {
  return GetMetaString(node, GetMetaNames().nickname);
}

const std::string& GetDescription(const BookmarkNode* node) {
  return GetMetaString(node, GetMetaNames().description);
}

SkColor GetThemeColor(const BookmarkNode* node) {
  std::string theme_color_str = GetMetaString(node, GetMetaNames().theme_color);
  if (theme_color_str.empty())
    return SK_ColorTRANSPARENT;

  SkColor theme_color;
  if (!base::StringToUint(theme_color_str, &theme_color))
    return SK_ColorTRANSPARENT;
  return theme_color;
}

std::string GetThemeColorForCSS(const BookmarkNode* node) {
  SkColor theme_color = GetThemeColor(node) ;
  if (theme_color == SK_ColorTRANSPARENT)
    return "";

  std::vector<uint8_t> theme_rgb;
  theme_rgb.push_back(SkColorGetR(theme_color));
  theme_rgb.push_back(SkColorGetG(theme_color));
  theme_rgb.push_back(SkColorGetB(theme_color));

  return std::string("#") + base::HexEncode(theme_rgb);
}

const base::Uuid GetPartner(const BookmarkNode::MetaInfoMap& meta_info_map) {
  base::Uuid partner_id;
  const std::string& partner_string =
      GetMetaString(meta_info_map, GetMetaNames().partner);
  if (!partner_string.empty()) {
    partner_id = base::Uuid::ParseCaseInsensitive(partner_string);
    if (!partner_id.is_valid()) {
      LOG(ERROR) << "Invalid Uuid as a partner id - " << partner_string;
    }
  }
  return partner_id;
}

const base::Uuid GetPartner(const BookmarkNode* node) {
  base::Uuid partner_id;
  const std::string& partner_string =
      GetMetaString(node, GetMetaNames().partner);
  if (!partner_string.empty()) {
    partner_id = base::Uuid::ParseCaseInsensitive(partner_string);
    if (!partner_id.is_valid()) {
      LOG(ERROR) << "Invalid Uuid as a partner id - " << partner_string;
    }
  }
  return partner_id;
}

const std::string& GetThumbnail(const BookmarkNode* node) {
  return GetMetaString(node, GetMetaNames().thumbnail);
}

const std::string& GetThumbnail(
    const BookmarkNode::MetaInfoMap& meta_info_map) {
  return GetMetaString(meta_info_map, GetMetaNames().thumbnail);
}

bool IsSeparator(const BookmarkNode* node) {
  // TODO(espen@vivaldi.com): Add separator flag to node. Needed many places.
  return node->GetTitle() == GetMetaNames().tripple_dash &&
         GetDescription(node) == GetMetaNames().separator;
}

bool IsTrash(const BookmarkNode* node) {
  return node->type() == bookmarks::BookmarkNode::TRASH;
}

void InitModelNonClonedKeys(BookmarkModel* model) {
  model->AddNonClonedKey(GetMetaNames().nickname);
  model->AddNonClonedKey(GetMetaNames().partner);
  model->AddNonClonedKey(GetMetaNames().speeddial);
  model->AddNonClonedKey(GetMetaNames().bookmarkbar);
}

bool DoesNickExists(const BookmarkModel* model,
                    const std::string& nickname,
                    const BookmarkNode* updated_node) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (GetNickname(node) == nickname && node != updated_node)
      return true;
  }
  return false;
}

bool SetBookmarkThumbnail(BookmarkModel* model,
                          int64_t bookmark_id,
                          const std::string& url) {
  // model should be loaded as bookmark_id comes from it.
  DCHECK(model->loaded());
  const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(model, bookmark_id);
  if (!node) {
    LOG(ERROR) << "Failed to locate bookmark with id " << bookmark_id;
    return false;
  }
  if (model->is_permanent_node(node)) {
    LOG(ERROR) << "Cannot modify special bookmark " << bookmark_id;
    return false;
  }
  model->SetNodeMetaInfo(node, GetMetaNames().thumbnail, url);
  return true;
}

void RemovePartnerId(BookmarkModel* model, const BookmarkNode* node) {
  model->DeleteNodeMetaInfo(node, GetMetaNames().partner);
}

void SetNodeNickname(BookmarkModel* model,
                     const BookmarkNode* node,
                     const std::string& nickname) {
  VivaldiBookmarkModelFriend::SetNodeMetaInfoWithIndexChange(
      model, node, GetMetaNames().nickname, nickname);
}

void SetNodeDescription(BookmarkModel* model,
                        const BookmarkNode* node,
                        const std::string& description) {
  VivaldiBookmarkModelFriend::SetNodeMetaInfoWithIndexChange(
      model, node, GetMetaNames().description, description);
}

void SetNodeSpeeddial(BookmarkModel* model,
                      const BookmarkNode* node,
                      bool speeddial) {
  // Use Chromium implementation as URL index does not depends on the speeddial
  // status.
  if (speeddial) {
    model->SetNodeMetaInfo(node, GetMetaNames().speeddial,
                           GetMetaNames().true_value);
  } else {
    model->DeleteNodeMetaInfo(node, GetMetaNames().speeddial);
  }
}

void SetNodeThumbnail(BookmarkModel* model,
                      const BookmarkNode* node,
                      const std::string& thumbnail) {
  VivaldiBookmarkModelFriend::SetNodeMetaInfoWithIndexChange(
      model, node, GetMetaNames().thumbnail, thumbnail);
}

void SetNodeThemeColor(BookmarkModel* model,
                       const BookmarkNode* node,
                       SkColor theme_color) {
  model->SetNodeMetaInfo(node, GetMetaNames().theme_color,
                         base::NumberToString(theme_color));
}

bool WriteBookmarkData(const base::Value::Dict& value,
                       BookmarkWriteFunc write_func,
                       BookmarkWriteFunc write_func_att) {
  static const char kNickLabel[] = "\" NICKNAME=\"";
  static const char kDescriptionLabel[] = "\" DESCRIPTION=\"";
  static const char kSpeedDialLabel[] = "\" SPEEDDIAL=\"";

  const base::Value::Dict* meta_info =
      value.FindDict(bookmarks::BookmarkCodec::kMetaInfo);
  if (!meta_info)
    return true;

  const std::string* nick_name = meta_info->FindString(GetMetaNames().nickname);
  const std::string* description =
      meta_info->FindString(GetMetaNames().description);
  const std::string* speed_dial =
      meta_info->FindString(GetMetaNames().speeddial);

  if (nick_name &&
      (!write_func.Run(kNickLabel) || !write_func_att.Run(*nick_name))) {
    return false;
  }

  if (description && (!write_func.Run(kDescriptionLabel) ||
                      !write_func_att.Run(*description))) {
    return false;
  }

  if (speed_dial && *speed_dial == GetMetaNames().true_value &&
      (!write_func.Run(kSpeedDialLabel) ||
       !write_func_att.Run(GetMetaNames().true_value))) {
    return false;
  }

  return true;
}

void ReadBookmarkAttributes(BookmarkAttributeReadFunc GetAttribute,
                            CodePagetoUTF16Func CodePagetoUTF16,
                            std::u16string* nickname,
                            std::u16string* description,
                            bool* is_speeddial_folder) {
  static const char kNickAttrName[] = "NICKNAME";
  static const char kDescriptionAttrName[] = "DESCRIPTION";
  static const char kSpeedDialAttrName[] = "SPEEDDIAL";

  std::string value;
  if (nickname) {
    if (GetAttribute.Run(kNickAttrName, &value)) {
      CodePagetoUTF16(value, nickname);
      *nickname = base::UnescapeForHTML(*nickname);
    }
  }
  if (description) {
    if (GetAttribute.Run(kDescriptionAttrName, &value)) {
      CodePagetoUTF16(value, description);
      *description = base::UnescapeForHTML(*description);
    }
  }
  if (is_speeddial_folder) {
    if (GetAttribute.Run(kSpeedDialAttrName, &value) &&
        base::EqualsCaseInsensitiveASCII(value, "true"))
      *is_speeddial_folder = true;
    else
      *is_speeddial_folder = false;
  }
}

}  // namespace vivaldi_bookmark_kit
