//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#include "chrome/browser/ui/tabs/tab_model.h"
#include "components/extensions/vivaldi_panel_utils.h"

namespace tabs {
bool TabModel::IsVivaldiPanel() const {
  if (!is_vivaldi_panel_cache_) {
    is_vivaldi_panel_cache_ = !!vivaldi::GetVivPanelId(contents_.get());
  }

  return *is_vivaldi_panel_cache_;
}
}  // namespace tabs
