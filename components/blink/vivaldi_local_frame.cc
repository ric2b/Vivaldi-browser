// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"

namespace blink {

void LocalFrame::VisibleTextSelectionChanged(
    const WTF::String& selection_text) const {
  GetLocalFrameHostRemote().VisibleTextSelectionChanged(selection_text);
}

void WebLocalFrameImpl::VisibleTextSelectionChanged(
    const WebString& selection_text) {
  GetFrame()->VisibleTextSelectionChanged(selection_text);
}

} // namespace blink