// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"

namespace content {
void RenderWidgetHostInputEventRouter::MergeFrameSinkIdOwners(
    RenderWidgetHostInputEventRouter* other_input_event_router) {
  // NOTE(david@vivaldi): This method adds the FrameSinkIds of another
  // Input Event Router into the current one. This is most likly required when
  // we have iframes.
  for (auto entry : other_input_event_router->owner_map_) {
    if (!is_registered(entry.first)) {
      AddFrameSinkIdOwner(entry.first, entry.second);
    }
  }
}
}  // namespace content