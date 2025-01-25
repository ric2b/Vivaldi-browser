//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#include "chrome/browser/ui/tabs/tab_strip_model.h"

bool TabStripModel::IsVivPanel(int index) const {
  CHECK(ContainsIndex(index)) << index;
  return GetTabAtIndex(index)->is_viv_panel();
}

int TabStripModel::ConstrainVivaldiMoveIndex(int index,
                                             bool is_viv_panel) const {
  CHECK(index >= 0);
  if (is_viv_panel) {
    // We avoid of moving the panels. However, if it happens we
    // should always place the panel at the end of the tab-strip. Since we are
    // MOVING, the last possible index remains count() - 1.
    return std::max(count() - 1, 0);
  }

  // Find the first panel.
  for (int i = 0; i < count(); ++i) {
    if (IsVivPanel(i)) {
      // The tab can't be placed among the panels. Also, since by the move
      // operation the number of the tabs remains unchanged, the last possible
      // index for the "non-panel" tab is the index of the last panel - 1.
      return std::min(index, std::max(i - 1, 0));
    }
  }

  return index;
}

int TabStripModel::ConstrainVivaldiInsertionIndex(int index,
                                                  bool is_viv_panel) const {
  CHECK(index >= 0);
  if (is_viv_panel) {
    // Always place the panel at the end of the tab-strip.
    // By adding a new tab we increase the number of tabs by 1, so the highest
    // possible target index is equal to the current number of the tabs =
    // count().
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
