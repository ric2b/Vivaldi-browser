// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/bookmarks/vivaldi_bookmark_kit.h"

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "ui/base/models/tree_node_iterator.h"

namespace vivaldi_bookmark_kit {

namespace {

// Struct holding std::string constants with names that we use to get/set values
// from bookmark meta data. This avoids running string destructors at shutdown.
struct VivaldiMetaNames {
  // meta data keys
  const std::string speeddial = "Speeddial";
  const std::string bookmarkbar = "Bookmarkbar";
  const std::string nickname = "Nickname";
  const std::string description = "Description";
  const std::string partner = "Partner";
  const std::string default_favicon_uri = "Default_Favicon_URI";
  const std::string thumbnail = "Thumbnail";
  const std::string visited = "Visited";

  // values stored in the node
  const std::string true_value = "true";
  const base::string16 tripple_dash = base::UTF8ToUTF16("---");
  const std::string separator = "separator";
};

const VivaldiMetaNames& GetMetaNames() {
  static base::NoDestructor<VivaldiMetaNames> instance;
  return *instance;
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

void SetMetaTime(BookmarkNode::MetaInfoMap* map,
                 const std::string& key,
                 base::Time time_value) {
  if (!time_value.is_null()) {
    (*map)[key] = base::NumberToString(time_value.ToInternalValue());
  } else {
    map->erase(key);
  }
}

}  // namespace

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

void CustomMetaInfo::SetPartner(const std::string& partner) {
  SetMetaString(&map_, GetMetaNames().partner, partner);
}

void CustomMetaInfo::SetDefaultFaviconUri(
    const std::string& default_favicon_uri) {
  SetMetaString(&map_, GetMetaNames().default_favicon_uri, default_favicon_uri);
}

void CustomMetaInfo::SetVisitedDate(base::Time visited) {
  SetMetaTime(&map_, GetMetaNames().visited, visited);
}

void CustomMetaInfo::SetThumbnail(const std::string& thumbnail) {
  SetMetaString(&map_, GetMetaNames().thumbnail, thumbnail);
}

void InitModelNonClonedKeys(BookmarkModel* model) {
  model->AddNonClonedKey(GetMetaNames().nickname);
  model->AddNonClonedKey(GetMetaNames().partner);
  model->AddNonClonedKey(GetMetaNames().speeddial);
  model->AddNonClonedKey(GetMetaNames().bookmarkbar);
}

}  // namespace vivaldi_bookmark_kit