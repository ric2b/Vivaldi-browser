// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.
/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"

#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"

namespace blink {

/* static */
bool AutoplayPolicy::IsAutoplayAllowedForFrame(blink::LocalFrame* frame,
                                               bool play_requested) {
  if (!frame)
    return false;
  if (auto* settings_client = frame->GetContentSettingsClient()) {
    return settings_client->AllowAutoplay(play_requested);
  }
  return true;
}

/* static */
bool AutoplayPolicy::IsAutoplayAllowedForDocument(const Document& document) {
  return IsAutoplayAllowedForFrame(document.GetFrame(), false);
}

/* static */
bool AutoplayPolicy::IsAutoplayAllowedForElement(
    Member<HTMLMediaElement> element) {
  return IsAutoplayAllowedForFrame(element->GetDocument().GetFrame(), true);
}

}  // namespace blink
