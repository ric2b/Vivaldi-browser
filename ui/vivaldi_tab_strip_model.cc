//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#include "chrome/browser/ui/tabs/tab_strip_model.h"

bool TabStripModel::IsVivPanel(int index) const {
  CHECK(ContainsIndex(index)) << index;
  return GetTabAtIndex(index)->is_viv_panel();
}

int TabStripModel::ConstrainVivaldiIndex(int index, bool is_viv_panel) const {
  CHECK(index >= 0);
  if (is_viv_panel) {
    // Always put the panels at the end.
    return count();
  }

  // Find the first panel.
  for (int i = 0; i < count(); ++i) {
    if (IsVivPanel(i)) {
      // The tab can't be placed among the panels.
      return std::min(index, std::max(i, 0));
    }
  }

  return index;
}
