// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "content/browser/renderer_host/render_frame_host_impl.h"

#include "content/browser/renderer_host/frame_tree_node.h"

namespace content {

void RenderFrameHostImpl::DidChangeLoadProgressExtended(double load_progress,
                                                        double loaded_bytes,
                                                        int loaded_elements,
                                                        int total_elements) {
  frame_tree_node_->DidChangeLoadProgressExtended(
      load_progress, loaded_bytes, loaded_elements, total_elements);
}

}  // namespace content
