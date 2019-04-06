// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/bookmarks/browser/bookmark_node.h"

#include <map>
#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

#include "base/strings/string_number_conversions.h"

namespace bookmarks {

// BookmarkNode ---------------------------------------------------------------

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

}  // namespace bookmarks
