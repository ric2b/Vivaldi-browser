// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.

#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"

namespace content {

void RenderWidgetHostViewChildFrame::PassSurfaceCopyRequest(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  support_->RequestCopyOfOutput(viz::LocalSurfaceId(), std::move(request));
}

bool RenderWidgetHostDelegate::HasFocusedGuests() {
  return false;
}

bool RenderWidgetHostViewBase::IsAura() const {
  return false;
}

}  // namespace content
