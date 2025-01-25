// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef UI_CONTENT_VIVALDI_EVENT_HOOKS_H_
#define UI_CONTENT_VIVALDI_EVENT_HOOKS_H_

#include "base/supports_user_data.h"
#include "base/component_export.h"  // nogncheck
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"

namespace blink {
class WebMouseEvent;
class WebMouseWheelEvent;
}  // namespace blink

namespace input {
struct NativeWebKeyboardEvent;
class RenderWidgetHostViewInput;
}  // namespace input

namespace content {
class RenderWidgetHostImpl;
class WebContents;
}  // namespace content

namespace ui {
class LatencyInfo;
}  // namespace ui

// Hooks into Chromium event processing. The implementation is provided in
// vivaldi_ui_events.cc and stored in WebContents associated with the Vivaldi
// window using SupportsUserData API.
class COMPONENT_EXPORT(INPUT) VivaldiEventHooks
    : public base::SupportsUserData::Data {
 public:
  // Check for a mouse gesture event before it is dispatched to the web page
  // or default chromium handlers. Return true to stop further event
  // propagation or false to allow normal event flow.
  static bool HandleMouseEvent(input::RenderWidgetHostViewInput* root_view,
                               const blink::WebMouseEvent& event);

  // Check for a wheel gesture event before it is dispatched to the web page
  // or default chromium handlers. Return true to stop further event
  // propagation or false to allow normal event flow.
  static bool HandleWheelEvent(input::RenderWidgetHostViewInput* root_view,
                               const blink::WebMouseWheelEvent& event,
                               const ui::LatencyInfo& latency);

  // Check for a wheel gesture after the event was not consumed by any child
  // view. Return true to stop further event propagation or false to allow
  // normal event flow.
  static bool HandleWheelEventAfterChild(
      input::RenderWidgetHostViewInput* root_view,
      const blink::WebMouseWheelEvent& event);

  // Handle a keyboard event before it is send to the renderer process. Return
  // true to stop further event propagation or false to allow normal event flow.
  static bool HandleKeyboardEvent(content::RenderWidgetHostImpl* widget_host,
                                  const input::NativeWebKeyboardEvent& event);

  // Hook to notify UI about the end of the drag operation and pointer position
  // when the user released the pointer. Return true to prevent any default
  // action in Chromium. cancelled indicate that the platform API indicated
  // explicitly cancelled drag (currently can be true only on Windows).
  static bool HandleDragEnd(content::WebContents* web_contents,
                            ui::mojom::DragOperation operation,
                            int screen_x,
                            int screen_y);

 protected:
  static bool HasInstance();

  static void InitInstance(VivaldiEventHooks& instance);

  virtual bool DoHandleMouseEvent(input::RenderWidgetHostViewInput* root_view,
                                  const blink::WebMouseEvent& event) = 0;

  virtual bool DoHandleWheelEvent(input::RenderWidgetHostViewInput* root_view,
                                  const blink::WebMouseWheelEvent& event,
                                  const ui::LatencyInfo& latency) = 0;

  virtual bool DoHandleWheelEventAfterChild(
      input::RenderWidgetHostViewInput* root_view,
      const blink::WebMouseWheelEvent& event) = 0;

  virtual bool DoHandleKeyboardEvent(
      content::RenderWidgetHostImpl* widget_host,
      const input::NativeWebKeyboardEvent& event) = 0;

  virtual bool DoHandleDragEnd(content::WebContents* web_contents,
                               ui::mojom::DragOperation operation,
                               int screen_x,
                               int screen_y) = 0;

 private:
  static VivaldiEventHooks* instance_;
};

#endif  // UI_CONTENT_VIVALDI_EVENT_HOOKS_H_
