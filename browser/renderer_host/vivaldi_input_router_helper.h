// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_RENDERER_HOST_VIVALDI_INPUT_ROUTER_HELPER_H_
#define BROWSER_RENDERER_HOST_VIVALDI_INPUT_ROUTER_HELPER_H_

namespace ui {
class LatencyInfo;
}  // namespace ui

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace content {

// Utilities to help with Vivaldi-specific event routing. They are defined as
// static methods in a class so it is easy to declare them friends with Chromium
// classes they need access to.
class VivaldiInputRouterHelper {
 public:
  // NOTE(tomas@vivaldi.com): Send a copy of the event to the root view, so that
  // mouse gestures can work. See VB-41799, VB-41071, VB-42761
  static void SendEventCopyToUI(RenderWidgetHostViewBase* root_view,
                                RenderWidgetHostViewBase* target_view,
                                const blink::WebInputEvent& event,
                                const ui::LatencyInfo& latency);
};

}  // namespace content

#endif  // BROWSER_RENDERER_HOST_VIVALDI_INPUT_ROUTER_HELPER_H_
