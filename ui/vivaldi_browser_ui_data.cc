//
// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved.
//

#include "ui/vivaldi_browser_ui_data.h"

#include "base/check_deref.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"

namespace vivaldi {

DEFINE_USER_DATA(VivaldiBrowserUiData);

VivaldiBrowserUiData::VivaldiBrowserUiData(
    BrowserWindowInterface* browser)
    : browser_interface_(CHECK_DEREF(browser)),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {
  Browser* typed_browser = static_cast<Browser*>(browser);
  if (!typed_browser->create_params().viv_ext_data.empty()) {
    set_viv_ext_data(typed_browser->create_params().viv_ext_data);
  }
}

/*static*/
VivaldiBrowserUiData* VivaldiBrowserUiData::From(
    BrowserWindowInterface* browser_window_interface) {
  return ui::ScopedUnownedUserData<VivaldiBrowserUiData>::Get(
      browser_window_interface->GetUnownedUserDataHost());
}

void VivaldiBrowserUiData::set_viv_ext_data(const std::string& viv_ext_data) {
  viv_ext_data_ = viv_ext_data;

  SessionService* session_service =
      SessionServiceFactory::GetForProfile(browser_interface_->GetProfile());
  if (session_service) {
    session_service->SetWindowVivExtData(browser_interface_->GetSessionID(),
                                         viv_ext_data_);
  }
}

} // namespace vivaldi
