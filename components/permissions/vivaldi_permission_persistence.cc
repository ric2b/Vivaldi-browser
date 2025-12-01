// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/permissions/vivaldi_permission_persistence.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/helper/vivaldi_app_helper.h"
#include "extensions/vivaldi_browser_component_wrapper.h"

namespace vivaldi {

void PersistGeolocationPermission(extensions::WebViewGuest* web_view_guest,
                                  bool allow) {
  extensions::VivaldiAppHelper* helper =
      extensions::VivaldiAppHelper::FromWebContents(
          web_view_guest->embedder_web_contents());
  if (helper) {
    GURL requesting_url = web_view_guest->web_contents()->GetLastCommittedURL();
    ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromURLNoWildcard(requesting_url);

    VivaldiBrowserComponentWrapper::GetInstance()->SetContentSettingCustomScope(
        web_view_guest->web_contents(), allow, primary_pattern,
        ContentSettingsPattern::Wildcard(), ContentSettingsType::GEOLOCATION,
        allow ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
  }
}

}  // namespace vivaldi
