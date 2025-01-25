// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORTED_TAB_ENTRY_H_
#define IMPORTED_TAB_ENTRY_H_

#include <string>
#include <vector>

#include "url/gurl.h"
#include "base/time/time.h"

namespace sessions {

struct SessionTab;

}  // namespace sessions

struct NavigationEntry {
  std::u16string title;
  GURL url;
  GURL favicon_url;
};

struct ImportedTabEntry {
  ImportedTabEntry();
  ImportedTabEntry(const ImportedTabEntry&);
  ~ImportedTabEntry();

  static ImportedTabEntry FromSessionTab(const sessions::SessionTab& tab);

  // When changing this, also update
  // profile_vivaldi_import_process_param_traits_macros.h
  std::vector<NavigationEntry> navigations;
  bool pinned;
  base::Time timestamp;
  int current_navigation_index;
  // For nonempty decode via TabGroupId::FromRawToken(base::Token::FromString...
  std::string group;
  // In case of Vivaldi import this contains our vivaldi-specific json.
  std::string viv_ext_data;
};

#endif  // IMPORTED_TAB_ENTRY_H_
