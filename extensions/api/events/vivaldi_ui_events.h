// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_EVENTS_VIVALDI_UI_EVENTS_H_
#define EXTENSIONS_API_EVENTS_VIVALDI_UI_EVENTS_H_

#include "components/sessions/core/session_id.h"
#include "ui/content/vivaldi_event_hooks.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class VivaldiUIEvents : public VivaldiEventHooks {
 public:
  VivaldiUIEvents(content::WebContents* web_contents, SessionID window_id);

  static void SetupWindowContents(content::WebContents* web_contents,
                                  SessionID window_id);

  static void SendKeyboardShortcutEvent(
      content::BrowserContext* browser_context,
      const content::NativeWebKeyboardEvent& event,
      bool is_auto_repeat);

  // Helper for sending simple mouse change states. To be used by JS to detect
  // if a mouse change happens when it should not. JS will not receive this by a
  // regular document listners depending on keyboard shift state. 'is_motion' is
  // true when the change is that mouse has been moved, it is false when any
  // button has been pressed.
  static void SendMouseChangeEvent(content::BrowserContext* browser_context,
                                   bool is_motion);

  // VivaldiEventHooks implementation.

  bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;

  bool HandleMouseEvent(content::RenderWidgetHostViewBase* root_view,
                        const blink::WebMouseEvent& event) override;

  bool HandleWheelEvent(content::RenderWidgetHostViewBase* root_view,
                        const blink::WebMouseWheelEvent& event,
                        const ui::LatencyInfo& latency) override;

  bool HandleWheelEventAfterChild(
      content::RenderWidgetHostViewBase* root_view,
      content::RenderWidgetHostViewBase* child_view,
      const blink::WebMouseWheelEvent& event) override;

  bool HandleDragEnd(blink::WebDragOperation operation,
                     bool cancelled,
                     int screen_x,
                     int screen_y) override;

 private:
  content::WebContents* const web_contents_;
  const SessionID window_id_;
};


}  // namespace extensions

#endif  // EXTENSIONS_API_EVENTS_VIVALDI_UI_EVENTS_H_
