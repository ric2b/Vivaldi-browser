// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "components/tabs/public/unpinned_tab_collection.h"

namespace tabs {

bool TabStripCollection::IsMovable(int index) const {
  // The tabs in the vivaldi collection are not movable since they
  // are the panels and they don't move in the tab strip.
  // Since the vivaldi collection is the last in the tab strip,
  // the tabs before are movable.
  return index < static_cast<int>(IndexOfFirstVivaldiTab());
}

// Are all the tab indices movable tabs?
bool TabStripCollection::AreMovable(const std::vector<int>& tab_indices) const {
  if (tab_indices.empty()) {
    return false;
  }
  for (auto index: tab_indices) {
    if (!IsMovable(index)) {
      return false;
    }
  }
  return true;
}

// This is how are te tab collections placed:
// [...pinned tabs...][...unpined tabs...][...vivaldi tabs...]
//
// It is obvious how to calculate the index of the first vivaldi tab.
size_t TabStripCollection::IndexOfFirstVivaldiTab() const {
  return pinned_collection_->TabCountRecursive() +
         unpinned_collection_->TabCountRecursive();
}

} // namespace tabs
