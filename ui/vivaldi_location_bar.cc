// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/vivaldi_location_bar.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

VivaldiLocationBar::VivaldiLocationBar(Profile* profile) {}

VivaldiLocationBar::~VivaldiLocationBar() = default;

GURL VivaldiLocationBar::GetDestinationURL() const {
  return GURL();
}

WindowOpenDisposition VivaldiLocationBar::GetWindowOpenDisposition() const {
  return WindowOpenDisposition::UNKNOWN;
}

ui::PageTransition VivaldiLocationBar::GetPageTransition() const {
  return ui::PageTransition::PAGE_TRANSITION_LINK;
}

const OmniboxView* VivaldiLocationBar::GetOmniboxView() const {
  return nullptr;
}

OmniboxView* VivaldiLocationBar::GetOmniboxView() {
  return nullptr;
}

LocationBarTesting* VivaldiLocationBar::GetLocationBarForTesting() {
  return nullptr;
}

base::TimeTicks VivaldiLocationBar::GetMatchSelectionTimestamp() const {
  return base::TimeTicks();
}
