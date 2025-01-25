// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "ui/content/vivaldi_event_hooks.h"

#include "app/vivaldi_apptools.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"

VivaldiEventHooks* VivaldiEventHooks::instance_ = nullptr;

// static
bool VivaldiEventHooks::HasInstance() {
  return instance_ != nullptr;
}

// static
void VivaldiEventHooks::InitInstance(VivaldiEventHooks& instance) {
  DCHECK(!instance_) << "This should be called only once";
  instance_ = &instance;
}

// static
bool VivaldiEventHooks::HandleMouseEvent(
    input::RenderWidgetHostViewInput* root_view,
    const blink::WebMouseEvent& event) {
  if (!instance_)
    return false;
  return instance_->DoHandleMouseEvent(root_view, event);
}

// static
bool VivaldiEventHooks::HandleWheelEvent(
    input::RenderWidgetHostViewInput* root_view,
    const blink::WebMouseWheelEvent& event,
    const ui::LatencyInfo& latency) {
  if (!instance_)
    return false;
  return instance_->DoHandleWheelEvent(root_view, event, latency);
}

// static
bool VivaldiEventHooks::HandleWheelEventAfterChild(
    input::RenderWidgetHostViewInput* root_view,
    const blink::WebMouseWheelEvent& event) {
  if (!instance_)
    return false;
  return instance_->DoHandleWheelEventAfterChild(root_view, event);
}

// static
bool VivaldiEventHooks::HandleKeyboardEvent(
    content::RenderWidgetHostImpl* widget_host,
    const input::NativeWebKeyboardEvent& event) {
  if (!instance_)
    return false;
  return instance_->DoHandleKeyboardEvent(widget_host, event);
}

bool VivaldiEventHooks::HandleDragEnd(content::WebContents* web_contents,
                                      ui::mojom::DragOperation operation,
                                      int screen_x,
                                      int screen_y) {
  if (!web_contents || !instance_)
    return false;
  return instance_->DoHandleDragEnd(web_contents, operation, screen_x,
                                    screen_y);
}