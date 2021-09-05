// Copyright (c) 2018-2020 Vivaldi Technologies AS. All rights reserved

#include "content/browser/renderer_host/render_view_host_impl.h"

namespace content {

void RenderViewHostImpl::LoadImageAt(int x, int y) {
  GetWidget()->GetAssociatedFrameWidget()->LoadImageAt(gfx::Point(x, y));
}

}  // namespace content
