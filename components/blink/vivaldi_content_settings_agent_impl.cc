// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.
/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "components/content_settings/renderer/content_settings_agent_impl.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content_settings {

#if defined(VIVALDI_BUILD)
bool ContentSettingsAgentImpl::AllowAutoplay(bool play_requested) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  auto origin = frame->GetSecurityOrigin();
  if (origin.IsNull()) {
    // No origin, allow it.
    return true;
  }
  if (origin.Protocol().Ascii() == url::kFileScheme) {
    // Allow local files as default.
    return true;
  }
  // User site blocklist.
  if (content_setting_rules_) {
    ContentSetting setting = VivaldiGetContentSettingFromRules(
        content_setting_rules_->autoplay_rules, frame,
        url::Origin(origin).GetURL());
    if (setting == CONTENT_SETTING_BLOCK) {
      if (play_requested) {
        DidBlockContentType(ContentSettingsType::AUTOPLAY);
      }
      return false;
    } else if (setting == CONTENT_SETTING_ALLOW) {
      return true;
    }
  }
  return true;
}
#endif  // defined(VIVALDI_BUILD)

}  // namespace content_settings
