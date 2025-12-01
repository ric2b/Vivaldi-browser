//
// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved.
//

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_strip_collection.h"

bool TabStripModel::IsMovable(int index) const {
  return contents_data_->IsMovable(index);
}
