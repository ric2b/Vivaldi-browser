//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#include "chrome/browser/ui/tabs/tab_model.h"
#include "components/extensions/vivaldi_panel_utils.h"

namespace tabs {

bool TabModel::is_viv_panel() const {
  if (vivaldi::GetVivPanelId(contents_.get())) {
    return true;
  }
  return false;
}

}  // namespace tabs
