// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

namespace blink {

void WebLocalFrameImpl::SetCosmeticFilterClient(
    WebCosmeticFilterClient* client) {
  web_cosmetic_filter_client_ = client;
}

WebCosmeticFilterClient* WebLocalFrameImpl::GetCosmeticFilterClient() {
  return web_cosmetic_filter_client_;
}

}  // namespace blink
