// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node.h"

#include <map>
#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

#include "base/strings/string_number_conversions.h"

namespace bookmarks {

namespace {

// Whitespace characters to strip from bookmark titles.
const base::char16 kInvalidChars[] = {
  '\n', '\r', '\t',
  0x2028,  // Line separator
  0x2029,  // Paragraph separator
  0
};

}  // namespace

// BookmarkNode ---------------------------------------------------------------

const int64_t BookmarkNode::kInvalidSyncTransactionVersion = -1;

BookmarkNode::BookmarkNode(const GURL& url)
    : url_(url) {
  Initialize(0);
}

BookmarkNode::BookmarkNode(int64_t id, const GURL& url) : url_(url) {
  Initialize(id);
}

BookmarkNode::~BookmarkNode() {
}

void BookmarkNode::SetTitle(const base::string16& title) {
  // Replace newlines and other problematic whitespace characters in
  // folder/bookmark names with spaces.
  base::string16 trimmed_title;
  base::ReplaceChars(title, kInvalidChars, base::ASCIIToUTF16(" "),
                     &trimmed_title);
  ui::TreeNode<BookmarkNode>::SetTitle(trimmed_title);
}

bool BookmarkNode::IsVisible() const {
  return true;
}

bool BookmarkNode::GetMetaInfo(const std::string& key,
                               std::string* value) const {
  if (!meta_info_map_)
    return false;

  MetaInfoMap::const_iterator it = meta_info_map_->find(key);
  if (it == meta_info_map_->end())
    return false;

  *value = it->second;
  return true;
}

const base::Time BookmarkNode::date_visited() const
{
  std::string date;
  int64_t date_val=0;

  if (!GetMetaInfo("Visited", &date))
	return base::Time();

  if (!base::StringToInt64(date, &date_val))
	return base::Time();

  return base::Time(base::Time::FromInternalValue(date_val));
}

void BookmarkNode::set_date_visited(const base::Time& date)
{
  if (!date.is_null())
    SetMetaInfo("Visited", base::Int64ToString(date.ToInternalValue()));
}

void BookmarkNode::set_nickname(const base::string16 &nick)
{
  SetMetaInfo("Nickname", base::UTF16ToUTF8(nick));
}

void BookmarkNode::set_thumbnail(const base::string16 &thumbnail)
{
  SetMetaInfo("Thumbnail", base::UTF16ToUTF8(thumbnail));
}

base::string16 BookmarkNode::GetThumbnail() const
{
  std::string temp;
  if (GetMetaInfo("Thumbnail",&temp))
    return base::UTF8ToUTF16(temp);

  return base::string16();
}

void BookmarkNode::set_speeddial(bool speeddial)
{
  std::string temp;

  temp = speeddial ? "true" : "false";

  SetMetaInfo("Speeddial", temp);
}

bool BookmarkNode::GetSpeeddial() const
{
  std::string temp;
  if (GetMetaInfo("Speeddial",&temp))
    return temp == "true";

  return false;
}

base::string16 BookmarkNode::GetNickName() const
{
  std::string temp;
  if (GetMetaInfo("Nickname",&temp))
    return base::UTF8ToUTF16(temp);

  return base::string16();
}

void BookmarkNode::set_description(const base::string16 &desc)
{
  SetMetaInfo("Description", base::UTF16ToUTF8(desc));
}

base::string16 BookmarkNode::GetDescription() const
{
  std::string temp;
  if (GetMetaInfo("Description",&temp))
    return base::UTF8ToUTF16(temp);

  return base::string16();
}

bool BookmarkNode::SetMetaInfo(const std::string& key,
                               const std::string& value) {
  if (!meta_info_map_)
    meta_info_map_.reset(new MetaInfoMap);

  MetaInfoMap::iterator it = meta_info_map_->find(key);
  if (it == meta_info_map_->end()) {
    (*meta_info_map_)[key] = value;
    return true;
  }
  // Key already in map, check if the value has changed.
  if (it->second == value)
    return false;
  it->second = value;
  return true;
}

bool BookmarkNode::DeleteMetaInfo(const std::string& key) {
  if (!meta_info_map_)
    return false;
  bool erased = meta_info_map_->erase(key) != 0;
  if (meta_info_map_->empty())
    meta_info_map_.reset();
  return erased;
}

void BookmarkNode::SetMetaInfoMap(const MetaInfoMap& meta_info_map) {
  if (meta_info_map.empty())
    meta_info_map_.reset();
  else
    meta_info_map_.reset(new MetaInfoMap(meta_info_map));
}

const BookmarkNode::MetaInfoMap* BookmarkNode::GetMetaInfoMap() const {
  return meta_info_map_.get();
}

const base::string16& BookmarkNode::GetTitledUrlNodeTitle() const {
  return GetTitle();
}

const GURL& BookmarkNode::GetTitledUrlNodeUrl() const {
  return url_;
}

void BookmarkNode::Initialize(int64_t id) {
  id_ = id;
  type_ = url_.is_empty() ? FOLDER : URL;
  date_added_ = base::Time::Now();
  favicon_type_ = favicon_base::INVALID_ICON;
  favicon_state_ = INVALID_FAVICON;
  favicon_load_task_id_ = base::CancelableTaskTracker::kBadTaskId;
  meta_info_map_.reset();
  sync_transaction_version_ = kInvalidSyncTransactionVersion;
}

void BookmarkNode::InvalidateFavicon() {
  icon_url_ = GURL();
  favicon_ = gfx::Image();
  favicon_type_ = favicon_base::INVALID_ICON;
  favicon_state_ = INVALID_FAVICON;
}

// BookmarkPermanentNode -------------------------------------------------------

BookmarkPermanentNode::BookmarkPermanentNode(int64_t id)
    : BookmarkNode(id, GURL()), visible_(true) {}

BookmarkPermanentNode::~BookmarkPermanentNode() {
}

bool BookmarkPermanentNode::IsVisible() const {
  return visible_ || !empty();
}

}  // namespace bookmarks
