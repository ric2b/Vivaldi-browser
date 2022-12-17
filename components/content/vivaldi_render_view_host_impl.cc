// Copyright (c) 2018-2020 Vivaldi Technologies AS. All rights reserved

#include "content/browser/renderer_host/render_view_host_impl.h"

namespace content {

void RenderViewHostImpl::LoadImageAt(int x, int y) {
  GetWidget()->GetAssociatedFrameWidget()->LoadImageAt(gfx::Point(x, y));
}

void RenderViewHostImpl::SetImagesEnabled(const bool show_images) {
  if (GetWidget()->GetAssociatedFrameWidget()) {
    GetWidget()->GetAssociatedFrameWidget()->SetImagesEnabled(show_images);
  }
}

void RenderViewHostImpl::SetServeResourceFromCacheOnly(const bool only_cache) {
  if (GetWidget()->GetAssociatedFrameWidget()) {
    GetWidget()->GetAssociatedFrameWidget()->SetServeResourceFromCacheOnly(only_cache);
  }
}

}  // namespace content
