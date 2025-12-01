// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/tabs/vivaldi_tab_collection.h"

#include <memory>
#include <optional>

#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

VivaldiTabCollection::VivaldiTabCollection()
    : TabCollection(TabCollection::Type::VIVALDI,
                    {TabCollection::Type::UNPINNED},
                    /*supports_tabs=*/true) {}

VivaldiTabCollection::~VivaldiTabCollection() = default;


}  // namespace tabs
