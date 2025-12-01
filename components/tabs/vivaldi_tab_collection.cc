// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/tabs/vivaldi_tab_collection.h"

#include <memory>
#include <optional>

#include "app/vivaldi_apptools.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

VivaldiTabCollection::VivaldiTabCollection()
    : TabCollection(TabCollection::Type::VIVALDI,
                    {TabCollection::Type::UNPINNED},
                    /*supports_tabs=*/true) {}

VivaldiTabCollection::~VivaldiTabCollection() = default;

size_t TabCollection::VivaldiTabCountRecursive() const {
  if (!::vivaldi::IsVivaldiRunning()) {
    return TabCountRecursive();
  }
  // Same as TabCountRecursive(), but skips the panels.
  size_t real_count = 0;
  for (const auto& child : GetChildren()) {
    if (std::holds_alternative<std::unique_ptr<tabs::TabInterface>>(child)) {
      real_count++;
    } else if (std::holds_alternative<std::unique_ptr<tabs::TabCollection>>(child)) {
      auto &ptr = std::get<std::unique_ptr<tabs::TabCollection>>(child);
      if (ptr->type() == TabCollection::Type::VIVALDI)
        continue;
      real_count += ptr->TabCountRecursive();
    }
  }
  return real_count;
}
}  // namespace tabs
