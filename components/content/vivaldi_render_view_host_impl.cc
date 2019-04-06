// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "content/browser/renderer_host/render_view_host_impl.h"

#include "content/common/view_messages.h"

namespace content {

void RenderViewHostImpl::LoadImageAt(int x, int y) {
  Send(new ViewMsg_LoadImageAt(GetRoutingID(), x, y));
}

}  // namespace content
