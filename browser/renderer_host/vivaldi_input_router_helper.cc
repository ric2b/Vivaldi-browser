// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "browser/renderer_host/vivaldi_input_router_helper.h"

#include "content/browser/renderer_host/input/input_router_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "ui/events/blink/web_input_event_traits.h"

#include "app/vivaldi_apptools.h"

namespace content {

namespace {

// This sources is included from
// content/browser/renderer_host/input/input_router_impl.cc where the following
// anonymous function is defined.
ui::WebScopedInputEvent ScaleEvent(const blink::WebInputEvent& event,
                                   double scale);

}  // namespace

// static
void VivaldiInputRouterHelper::SendEventCopyToUI(
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* target_view,
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency) {
  DCHECK(vivaldi::IsVivaldiRunning());
  DCHECK(!root_view->IsRenderWidgetHostViewChildFrame());

  if (target_view == root_view)
    return;

  switch (event.GetType()) {
    case blink::WebInputEvent::kMouseUp:
    case blink::WebInputEvent::kMouseDown:
    case blink::WebInputEvent::kMouseMove:
      break;
    default:
      // NOTE(igor@vivaldi.com): We forward blink::WebInputEvent::kMouseWheel
      // to the root view in
      // InputRouterImpl::MouseWheelEventHandledWithRedirect() only after we
      // know that the page has not consumed it. This let the page to
      // implement custom scrolling and zooming.
      return;
  }

  // NOTE(igor@vivaldi.com): As we are sending a copy of the event just for
  // internal accounting and notifications in JS we use the lowest level event
  // dispatching API to bypass all native event processing in Chromium and avoid
  // bugs like VB-43554.
  //
  // This code follows InputRouterImpl::FilterAndSendWebInputEvent.
  InputRouter* router = root_view->host()->input_router();
  if (!router)
    return;
  InputRouterImpl* router_impl = static_cast<InputRouterImpl*>(router);
  std::unique_ptr<InputEvent> event_to_dispatch = std::make_unique<InputEvent>(
      ScaleEvent(event, router_impl->device_scale_factor_), latency);
  router_impl->client_->GetWidgetInputHandler()->DispatchNonBlockingEvent(
      std::move(event_to_dispatch));
}

}  // namespace content
