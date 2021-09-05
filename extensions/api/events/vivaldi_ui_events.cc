// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/events/vivaldi_ui_events.h"

#include "app/vivaldi_apptools.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/prefs/pref_service.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"  // nogncheck
#include "content/public/browser/native_web_keyboard_event.h"
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

struct MouseGestures {
  // To avoid depending on platform's focus policy store the id of the window
  // where the gesture was initiated and send the gesture events towards it and
  // not to the focused window, see VB-47721. Similarly, pass the initial
  // pointer coordinates relative to root to apply the gesture to the tab over
  // which the gesture has started, see VB-48232.
  SessionID window_id = SessionID::InvalidValue();
  gfx::PointF initial_client_pos;

  // Gesture started with the Alt key
  bool with_alt = false;

  bool recording = false;
  float last_x = 0.0f;
  float last_y = 0.0f;
  float min_move_squared = 0.0f;

  // The string of uniqe gesture directions that is send to JS.
  std::string directions;
  int last_direction = -1;
};

struct WheelGestures {
  bool active = 0;
  SessionID window_id = SessionID::InvalidValue();
};

struct RockerGestures {
  bool eat_next_left_mouseup = false;
  bool eat_next_right_mouseup = false;
};

class GestureState {
 public:
  void StartMouseGestureDetection(content::WebContents* web_contents,
                                  SessionID window_id,
                                  const blink::WebMouseEvent& mouse_event,
                                  bool with_alt);
  bool FinishMouseOrWheelGesture(bool with_alt);
  bool CheckMouseGesture(content::WebContents* web_contents,
                         SessionID window_id,
                         const blink::WebMouseEvent& mouse_event);
  bool CheckRockerGesture(content::WebContents* web_contents,
                          SessionID window_id,
                          const blink::WebMouseEvent& mouse_event);
  bool HandleMouseGestureMove(const blink::WebMouseEvent& mouse_event,
                              content::WebContents* web_contents);
  void CheckWebviewClick(content::WebContents* web_contents,
                         SessionID window_id,
                         const blink::WebMouseEvent& mouse_event);

 private:
  friend class ::extensions::VivaldiUIEvents;
  std::unique_ptr<MouseGestures> mouse_gestures_;
  WheelGestures wheel_gestures_;
  RockerGestures rocker_gestures_;
};

GestureState& GetGestureState() {
  static base::NoDestructor<GestureState> instance;
  return *instance;
}

// Helper to send the event using BrowserContext corresponding to to the gven
// window_id.
void SendEventToUI(SessionID window_id,
                   const std::string& eventname,
                   std::unique_ptr<base::ListValue> args) {
  Browser* browser = chrome::FindBrowserWithID(window_id);
  if (!browser)
    return;
  VivaldiBrowserWindow* window = VivaldiBrowserWindow::FromBrowser(browser);
  if (!window || !window->web_contents())
    return;

  // TODO(igor@vivaldi.com): This broadcats the event to all windows and
  // extensions forcing our JS code to check in each window if it matches
  // the window id. Find a way to send this only to Vivaldi JS
  // in a specific window.
  ::vivaldi::BroadcastEvent(eventname, std::move(args),
                            window->web_contents()->GetBrowserContext());
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

void GestureState::StartMouseGestureDetection(
    content::WebContents* web_contents,
    SessionID window_id,
    const blink::WebMouseEvent& mouse_event,
    bool with_alt) {
  DCHECK(!mouse_gestures_);

  // Ignore any gesture after the wheel scroll with the Alt key or right button
  // pressed but before the key or button was released.
  if (wheel_gestures_.active)
    return;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (with_alt) {
    if (!profile->GetPrefs()->GetBoolean(
            vivaldiprefs::kMouseGesturesAltGesturesEnabled))
      return;
  } else {
    if (!profile->GetPrefs()->GetBoolean(vivaldiprefs::kMouseGesturesEnabled))
      return;
  }

  mouse_gestures_ = std::make_unique<MouseGestures>();
  mouse_gestures_->window_id = window_id;
  mouse_gestures_->initial_client_pos =
      ::vivaldi::ToUICoordinates(web_contents, mouse_event.PositionInWidget());
  mouse_gestures_->with_alt = with_alt;
  mouse_gestures_->last_x = mouse_event.PositionInScreen().x();
  mouse_gestures_->last_y = mouse_event.PositionInScreen().y();

  SendEventToUI(window_id, tabs_private::OnMouseGestureDetection::kEventName,
                tabs_private::OnMouseGestureDetection::Create(window_id.id()));
}

bool GestureState::HandleMouseGestureMove(
    const blink::WebMouseEvent& mouse_event,
    content::WebContents* web_contents) {
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
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    float tolerance = static_cast<float>(profile->GetPrefs()->GetDouble(
        vivaldiprefs::kMouseGesturesStrokeTolerance));
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

bool GestureState::FinishMouseOrWheelGesture(bool with_alt) {
  bool after_gesture = false;
  if (wheel_gestures_.active) {
    DCHECK(!mouse_gestures_);
    after_gesture = true;
    SessionID window_id = wheel_gestures_.window_id;
    SendEventToUI(window_id, tabs_private::OnTabSwitchEnd::kEventName,
                  tabs_private::OnTabSwitchEnd::Create(window_id.id()));
    wheel_gestures_.active = false;
    wheel_gestures_.window_id = SessionID::InvalidValue();
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

    gfx::PointF p = mouse_gestures_->initial_client_pos;
    SessionID window_id = mouse_gestures_->window_id;
    SendEventToUI(
        window_id, tabs_private::OnMouseGesture::kEventName,
        tabs_private::OnMouseGesture::Create(window_id.id(), p.x(), p.y(),
                                             mouse_gestures_->directions));
  }
  mouse_gestures_.reset();
  return after_gesture;
}

bool GestureState::CheckMouseGesture(content::WebContents* web_contents,
                                     SessionID window_id,
                                     const blink::WebMouseEvent& mouse_event) {
  using blink::WebInputEvent;
  using blink::WebMouseEvent;
  using blink::WebMouseWheelEvent;

  bool eat_event = false;
  // We should not have both wheel and mouse gestures running.
  DCHECK(!wheel_gestures_.active || !mouse_gestures_);
  switch (mouse_event.GetType()) {
    default:
      break;
    case WebInputEvent::Type::kMouseDown: {
      if (!mouse_gestures_) {
        if (mouse_event.button == WebMouseEvent::Button::kRight &&
            !(mouse_event.GetModifiers() & WebInputEvent::kLeftButtonDown)) {
          StartMouseGestureDetection(web_contents, window_id, mouse_event,
                                     false);
        }
      }
      break;
    }
    case WebInputEvent::Type::kMouseMove: {
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
          StartMouseGestureDetection(web_contents, window_id, mouse_event,
                                     with_alt);
        }
        break;
      }
      bool gesture = false;
      if (mouse_gestures_->with_alt) {
        gesture = IsGestureAltMouseMove(mouse_event);
      } else {
        gesture = IsGestureMouseMove(mouse_event);
      }
      if (gesture) {
        eat_event = HandleMouseGestureMove(mouse_event, web_contents);
        break;
      }
      // This happens when the right mouse button is released outside of webview
      // or the alt key was released when the window lost input focus.
      mouse_gestures_.reset();
      break;
    }
    case WebInputEvent::Type::kMouseUp: {
      eat_event = FinishMouseOrWheelGesture(false);
      break;
    }
  }
  return eat_event;
}

bool GestureState::CheckRockerGesture(content::WebContents* web_contents,
                                      SessionID window_id,
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
      Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext());
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
            window_id, tabs_private::OnRockerGesture::kEventName,
            tabs_private::OnRockerGesture::Create(window_id.id(), is_left));
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
void GestureState::CheckWebviewClick(content::WebContents* web_contents,
                                     SessionID window_id,
                                     const blink::WebMouseEvent& mouse_event) {
  using blink::WebInputEvent;
  using Button = blink::WebPointerProperties::Button;
  WebInputEvent::Type type = mouse_event.GetType();
  if (type != WebInputEvent::Type::kMouseDown &&
      type != WebInputEvent::Type::kMouseUp)
    return;

  bool mousedown = (type == WebInputEvent::Type::kMouseDown);
  int button = 0;
  if (mouse_event.button == Button::kMiddle) {
    button = 1;
  } else if (mouse_event.button == Button::kRight) {
    button = 2;
  }
  gfx::PointF p =
      ::vivaldi::ToUICoordinates(web_contents, mouse_event.PositionInWidget());
  SendEventToUI(window_id, tabs_private::OnWebviewClickCheck::kEventName,
                tabs_private::OnWebviewClickCheck::Create(
                    window_id.id(), mousedown, button, p.x(), p.y()));
}

}  // namespace

VivaldiUIEvents::VivaldiUIEvents(content::WebContents* web_contents,
                                 SessionID window_id)
    : web_contents_(web_contents), window_id_(window_id) {}

bool VivaldiUIEvents::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  bool down = false;
  bool after_gesture = false;
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown) {
    down = true;
  } else if (event.GetType() == blink::WebInputEvent::Type::kKeyUp) {
    // Check for Alt aka Menu release
    if (event.windows_key_code == blink::VKEY_MENU) {
      GestureState& state = GetGestureState();
      after_gesture = state.FinishMouseOrWheelGesture(true);
    }
  } else {
    return false;
  }

  // We only need the four lowest bits for cmd, alt, ctrl, shift
  int modifiers = event.GetModifiers() & 15;

  SendEventToUI(window_id_, tabs_private::OnKeyboardChanged::kEventName,
                tabs_private::OnKeyboardChanged::Create(
                    window_id_.id(), down, modifiers, event.windows_key_code,
                    after_gesture));

  return after_gesture;
}

bool VivaldiUIEvents::HandleMouseEvent(
    content::RenderWidgetHostViewBase* root_view,
    const blink::WebMouseEvent& event) {
  bool eat_event = false;
  bool is_blocked = false;
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_);
  if (manager) {
    // Avoid dangling modal dialogs that will crash if the tab is closed
    // through a gesture.
    // TODO(pettern): There is still a chance a tab will be able to close
    // before this check, so investigate blocking on the api level after
    // the tab close rewrite on the js side.
    is_blocked = manager->IsDialogActive();
  }
  if (!is_blocked) {
    GestureState& state = GetGestureState();

    // Rocker gestures take priority over any other mouse gestures.
    eat_event = state.CheckRockerGesture(web_contents_, window_id_, event);
    if (!eat_event) {
      eat_event = state.CheckMouseGesture(web_contents_, window_id_, event);
      if (!eat_event) {
        state.CheckWebviewClick(web_contents_, window_id_, event);
      }

      Browser* browser =
          ::vivaldi::FindBrowserForEmbedderWebContents(web_contents_);
      content::WebContents* activecontents;
      // browser is null for devtools
      if (browser && (activecontents =
                          browser->tab_strip_model()->GetActiveWebContents())) {
        content::NavigationController& controller =
            activecontents->GetController();
        if (event.GetType() == blink::WebInputEvent::Type::kMouseUp) {
          if (event.button == blink::WebPointerProperties::Button::kBack &&
              controller.CanGoBack()) {
            controller.GoBack();
            eat_event = true;
          } else if (event.button ==
                         blink::WebPointerProperties::Button::kForward &&
                     controller.CanGoForward()) {
            controller.GoForward();
            eat_event = true;
          }
        }
      }
    }
  }
  return eat_event;
}

bool VivaldiUIEvents::HandleWheelEvent(
    content::RenderWidgetHostViewBase* root_view,
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

  GestureState& state = GetGestureState();

  // We should not have both wheel and mouse gestures running.
  DCHECK(!state.wheel_gestures_.active || !state.mouse_gestures_);

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!profile->GetPrefs()->GetBoolean(vivaldiprefs::kMouseWheelTabSwitch))
    return false;

  if (!state.wheel_gestures_.active) {
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
      state.mouse_gestures_.reset();
      state.wheel_gestures_.active = true;
      state.wheel_gestures_.window_id = window_id_;
    }
  }
  root_view->ProcessMouseWheelEvent(wheel_event, latency);
  return true;
}

bool VivaldiUIEvents::HandleWheelEventAfterChild(
    content::RenderWidgetHostViewBase* root_view,
    content::RenderWidgetHostViewBase* child_view,
    const blink::WebMouseWheelEvent& event) {
  using blink::WebInputEvent;
  using blink::WebMouseWheelEvent;
  constexpr int zoom_modifier =
#if defined(OS_MAC)
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

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
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

  gfx::PointF p = event.PositionInWidget();
  if (child_view) {
    p = child_view->TransformPointToRootCoordSpaceF(p);
  }
  p = ::vivaldi::ToUICoordinates(web_contents_, p);
  SendEventToUI(
      window_id_, tabs_private::OnPageZoom::kEventName,
      tabs_private::OnPageZoom::Create(window_id_.id(), steps, p.x(), p.y()));

  return true;
}

bool VivaldiUIEvents::HandleDragEnd(blink::WebDragOperation operation,
                                    bool cancelled,
                                    int screen_x,
                                    int screen_y) {
  if (!::vivaldi::IsTabDragInProgress())
    return false;
  ::vivaldi::SetTabDragInProgress(false);

  bool outside = ::vivaldi::ui_tools::IsOutsideAppWindow(screen_x, screen_y);
  if (!outside && operation == blink::WebDragOperation::kWebDragOperationNone) {
    // None of browser windows accepted the drag and we do not moving tabs out.
    cancelled = true;
  }

  ::vivaldi::BroadcastEvent(
      tabs_private::OnDragEnd::kEventName,
      tabs_private::OnDragEnd::Create(cancelled, outside, screen_x, screen_y),
      web_contents_->GetBrowserContext());

  return outside;
}

// static
void VivaldiUIEvents::SendKeyboardShortcutEvent(
    content::BrowserContext* browser_context,
    const content::NativeWebKeyboardEvent& event,
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
    ::vivaldi::BroadcastEvent(
        tabs_private::OnKeyboardShortcut::kEventName,
        tabs_private::OnKeyboardShortcut::Create(shortcut_text, is_auto_repeat),
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
void VivaldiUIEvents::SetupWindowContents(content::WebContents* web_contents,
                                          SessionID window_id) {
  DCHECK(!web_contents->GetUserData(VivaldiEventHooks::UserDataKey()));
  web_contents->SetUserData(
      VivaldiEventHooks::UserDataKey(),
      std::make_unique<VivaldiUIEvents>(web_contents, window_id));
}

}  // namespace extensions
