// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_EVENTS_VIVALDI_UI_EVENTS_H_
#define EXTENSIONS_API_EVENTS_VIVALDI_UI_EVENTS_H_

#include <memory>

#include "components/sessions/core/session_id.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/content/vivaldi_event_hooks.h"
#include "ui/gfx/geometry/point_f.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class VivaldiUIEvents : public VivaldiEventHooks {
 public:
  VivaldiUIEvents();
  ~VivaldiUIEvents() override;

  static void InitSingleton();

  static void SendKeyboardShortcutEvent(
      SessionID::id_type window_id,
      content::BrowserContext* browser_context,
      const input::NativeWebKeyboardEvent& event,
      bool is_auto_repeat);

  // Helper for sending simple mouse change states. To be used by JS to detect
  // if a mouse change happens when it should not. JS will not receive this by a
  // regular document listners depending on keyboard shift state. 'is_motion' is
  // true when the change is that mouse has been moved, it is false when any
  // button has been pressed.
  static void SendMouseChangeEvent(content::BrowserContext* browser_context,
                                   bool is_motion);

 private:
  struct MouseGestures {
    MouseGestures();
    ~MouseGestures();

    // To avoid depending on platform's focus policy store the id of the window
    // where the gesture was initiated and send the gesture events towards it
    // and not to the focused window, see VB-47721. Similarly, pass the initial
    // pointer coordinates relative to root to apply the gesture to the tab over
    // which the gesture has started, see VB-48232.
    SessionID::id_type window_id = 0;

    // Starting point of the gesture in screen coordinates. It is passed to UI
    // to apply the gesture to a specific element.
    gfx::PointF initial_pos;

    // Gesture started with the Alt key
    bool with_alt = false;

    bool recording = false;
    float last_x = 0.0f;
    float last_y = 0.0f;
    float min_move_squared = 0.0f;
    float stroke_tolerance = 0.0;

    // The string of uniqe gesture directions that is send to JS.
    std::string directions;
    int last_direction = -1;
  };

  struct WheelGestures {
    bool active = 0;
    SessionID::id_type window_id = 0;
  };

  struct RockerGestures {
    bool eat_next_left_mouseup = false;
    bool eat_next_right_mouseup = false;
  };

  bool CheckMouseMove(input::RenderWidgetHostViewInput* root_view,
                      const blink::WebMouseEvent& mouse_event);
  bool CheckRockerGesture(input::RenderWidgetHostViewInput* root_view,
                          const blink::WebMouseEvent& mouse_event);
  bool CheckMouseGesture(input::RenderWidgetHostViewInput* root_view,
                         const blink::WebMouseEvent& mouse_event);
  void CheckWebviewClick(input::RenderWidgetHostViewInput* root_view,
                         const blink::WebMouseEvent& mouse_event);
  bool CheckBackForward(input::RenderWidgetHostViewInput* root_view,
                        const blink::WebMouseEvent& mouse_event);
  bool HandleMouseGestureMove(const blink::WebMouseEvent& mouse_event);

  void StartMouseGestureDetection(input::RenderWidgetHostViewInput* root_view,
                                  const blink::WebMouseEvent& mouse_event,
                                  bool with_alt);
  bool FinishMouseOrWheelGesture(bool with_alt);

  // VivaldiEventHooks implementation.

  bool DoHandleKeyboardEvent(
      content::RenderWidgetHostImpl* widget_host,
      const input::NativeWebKeyboardEvent& event) override;

  bool DoHandleMouseEvent(input::RenderWidgetHostViewInput* root_view,
                          const blink::WebMouseEvent& event) override;

  bool DoHandleWheelEvent(input::RenderWidgetHostViewInput* root_view,
                          const blink::WebMouseWheelEvent& event,
                          const ui::LatencyInfo& latency) override;

  bool DoHandleWheelEventAfterChild(
      input::RenderWidgetHostViewInput* root_view,
      const blink::WebMouseWheelEvent& event) override;

  bool DoHandleDragEnd(content::WebContents* web_contents,
                       ui::mojom::DragOperation operation,
                       int screen_x,
                       int screen_y) override;

  std::unique_ptr<MouseGestures> mouse_gestures_;
  WheelGestures wheel_gestures_;
  RockerGestures rocker_gestures_;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_EVENTS_VIVALDI_UI_EVENTS_H_
