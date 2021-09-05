// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/vivaldi_location_bar.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/content_settings/browser/tab_specific_content_settings.h"
#include "extensions/api/tabs/tabs_private_api.h"

using content_settings::TabSpecificContentSettings;

VivaldiLocationBar::VivaldiLocationBar() {
}

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

void VivaldiLocationBar::UpdateContentSettingsIcons() {
  // Fetch tab specific content settings for current tab and make events based
  // on that to display to the user.

  content::WebContents* active_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (active_contents) {
    auto* content_settings =
        TabSpecificContentSettings::FromWebContents(active_contents);
    extensions::VivaldiPrivateTabObserver* private_tab =
        extensions::VivaldiPrivateTabObserver::FromWebContents(active_contents);
    if (private_tab && content_settings) {
      TabSpecificContentSettings::MicrophoneCameraState cam_state =
          content_settings->GetMicrophoneCameraState();
      bool microphoneAccessed =
          (cam_state & TabSpecificContentSettings::MICROPHONE_ACCESSED) != 0;
      bool cameraAccessed =
          (cam_state & TabSpecificContentSettings::CAMERA_ACCESSED) != 0;
      bool microphoneBlocked =
          (cam_state & TabSpecificContentSettings::MICROPHONE_BLOCKED) != 0;
      bool cameraBlocked =
          (cam_state & TabSpecificContentSettings::CAMERA_BLOCKED) != 0;

      if (microphoneAccessed || cameraAccessed || microphoneBlocked ||
          cameraBlocked) {
        ContentSetting setting = CONTENT_SETTING_DEFAULT;
        GURL reqURL = active_contents->GetURL();

        if (microphoneAccessed || microphoneBlocked) {
          setting =
              microphoneBlocked ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW;
          private_tab->OnPermissionAccessed(
              ContentSettingsType::MEDIASTREAM_MIC, reqURL.spec(), setting);
        }

        if (cameraAccessed || cameraBlocked) {
          setting =
              cameraBlocked ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW;
          private_tab->OnPermissionAccessed(
              ContentSettingsType::MEDIASTREAM_CAMERA, reqURL.spec(), setting);
        }
      }
    }
  }
}
