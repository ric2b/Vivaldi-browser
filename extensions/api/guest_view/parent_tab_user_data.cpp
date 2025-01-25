// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/guest_view/parent_tab_user_data.h"

namespace vivaldi {

ParentTabUserData::ParentTabUserData(content::WebContents* contents)
    : content::WebContentsUserData<ParentTabUserData>(*contents) {}

ParentTabUserData* ParentTabUserData::GetParentTabUserData(
    content::WebContents* contents) {
  if (!contents)
    return nullptr;

  ParentTabUserData* parent_tab = FromWebContents(contents);
  if (parent_tab)
    return parent_tab;

  CreateForWebContents(contents);
  return FromWebContents(contents);
}

std::optional<int> ParentTabUserData::GetParentTabId(
    content::WebContents* contents) {
  if (!contents)
    return std::nullopt;

  auto* parent_tab_data = FromWebContents(contents);
  if (!parent_tab_data)
    return std::nullopt;
  return parent_tab_data->GetParentTabId();
}

bool ParentTabUserData::ShouldSync(content::WebContents* contents) {
  auto parent = GetParentTabId(contents);
  // Sync regular tabs.
  if (!parent)
    return true;
  // Sync panels.
  if (*parent == 0)
    return true;
  // Widgets don't!
  return false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ParentTabUserData);
}  // namespace vivaldi
