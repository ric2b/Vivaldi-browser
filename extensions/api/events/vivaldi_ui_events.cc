// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/events/vivaldi_ui_events.h"

#include "app/vivaldi_apptools.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "browser/vivaldi_browser_finder.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/input/render_widget_host_view_input.h"
#include "components/prefs/pref_service.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"  // nogncheck
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "extensions/schema/tabs_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "prefs/vivaldi_pref_names.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"  //nogncheck
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"

namespace extensions {

namespace tabs_private = vivaldi::tabs_private;

namespace {

// The distance the mouse pointer has to travel in logical pixels before we
// start recording a gesture and eat the following pointer move events.
constexpr float MOUSE_GESTURE_THRESHOLD = 5.0f;

bool ShouldPreventWindowGestures(VivaldiBrowserWindow* window) {
  if (!window->web_contents())
    return true;
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          window->web_contents());
  if (manager && manager->IsDialogActive()) {
    // Avoid dangling modal dialogs that will crash if the tab is closed
    // through a gesture.
    // TODO(pettern): There is still a chance a tab will be able to close
    // before this check, so investigate blocking on the api level after
    // the tab close rewrite on the js side.
    return true;
  }
  return false;
}

VivaldiBrowserWindow* FindMouseEventWindowFromView(
    input::RenderWidgetHostViewInput* root_view,
    bool ignore_native_children = false) {
  if (!root_view->GetViewRenderInputRouter()) {
    return nullptr;
  }

  content::RenderWidgetHostViewBase* host =
      static_cast<content::RenderWidgetHostViewBase*>(root_view);

  auto* web_contents =
      content::WebContentsImpl::FromRenderWidgetHostImpl(host->host());

  if (!web_contents) {
    return nullptr;
  }
  // web_contents is not ourtermost content when the root_view corresponds
  // to a native control like date picker on Mac.
  web_contents = web_contents->GetOutermostWebContents();
  VivaldiBrowserWindow* window =
      ::vivaldi::FindWindowForEmbedderWebContents(web_contents);
  if (window && ShouldPreventWindowGestures(window)) {
    window = nullptr;
  }
  return window;
}

VivaldiBrowserWindow* FindMouseEventWindowFromId(SessionID::id_type window_id) {
  VivaldiBrowserWindow* window = nullptr;
  Browser* browser = ::vivaldi::FindBrowserByWindowId(window_id);
  if (browser) {
    window = VivaldiBrowserWindow::FromBrowser(browser);
    if (window && ShouldPreventWindowGestures(window)) {
      window = nullptr;
    }
  }
  return window;
}

// Transform screen coordinates to the UI coordinates for the given window.
gfx::PointF TransformToWindowUICoordinates(VivaldiBrowserWindow* window,
                                           gfx::PointF screen_point) {
  gfx::Rect ui_bounds = window->web_contents()->GetContainerBounds();
  gfx::PointF p = screen_point;
  p.Offset(-ui_bounds.x(), -ui_bounds.y());
  p = ::vivaldi::ToUICoordinates(window->web_contents(), p);
  return p;
}

void SendEventToUI(VivaldiBrowserWindow* window,
                   const std::string& eventname,
                   base::Value::List args) {
  // TODO(igor@vivaldi.com): This broadcats the event to all windows and
  // extensions forcing our JS code to check in each window if it matches
  // the window id embedded into the event. Find a way to send this only to
  // Vivaldi JS in a specific window.
  ::vivaldi::BroadcastEvent(eventname, std::move(args),
                            window->browser()->profile());
}

bool IsLoneAltKeyPressed(int modifiers) {
  using blink::WebInputEvent;
  return (modifiers & WebInputEvent::kKeyModifiers) == WebInputEvent::kAltKey;
}

bool IsGestureMouseMove(const blink::WebMouseEvent& mouse_event) {
  using blink::WebInputEvent;
  using blink::WebMouseEvent;
  DCHECK(mouse_event.GetType() == WebInputEvent::Type::kMouseMove);
  return (mouse_event.button == WebMouseEvent::Button::kRight &&
          !(mouse_event.GetModifiers() & WebInputEvent::kLeftButtonDown));
}

bool IsGestureAltMouseMove(const blink::WebMouseEvent& mouse_event) {
  DCHECK(mouse_event.GetType() == blink::WebInputEvent::Type::kMouseMove);
  return IsLoneAltKeyPressed(mouse_event.GetModifiers());
}

}  // namespace

VivaldiUIEvents::MouseGestures::MouseGestures() = default;
VivaldiUIEvents::MouseGestures::~MouseGestures() = default;

VivaldiUIEvents::VivaldiUIEvents() = default;
VivaldiUIEvents::~VivaldiUIEvents() = default;

void VivaldiUIEvents::StartMouseGestureDetection(
    input::RenderWidgetHostViewInput* root_view,
    const blink::WebMouseEvent& mouse_event,
    bool with_alt) {
  DCHECK(!mouse_gestures_);

  // Ignore any gesture after the wheel scroll with the Alt key or right button
  // pressed but before the key or button was released.
  if (wheel_gestures_.active)
    return;
  VivaldiBrowserWindow* window = FindMouseEventWindowFromView(root_view);
  if (!window)
    return;
  Profile* profile = window->browser()->profile();
  if (with_alt) {
    if (!profile->GetPrefs()->GetBoolean(
            vivaldiprefs::kMouseGesturesAltGesturesEnabled))
      return;
  } else {
    if (!profile->GetPrefs()->GetBoolean(vivaldiprefs::kMouseGesturesEnabled))
      return;
  }

  mouse_gestures_ = std::make_unique<MouseGestures>();
  mouse_gestures_->window_id = window->id();
  mouse_gestures_->initial_pos = mouse_event.PositionInScreen();
  mouse_gestures_->with_alt = with_alt;
  mouse_gestures_->last_x = mouse_event.PositionInScreen().x();
  mouse_gestures_->last_y = mouse_event.PositionInScreen().y();
  mouse_gestures_->stroke_tolerance =
      static_cast<float>(profile->GetPrefs()->GetDouble(
          vivaldiprefs::kMouseGesturesStrokeTolerance));

  SendEventToUI(window, tabs_private::OnMouseGestureDetection::kEventName,
                tabs_private::OnMouseGestureDetection::Create(
                    mouse_gestures_->window_id));
}

bool VivaldiUIEvents::HandleMouseGestureMove(
    const blink::WebMouseEvent& mouse_event) {
  DCHECK(mouse_event.GetType() == blink::WebInputEvent::Type::kMouseMove);
  DCHECK(mouse_gestures_);
  float x = mouse_event.PositionInScreen().x();
  float y = mouse_event.PositionInScreen().y();
  bool eat_event = false;

  // We do not need to account for HiDPI screens when comparing dx and dy with
  // threshould and tolerance. The values are in logical pixels adjusted from
  // real ones according to RenderWidgetHostViewBase::GetDeviceScaleFactor().
  float dx = x - mouse_gestures_->last_x;
  float dy = y - mouse_gestures_->last_y;
  if (!mouse_gestures_->recording) {
    if (fabs(dx) < MOUSE_GESTURE_THRESHOLD &&
        fabs(dy) < MOUSE_GESTURE_THRESHOLD) {
      return eat_event;
    }
    // The recording flag persists if we go under the threshold by moving the
    // mouse into the original location, which is expected.
    mouse_gestures_->recording = true;

    // tolerance = movement in pixels before gesture move initiates.
    // For min_move we devide the preference by two as we require at least two
    // mouse move events in the same direction to acount as a gesture move.
    float tolerance = mouse_gestures_->stroke_tolerance;
    mouse_gestures_->min_move_squared = (tolerance / 2.0f) * (tolerance / 2.0f);
  }

  // Do not propagate this mouse move as we are in the recording phase.
  eat_event = true;

  float sqDist = dx * dx + dy * dy;
  if (sqDist <= mouse_gestures_->min_move_squared)
    return eat_event;

  mouse_gestures_->last_x = x;
  mouse_gestures_->last_y = y;

  // Detect if the direction of movement is into one of 4 sectors,
  // -45° .. 45°, 45° .. 135°, 135° .. 225°, 225° .. 315°.
  unsigned sector;
  if (fabs(dx) >= fabs(dy)) {
    sector = (dx >= 0) ? 0 : 2;
  } else {
    sector = (dy >= 0) ? 1 : 3;
  }

  // Encode the sector as '0' - '2' - '4' - '6' characters.
  char direction = static_cast<char>('0' + (sector * 2));

  // We only record moves that repeats at least twice with the same value
  // and for repeated values we only record the first one.
  if (mouse_gestures_->last_direction != direction) {
    mouse_gestures_->last_direction = direction;
  } else if (mouse_gestures_->directions.size() == 0 ||
             mouse_gestures_->directions.back() != direction) {
    mouse_gestures_->directions.push_back(direction);
  }
  return eat_event;
}

bool VivaldiUIEvents::FinishMouseOrWheelGesture(bool with_alt) {
  bool after_gesture = false;
  if (wheel_gestures_.active) {
    DCHECK(!mouse_gestures_);
    after_gesture = true;
    SessionID::id_type window_id = wheel_gestures_.window_id;
    wheel_gestures_.active = false;
    wheel_gestures_.window_id = 0;
    if (VivaldiBrowserWindow* window = FindMouseEventWindowFromId(window_id)) {
      SendEventToUI(window, tabs_private::OnTabSwitchEnd::kEventName,
                    tabs_private::OnTabSwitchEnd::Create(window_id));
    }
  }
  if (!mouse_gestures_)
    return after_gesture;

  // Alt gestures can only be finished with the keyboard and pure mouse gestures
  // can only be finished with the mouse.
  if (with_alt != mouse_gestures_->with_alt)
    return after_gesture;

  // Do not send a gesture event and eat the pointer/keyboard up when we got no
  // gesture moves. This allows context menu to work on pointer up when on a
  // touchpad fingers can easly move more then MOUSE_GESTURE_THRESHOLD pixels,
  // see VB-48846.
  if (!mouse_gestures_->directions.empty()) {
    after_gesture = true;

    SessionID::id_type window_id = mouse_gestures_->window_id;
    if (VivaldiBrowserWindow* window = FindMouseEventWindowFromId(window_id)) {
      gfx::PointF p =
          TransformToWindowUICoordinates(window, mouse_gestures_->initial_pos);
      SendEventToUI(window, tabs_private::OnMouseGesture::kEventName,
                    tabs_private::OnMouseGesture::Create(
                        window_id, p.x(), p.y(), mouse_gestures_->directions));
    }
  }
  mouse_gestures_.reset();
  return after_gesture;
}

bool VivaldiUIEvents::CheckMouseMove(
    input::RenderWidgetHostViewInput* root_view,
    const blink::WebMouseEvent& mouse_event) {
  DCHECK_EQ(mouse_event.GetType(), blink::WebInputEvent::Type::kMouseMove);
  bool eat_event = false;
  if (!mouse_gestures_) {
    bool gesture = false;
    bool with_alt = false;
    if (IsGestureMouseMove(mouse_event)) {
      gesture = true;
    } else if (IsGestureAltMouseMove(mouse_event)) {
      gesture = true;
      with_alt = true;
    }
    if (gesture) {
      // Handle the right button pressed outside the window before entering
      // the window.
      StartMouseGestureDetection(root_view, mouse_event, with_alt);
    }
  } else {
    bool gesture = false;
    if (mouse_gestures_->with_alt) {
      gesture = IsGestureAltMouseMove(mouse_event);
    } else {
      gesture = IsGestureMouseMove(mouse_event);
    }
    if (gesture) {
      eat_event = HandleMouseGestureMove(mouse_event);
    } else {
      // This happens when the right mouse button is released outside of webview
      // or the alt key was released when the window lost input focus.
      mouse_gestures_.reset();
    }
  }
  return eat_event;
}

bool VivaldiUIEvents::CheckMouseGesture(
    input::RenderWidgetHostViewInput* root_view,
    const blink::WebMouseEvent& mouse_event) {
  using blink::WebInputEvent;
  using blink::WebMouseEvent;
  using blink::WebMouseWheelEvent;

  DCHECK(mouse_event.GetType() != WebInputEvent::Type::kMouseMove);
  bool eat_event = false;
  // We should not have both wheel and mouse gestures running.
  DCHECK(!wheel_gestures_.active || !mouse_gestures_);
  if (mouse_event.GetType() == WebInputEvent::Type::kMouseDown) {
    if (!mouse_gestures_) {
      if (mouse_event.button == WebMouseEvent::Button::kRight &&
          !(mouse_event.GetModifiers() & WebInputEvent::kLeftButtonDown)) {
        StartMouseGestureDetection(root_view, mouse_event, false);
      }
    }
  } else if (mouse_event.GetType() == WebInputEvent::Type::kMouseUp) {
    eat_event = FinishMouseOrWheelGesture(false);
  }
  return eat_event;
}

bool VivaldiUIEvents::CheckRockerGesture(
    input::RenderWidgetHostViewInput* root_view,
    const blink::WebMouseEvent& mouse_event) {
  using blink::WebInputEvent;
  using blink::WebMouseEvent;

  bool eat_event = false;
  if (mouse_event.GetType() == WebInputEvent::Type::kMouseDown) {
    enum { ROCKER_NONE, ROCKER_LEFT, ROCKER_RIGHT } rocker_action = ROCKER_NONE;
    if (mouse_event.button == WebMouseEvent::Button::kLeft) {
      if (mouse_event.GetModifiers() & WebInputEvent::kRightButtonDown) {
        rocker_action = ROCKER_LEFT;
      } else {
        // The eat flags can be true if buttons were released outside of the
        // window.
        rocker_gestures_.eat_next_right_mouseup = false;
      }
    } else if (mouse_event.button == WebMouseEvent::Button::kRight) {
      if (mouse_event.GetModifiers() & WebInputEvent::kLeftButtonDown) {
        rocker_action = ROCKER_RIGHT;
      } else {
        rocker_gestures_.eat_next_left_mouseup = false;
      }
    }
    // Check if rocker gestures are enabled only after we detected them to avoid
    // preference checks on each mouse down.
    if (rocker_action != ROCKER_NONE) {
      VivaldiBrowserWindow* window = FindMouseEventWindowFromView(root_view);
      if (!window)
        return eat_event;
      Profile* profile = window->browser()->profile();
      if (profile->GetPrefs()->GetBoolean(
              vivaldiprefs::kMouseGesturesRockerGesturesEnabled)) {
        // We got a rocker gesture. Follow Opera's implementation and consume
        // the last event which is a mouse down from either the left or the
        // right button and consume both the future left and right mouse up to
        // prevent clicks, menus or similar page actions.
        eat_event = true;
        rocker_gestures_.eat_next_left_mouseup = true;
        rocker_gestures_.eat_next_right_mouseup = true;

        // Stop any mouse gesture if any.
        mouse_gestures_.reset();
        bool is_left = (rocker_action == ROCKER_LEFT);
        SendEventToUI(
            window, tabs_private::OnRockerGesture::kEventName,
            tabs_private::OnRockerGesture::Create(window->id(), is_left));
      }
    }
  } else if (mouse_event.GetType() == WebInputEvent::Type::kMouseUp) {
    if (rocker_gestures_.eat_next_left_mouseup) {
      if (mouse_event.button == WebMouseEvent::Button::kLeft) {
        rocker_gestures_.eat_next_left_mouseup = false;
        eat_event = true;
      } else if (!(mouse_event.GetModifiers() &
                   WebInputEvent::kLeftButtonDown)) {
        // Missing mouse up when mouse was released outside the window etc.
        rocker_gestures_.eat_next_left_mouseup = false;
      }
    }
    if (rocker_gestures_.eat_next_right_mouseup) {
      if (mouse_event.button == WebMouseEvent::Button::kRight) {
        rocker_gestures_.eat_next_right_mouseup = false;
        eat_event = true;
      } else if (!(mouse_event.GetModifiers() &
                   WebInputEvent::kRightButtonDown)) {
        rocker_gestures_.eat_next_right_mouseup = false;
      }
    }
  }
  return eat_event;
}

// Notify Vivaldi UI about clicks into webviews to properly track focused tabs
// and to dismiss our popup controls and other GUI elements that cover
// web views, see VB-48000.
//
// Current implementation sends the extension event for any click inside Vivaldi
// window including clicks into UI outside webviews. Chromium API for locating
// views from the point are exteremely heavy, see code in
// RenderWidgetHostInputEventRouter::FindViewAtLocation(), and it is simpler to
// filter out clicks outside the webviews in the handler for the extension event
// using document.elementFromPoint().
void VivaldiUIEvents::CheckWebviewClick(
    input::RenderWidgetHostViewInput* root_view,
    const blink::WebMouseEvent& mouse_event) {
  using blink::WebInputEvent;
  using Button = blink::WebPointerProperties::Button;

  bool mousedown = (mouse_event.GetType() == WebInputEvent::Type::kMouseDown);
  bool mouseup = (mouse_event.GetType() == WebInputEvent::Type::kMouseUp);
  if (!mousedown && !mouseup)
    return;

  int button;
  if (mouse_event.button == Button::kLeft) {
    button = 0;
  } else if (mouse_event.button == Button::kMiddle) {
    button = 1;
  } else if (mouse_event.button == Button::kRight) {
    button = 2;
  } else {
    return;
  }

  VivaldiBrowserWindow* window = FindMouseEventWindowFromView(root_view);
  if (!window)
    return;

  /*
  if (window->web_contents()->GetRenderWidgetHostView() != root_view) {
    // The event was originated in a native child view like that of a date
    // picker input elelemnt on Mac. As popups such controls are supposed
    // to grab the mouse click to themselves and our UI should not know about
    // it.
    DCHECK(root_view->GetWidgetType() == content::WidgetType::kPopup);
    return;
  }*/

  gfx::PointF p =
      TransformToWindowUICoordinates(window, mouse_event.PositionInScreen());
  SendEventToUI(window, tabs_private::OnWebviewClickCheck::kEventName,
                tabs_private::OnWebviewClickCheck::Create(
                    window->id(), mousedown, button, p.x(), p.y()));
}

bool VivaldiUIEvents::CheckBackForward(
    input::RenderWidgetHostViewInput* root_view,
    const blink::WebMouseEvent& event) {
  using Button = blink::WebPointerProperties::Button;

  if (event.GetType() != blink::WebInputEvent::Type::kMouseUp)
    return false;

  bool back = (event.button == Button::kBack);
  bool forward = (event.button == Button::kForward);
  if (!back && !forward)
    return false;

  VivaldiBrowserWindow* window = FindMouseEventWindowFromView(root_view);
  if (!window)
    return false;

  content::WebContents* activecontents =
      window->browser()->tab_strip_model()->GetActiveWebContents();
  if (!activecontents)
    return false;

  bool eat_event = false;
  content::NavigationController& controller = activecontents->GetController();
  if (back && controller.CanGoBack()) {
    controller.GoBack();
    eat_event = true;
  }
  if (forward && controller.CanGoForward()) {
    controller.GoForward();
    eat_event = true;
  }
  return eat_event;
}

bool VivaldiUIEvents::DoHandleKeyboardEvent(
    content::RenderWidgetHostImpl* widget_host,
    const input::NativeWebKeyboardEvent& event) {
  bool down = false;
  bool after_gesture = false;
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown) {
    down = true;
  } else if (event.GetType() == blink::WebInputEvent::Type::kKeyUp) {
    // Check for Alt aka Menu release
    if (event.windows_key_code == blink::VKEY_MENU) {
      after_gesture = FinishMouseOrWheelGesture(true);
    }
  } else {
    return false;
  }

  content::WebContents* web_contents =
      content::WebContentsImpl::FromRenderWidgetHostImpl(widget_host);
  if (!web_contents)
    return false;
  web_contents = web_contents->GetOutermostWebContents();
  VivaldiBrowserWindow* window =
      ::vivaldi::FindWindowForEmbedderWebContents(web_contents);
  if (!window)
    return false;

  // We only need the four lowest bits for cmd, alt, ctrl, shift
  int modifiers = event.GetModifiers() & 15;

  bool is_auto_repeat =
    event.GetModifiers() & blink::WebInputEvent::kIsAutoRepeat;

  SendEventToUI(window, tabs_private::OnKeyboardChanged::kEventName,
                tabs_private::OnKeyboardChanged::Create(
                    window->id(), down, modifiers, event.windows_key_code,
                    after_gesture, is_auto_repeat));

  return after_gesture;
}

bool VivaldiUIEvents::DoHandleMouseEvent(
    input::RenderWidgetHostViewInput* root_view,
    const blink::WebMouseEvent& event) {
  // Check if the view has pointer-lock enabled. This should take precedence so
  // that the webpage mouse events do not trigger Vivaldi mouse actions by
  // accident. VB-66772.
  if (root_view->IsPointerLocked()) {
    return /*eat_event*/ false;
  }
  if (event.GetType() == blink::WebInputEvent::Type::kMouseMove) {
    return CheckMouseMove(root_view, event);
  }

  // Rocker gestures take priority over any other mouse gestures.
  bool eat_event = CheckRockerGesture(root_view, event);
  if (!eat_event) {
    eat_event = CheckMouseGesture(root_view, event);
  }
  if (!eat_event) {
    eat_event = CheckBackForward(root_view, event);
  }
  if (!eat_event) {
    CheckWebviewClick(root_view, event);
  }

  return eat_event;
}

bool VivaldiUIEvents::DoHandleWheelEvent(
    input::RenderWidgetHostViewInput* root_view,
    const blink::WebMouseWheelEvent& wheel_event,
    const ui::LatencyInfo& latency) {
  using blink::WebInputEvent;
  using blink::WebMouseWheelEvent;

  int modifiers = wheel_event.GetModifiers();
  constexpr int left = WebInputEvent::kLeftButtonDown;
  constexpr int right = WebInputEvent::kRightButtonDown;
  bool only_right = (modifiers & (left | right)) == right;
  bool wheel_gesture_event = only_right || IsLoneAltKeyPressed(modifiers);
  if (!wheel_gesture_event)
    return false;

  // We should not have both wheel and mouse gestures running.
  DCHECK(!wheel_gestures_.active || !mouse_gestures_);

  VivaldiBrowserWindow* window = FindMouseEventWindowFromView(root_view);
  if (!window)
    return false;

  Profile* profile = window->browser()->profile();
  if (!profile->GetPrefs()->GetBoolean(vivaldiprefs::kMouseWheelTabSwitch))
    return false;

  if (!wheel_gestures_.active) {
    // The event starts a new wheel gesture sequence canceling any mouse
    // gesture detection unless the wheel phase is:
    //
    // kPhaseEnded - with the inertial scrolling we can receive this with
    // modifiers indicating a pressed button after the user stopped
    // rotating the wheel and after the browser received the mouse up event.
    //
    // kPhaseCancelled - when the user presses touchpad with two fingers we
    // may receive kPhaseMayBegin with no modifiers, then kMouseDown with
    // kRightButtonDown then kPhaseCancelled with kRightButtonDown.
    constexpr int unwanted_phases =
        WebMouseWheelEvent::kPhaseEnded | WebMouseWheelEvent::kPhaseCancelled;
    if (!(wheel_event.phase & unwanted_phases)) {
      mouse_gestures_.reset();
      wheel_gestures_.active = true;
      wheel_gestures_.window_id = window->id();
    }
  }
  root_view->ProcessMouseWheelEvent(wheel_event, latency);
  return true;
}

bool VivaldiUIEvents::DoHandleWheelEventAfterChild(
    input::RenderWidgetHostViewInput* root_view,
    const blink::WebMouseWheelEvent& event) {
  using blink::WebInputEvent;
  using blink::WebMouseWheelEvent;
  constexpr int zoom_modifier =
#if BUILDFLAG(IS_MAC)
      WebInputEvent::kMetaKey
#else
      WebInputEvent::kControlKey
#endif
      ;
  int modifiers = event.GetModifiers();
  if ((modifiers & WebInputEvent::kKeyModifiers) != zoom_modifier)
    return false;

  constexpr int unwanted_phases =
      WebMouseWheelEvent::kPhaseEnded | WebMouseWheelEvent::kPhaseCancelled;
  if (event.phase & unwanted_phases)
    return false;

  VivaldiBrowserWindow* window = FindMouseEventWindowFromView(root_view);
  if (!window)
    return false;

  Profile* profile = window->browser()->profile();
  if (!profile->GetPrefs()->GetBoolean(vivaldiprefs::kMouseWheelPageZoom)) {
    return false;
  }

  float wheel_ticks;
  if (event.wheel_ticks_y != 0.0f) {
    wheel_ticks = event.wheel_ticks_y;
  } else if (event.wheel_ticks_x != 0.0f) {
    wheel_ticks = event.wheel_ticks_x;
  } else {
    return false;
  }

  // TODO(igor@vivaldi.com): Shall we scale steps according to wheel_ticks?
  double steps = (wheel_ticks > 0) ? 1.0 : -1.0;
  gfx::PointF p =
      TransformToWindowUICoordinates(window, event.PositionInScreen());
  SendEventToUI(
      window, tabs_private::OnPageZoom::kEventName,
      tabs_private::OnPageZoom::Create(window->id(), steps, p.x(), p.y()));

  return true;
}

bool VivaldiUIEvents::DoHandleDragEnd(content::WebContents* web_contents,
                                      ui::mojom::DragOperation operation,
                                      int screen_x,
                                      int screen_y) {
  if (!::vivaldi::IsTabDragInProgress())
    return false;
  ::vivaldi::SetTabDragInProgress(false);

  bool cancelled = false;
#if BUILDFLAG(IS_WIN)
  if (::vivaldi::g_cancelled_drag) {
    cancelled = true;
  }
#endif
  bool outside = ::vivaldi::ui_tools::IsOutsideAppWindow(screen_x, screen_y);
  if (!outside && operation == ui::mojom::DragOperation::kNone) {
    // None of browser windows accepted the drag and we do not moving tabs out.
    cancelled = true;
  }

  ::vivaldi::BroadcastEvent(
      tabs_private::OnDragEnd::kEventName,
      tabs_private::OnDragEnd::Create(cancelled, outside, screen_x, screen_y),
      web_contents->GetBrowserContext());

  return outside;
}

// static
void VivaldiUIEvents::SendKeyboardShortcutEvent(
    SessionID::id_type window_id,
    content::BrowserContext* browser_context,
    const input::NativeWebKeyboardEvent& event,
    bool is_auto_repeat) {
  // We don't allow AltGr keyboard shortcuts
  if (event.GetModifiers() & blink::WebInputEvent::kAltGrKey)
    return;
  // Don't send if event contains only modifiers.
  int key_code = event.windows_key_code;
  if (key_code == ui::VKEY_CONTROL || key_code == ui::VKEY_SHIFT ||
      key_code == ui::VKEY_MENU) {
    return;
  }
  if (event.GetType() == blink::WebInputEvent::Type::kKeyUp)
    return;

  std::string shortcut_text = ::vivaldi::ShortcutTextFromEvent(event);

  // If the event wasn't prevented we'll get a rawKeyDown event. In some
  // exceptional cases we'll never get that, so we let these through
  // unconditionally
  std::vector<std::string> exceptions = {"Up", "Down", "Shift+Delete",
                                         "Meta+Shift+V", "Esc"};
  bool is_exception = std::find(exceptions.begin(), exceptions.end(),
                                shortcut_text) != exceptions.end();
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown ||
      is_exception) {
    ::vivaldi::BroadcastEvent(tabs_private::OnKeyboardShortcut::kEventName,
                              tabs_private::OnKeyboardShortcut::Create(
                                  window_id, shortcut_text, is_auto_repeat,
                                  event.from_devtools),
                              browser_context);
  }
}

// static
void VivaldiUIEvents::SendMouseChangeEvent(
    content::BrowserContext* browser_context,
    bool is_motion) {
  ::vivaldi::BroadcastEvent(tabs_private::OnMouseChanged::kEventName,
                            tabs_private::OnMouseChanged::Create(is_motion),
                            browser_context);
}

// static
void VivaldiUIEvents::InitSingleton() {
  if (!VivaldiEventHooks::HasInstance()) {
    static base::NoDestructor<VivaldiUIEvents> instance;
    VivaldiEventHooks::InitInstance(*instance);
  }
}

}  // namespace extensions
