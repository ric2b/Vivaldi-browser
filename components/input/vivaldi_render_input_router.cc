// Copyright 2024 Vivaldi Technologies. All rights reserved.

#include "components/input/render_input_router.h"

#include "app/vivaldi_apptools.h"
#include "components/input/render_widget_host_view_input.h"
#include "ui/content/vivaldi_event_hooks.h"

namespace input {

bool RenderInputRouter::VivaldiHandleEventAfterChild(
    const blink::WebMouseWheelEvent& event) {
  if (!view_input_)
    return false;
  input::RenderWidgetHostViewInput* root_view = view_input_->GetRootView();
  if (!root_view)
    return false;

  // RenderWidgetHostInputEventRouter::DispatchMouseWheelEvent() calls
  // VivaldiEventHooks when root_view == view.
  if (root_view == view_input_.get())
    return false;

  return VivaldiEventHooks::HandleWheelEventAfterChild(root_view, event);
}

}  // namespace input
