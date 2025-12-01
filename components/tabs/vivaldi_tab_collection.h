// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_TABS_VIVALDI_TAB_COLLECTION_H_
#define COMPONENTS_TABS_VIVALDI_TAB_COLLECTION_H_

#include <optional>

#include "components/tabs/public/tab_collection.h"

namespace tabs {

class TabInterface;
class TabGroupTabCollection;

class VivaldiTabCollection : public TabCollection {
 public:
  VivaldiTabCollection();
  ~VivaldiTabCollection() override;
  VivaldiTabCollection(const VivaldiTabCollection&) = delete;
  VivaldiTabCollection& operator=(const VivaldiTabCollection&) = delete;
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_VIVALDI_TAB_COLLECTION_H_
