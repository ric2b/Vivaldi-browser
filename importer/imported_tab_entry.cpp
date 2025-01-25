// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include <algorithm>

#include "importer/imported_tab_entry.h"

#include "chromium/components/sessions/core/session_types.h"

ImportedTabEntry::ImportedTabEntry() = default;
ImportedTabEntry::ImportedTabEntry(const ImportedTabEntry&) = default;
ImportedTabEntry::~ImportedTabEntry() = default;

ImportedTabEntry ImportedTabEntry::FromSessionTab(
    const sessions::SessionTab& tab) {
  ImportedTabEntry imported;

  imported.timestamp = tab.timestamp;
  imported.pinned = tab.pinned;
  imported.current_navigation_index = tab.current_navigation_index;

  if (tab.group) {
    imported.group = tab.group->ToString();
  }

  std::transform(tab.navigations.begin(), tab.navigations.end(),
                 std::back_inserter(imported.navigations), [](const auto& it) {
                   NavigationEntry inav;
                   inav.url = it.virtual_url();
                   inav.favicon_url = it.favicon_url();
                   inav.title = it.title();
                   return inav;
                 });

  imported.viv_ext_data = tab.viv_ext_data;

  return imported;
}
