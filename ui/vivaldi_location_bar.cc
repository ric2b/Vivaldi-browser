// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/vivaldi_location_bar.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

VivaldiLocationBar::VivaldiLocationBar(Profile* profile, Browser* browser)
    : LocationBar(profile), browser_(browser) {
  icon_view_.reset(new VivaldiManagePasswordsIconView(profile));
}

VivaldiLocationBar::~VivaldiLocationBar() { }

GURL VivaldiLocationBar::GetDestinationURL() const {
  return GURL();
};

WindowOpenDisposition VivaldiLocationBar::GetWindowOpenDisposition() const {
  return WindowOpenDisposition::UNKNOWN;
};

ui::PageTransition VivaldiLocationBar::GetPageTransition() const {
  return ui::PageTransition::PAGE_TRANSITION_LINK;
};

const OmniboxView* VivaldiLocationBar::GetOmniboxView() const {
  return nullptr;
}

OmniboxView* VivaldiLocationBar::GetOmniboxView() {
  return nullptr;
}

LocationBarTesting* VivaldiLocationBar::GetLocationBarForTesting() {
  return nullptr;
}

void VivaldiLocationBar::UpdateManagePasswordsIconAndBubble() {
  content::WebContents* contents =
    browser_->tab_strip_model()->GetActiveWebContents();
  if (!contents)
    return;

  ManagePasswordsUIController::FromWebContents(contents)
      ->UpdateIconAndBubbleState(icon_view_.get());
}

VivaldiLocationBar::VivaldiManagePasswordsIconView::
    VivaldiManagePasswordsIconView(Profile* profile)
    : profile_(profile) {}

void VivaldiLocationBar::VivaldiManagePasswordsIconView::SetState(
  password_manager::ui::State state) {
  DCHECK(profile_);
  extensions::VivaldiUtilitiesAPI* utils_api =
        extensions::VivaldiUtilitiesAPI::GetFactoryInstance()->Get(
          profile_);
  bool show =  state == password_manager::ui::State::PENDING_PASSWORD_STATE;
  show = state != password_manager::ui::State::INACTIVE_STATE;
  utils_api->OnPasswordIconStatusChanged(show);
}
