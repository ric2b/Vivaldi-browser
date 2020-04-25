// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/bookmarks/browser/bookmark_node.h"

#include <map>
#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

#include "base/strings/string_number_conversions.h"

namespace bookmarks {

// BookmarkNode ---------------------------------------------------------------

const char BookmarkNode::kTrashNodeGuid[] =
    "00000000-0000-4000-A000-000000040000";

const base::Time BookmarkNode::date_visited() const {
  std::string date;
  int64_t date_val=0;

  if (!GetMetaInfo("Visited", &date))
	return base::Time();

  if (!base::StringToInt64(date, &date_val))
	return base::Time();

  return base::Time(base::Time::FromInternalValue(date_val));
}

void BookmarkNode::set_nickname(const base::string16 &nick) {
  SetMetaInfo("Nickname", base::UTF16ToUTF8(nick));
}

base::string16 BookmarkNode::GetThumbnail() const {
  std::string temp;
  if (GetMetaInfo("Thumbnail",&temp))
    return base::UTF8ToUTF16(temp);

  return base::string16();
}

base::string16 BookmarkNode::GetPartner() const {
  std::string temp;
  if (GetMetaInfo("Partner", &temp))
    return base::UTF8ToUTF16(temp);

  return base::string16();
}

// Note: This only works for folders
bool BookmarkNode::GetSpeeddial() const {
  std::string temp;
  if (GetMetaInfo("Speeddial",&temp))
    return temp == "true";

  return false;
}

bool BookmarkNode::GetBookmarkbar() const {
  std::string temp;
  if (GetMetaInfo("Bookmarkbar",&temp))
    return temp == "true";

  return false;
}

base::string16 BookmarkNode::GetNickName() const {
  std::string temp;
  if (GetMetaInfo("Nickname",&temp))
    return base::UTF8ToUTF16(temp);

  return base::string16();
}

void BookmarkNode::set_description(const base::string16 &desc) {
  SetMetaInfo("Description", base::UTF16ToUTF8(desc));
}

base::string16 BookmarkNode::GetDescription() const {
  std::string temp;
  if (GetMetaInfo("Description",&temp))
    return base::UTF8ToUTF16(temp);

  return base::string16();
}

base::string16 BookmarkNode::GetDefaultFaviconUri() const {
  std::string temp;
  if (GetMetaInfo("Default_Favicon_URI",&temp))
    return base::UTF8ToUTF16(temp);

  return base::string16();
}

const base::string16 BookmarkNode::GetTitledUrlNodeNickName() const {
  return GetNickName();
}

const base::string16 BookmarkNode::GetTitledUrlNodeDescription() const {
  return GetDescription();
}

bool BookmarkNode::is_separator() const {
  const static base::string16 desc = base::ASCIIToUTF16("separator");
  return GetDescription().compare(desc) == 0;
}


}  // namespace bookmarks
