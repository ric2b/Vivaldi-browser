// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "third_party/blink/renderer/core/exported/local_frame_client_impl.h"

#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

namespace blink {

// VB-6063:
void LocalFrameClientImpl::extendedProgressEstimateChanged(
    double progressEstimate,
    double loaded_bytes,
    int loaded_elements,
    int total_elements) {
  if (web_frame_->Client())
    web_frame_->Client()->didChangeLoadProgress(
        progressEstimate, loaded_bytes, loaded_elements, total_elements);
}

}  // namespace blink
