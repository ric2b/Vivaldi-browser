// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved
#include "content/browser/devtools/protocol/emulation_handler.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {
namespace protocol {

bool EmulationHandler::IsTouchEmulationRequiredByOthers() {
  for (WebContentsImpl* contents : GetWebContents()->GetAllWebContents()) {
    for (FrameTreeNode* node : contents->GetPrimaryFrameTree().Nodes()) {
      RenderWidgetHostViewBase* rwhvb = static_cast<RenderWidgetHostViewBase*>(
          node->render_manager()->GetRenderWidgetHostView());
      if (rwhvb) {
        RenderWidgetHostImpl* rwhi = rwhvb->host();
        if (rwhi != host_->GetRenderWidgetHost() &&
            rwhi->IsDeviceEmulationActive())
          return true;
      }
    }
  }
  return false;
}
}  // namespace protocol
}  // namespace content