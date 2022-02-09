// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "content/browser/renderer_host/render_frame_host_impl.h"

#include "content/browser/renderer_host/frame_tree_node.h"

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace content {

const mojo::AssociatedRemote<vivaldi::mojom::VivaldiFrameService>&
RenderFrameHostImpl::GetVivaldiFrameService() {
  if (!vivaldi_frame_service_)
    GetRemoteAssociatedInterfaces()->GetInterface(&vivaldi_frame_service_);
  return vivaldi_frame_service_;
}

}  // namespace content
