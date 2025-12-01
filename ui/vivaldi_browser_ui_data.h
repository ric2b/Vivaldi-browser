//
// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved.
//
#ifndef UI_VIVALDI_BROWSER_UI_DATA_H_
#define UI_VIVALDI_BROWSER_UI_DATA_H_

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace vivaldi {

class VivaldiBrowserUiData {
 public:
  DECLARE_USER_DATA(VivaldiBrowserUiData);

  VivaldiBrowserUiData(BrowserWindowInterface* browser);

  static VivaldiBrowserUiData* From(
      BrowserWindowInterface* browser_window_interface);

  const std::string& viv_ext_data() const { return viv_ext_data_; }
  void set_viv_ext_data(const std::string& viv_ext_data);

  bool is_vivaldi() {
    /*this exsistance is enough*/
    return true;
  }

 private:
  // Additional data/properties of the browser/window. JSON.
  std::string viv_ext_data_;
  const raw_ref<BrowserWindowInterface> browser_interface_;
  ui::ScopedUnownedUserData<VivaldiBrowserUiData> scoped_data_holder_;
};

}  // namespace vivaldi

#endif  // UI_VIVALDI_BROWSER_UI_DATA_H_
