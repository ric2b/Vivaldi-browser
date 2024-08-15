// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved.

#include "third_party/blink/public/web/modules/mediastream/web_media_player_ms.h"

#include "third_party/blink/renderer/modules/mediastream/media_stream_local_frame_wrapper.h"

namespace blink {

WebLocalFrame* WebMediaPlayerMS::VivaldiGetOwnerWebFrame() {
  return internal_frame_->web_frame();
}

}  // namespace blink
