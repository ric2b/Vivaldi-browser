//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#include "chrome/browser/ui/tabs/tab_model.h"
#include "components/extensions/vivaldi_panel_utils.h"

namespace tabs {

void TabModel::UpdateVivPanel() {
  viv_panel_ = !!vivaldi::GetVivPanelId(contents_.get());
}

}  // namespace tabs
