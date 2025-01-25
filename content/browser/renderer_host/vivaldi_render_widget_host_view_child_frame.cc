// Copyright 2024 Vivaldi Technologies. All rights reserved.

#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"

#include "app/vivaldi_apptools.h"

namespace content {

// VB-107749 [macOS] Scrolling on google sheets triggers history swipe
void RenderWidgetHostViewChildFrame::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  if (!vivaldi::IsVivaldiRunning()) {
    return;
  }

  RenderWidgetHostViewBase* parent_view = GetParentViewInput();
  if (parent_view && !parent_view->IsRenderWidgetHostViewChildFrame()) {
    parent_view->DidOverscroll(params);
  }
}

}  // namespace content
