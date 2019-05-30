// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "content/browser/frame_host/render_frame_host_impl.h"

#include "content/browser/frame_host/frame_tree_node.h"
#include "content/common/frame_messages.h"

namespace content {

void RenderFrameHostImpl::OnDidChangeLoadProgressExtended(
    const FrameMsg_ExtendedLoadingProgress_Params& params) {
  frame_tree_node_->DidChangeLoadProgressExtended(
      params.load_progress, params.loaded_bytes, params.loaded_elements,
      params.total_elements);
}

}  // namespace content
