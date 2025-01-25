// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/vivaldi_location_bar.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "ui/vivaldi_browser_window.h"

using content_settings::PageSpecificContentSettings;

VivaldiLocationBar::VivaldiLocationBar(VivaldiBrowserWindow& window)
    : LocationBar(window.browser()->command_controller()), window_(window) {}

VivaldiLocationBar::~VivaldiLocationBar() = default;

OmniboxView* VivaldiLocationBar::GetOmniboxView() {
  return nullptr;
}

content::WebContents* VivaldiLocationBar::GetWebContents() { return nullptr; }

LocationBarModel* VivaldiLocationBar::GetLocationBarModel() { return nullptr; }


LocationBarTesting* VivaldiLocationBar::GetLocationBarForTesting() {
  return nullptr;
}

void VivaldiLocationBar::UpdateContentSettingsIcons() {
  // Fetch tab specific content settings for current tab and make events based
  // on that to display to the user.

  content::WebContents* active_contents =
      window_->browser()->tab_strip_model()->GetActiveWebContents();
  if (active_contents) {
    auto* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            active_contents->GetPrimaryMainFrame());
    extensions::VivaldiPrivateTabObserver* private_tab =
        extensions::VivaldiPrivateTabObserver::FromWebContents(active_contents);
    if (private_tab && content_settings) {
      PageSpecificContentSettings::MicrophoneCameraState cam_state =
          content_settings->GetMicrophoneCameraState();
      bool microphoneAccessed =
          cam_state.Has(PageSpecificContentSettings::kMicrophoneAccessed);
      bool cameraAccessed =
          cam_state.Has(PageSpecificContentSettings::kCameraAccessed);
      bool microphoneBlocked =
          cam_state.Has(PageSpecificContentSettings::kMicrophoneBlocked);
      bool cameraBlocked =
          cam_state.Has(PageSpecificContentSettings::kCameraBlocked);

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
