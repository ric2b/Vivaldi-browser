// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#include "content/browser/renderer_host/vivaldi_renderer_host_util.h"

#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "ui/gfx/geometry/point.h"

using content::FrameTree;
using content::FrameTreeNode;

namespace vivaldi {

gfx::Point GetVivaldiUIOffset(
    content::RenderWidgetHostImpl* parent_host,
    content::RenderWidgetHostImpl* child_host,
    float device_scale_factor) {
  FrameTree* tree = child_host->frame_tree();
  FrameTreeNode* focused_node = tree->GetFocusedFrame();
  content::RenderFrameHostImpl* child =
      focused_node ? focused_node->current_frame_host() : nullptr;

  FrameTreeNode* window_root_node = parent_host->frame_tree()->root();
  content::RenderFrameHostImpl* parent =
      window_root_node ? window_root_node->current_frame_host() : nullptr;

  if (child && parent) {
    int xOffset = child->AccessibilityGetViewBounds().origin().x() -
                  parent->AccessibilityGetViewBounds().origin().x();
    int yOffset = child->AccessibilityGetViewBounds().origin().y() -
                  parent->AccessibilityGetViewBounds().origin().y();

    gfx::Point vivaldi_offset = gfx::ScaleToRoundedPoint(
        gfx::Point(xOffset, yOffset), 1.f-device_scale_factor);
    return vivaldi_offset;
  }
  return gfx::Point(0, 0);
}


}  // namespace vivaldi
