// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#include "ui/views/vivaldi_native_widget.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "chrome/browser/profiles/profile.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/widget/native_widget_mac.h"

#include "extensions/schema/window_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_browser_window.h"

class VivaldiNativeWidgetMac;

// This observer is used to get NSWindow notifications. We need to monitor
// zoom and full screen events to store the correct bounds to Restore() to.
@interface VivaldiResizeNotificationObserver : NSObject {
 @private
  // Weak. Owns us.
  VivaldiNativeWidgetMac* native_widget_;
}
- (id)initForNativeWidget:(VivaldiNativeWidgetMac*)native_widget;
- (void)onWindowWillStartLiveResize:(NSNotification*)notification;
- (void)onWindowWillEnterFullScreen:(NSNotification*)notification;
- (void)onWindowWillExitFullScreen:(NSNotification*)notification;
- (void)onWindowDidExitFullScreen:(NSNotification*)notification;
- (void)stopObserving;
- (void)onWindowDidChangeScreen:(NSNotification*)notification;
@end

class VivaldiNativeWidgetMac : public views::NativeWidgetMac {
 public:
  VivaldiNativeWidgetMac(VivaldiBrowserWindow* browser_window)
      : NativeWidgetMac(browser_window->GetWidget()),
        browser_window_(browser_window) {
    DCHECK(browser_window);
  }
  ~VivaldiNativeWidgetMac() override {}
  VivaldiNativeWidgetMac(const VivaldiNativeWidgetMac&) = delete;
  VivaldiNativeWidgetMac& operator=(const VivaldiNativeWidgetMac&) = delete;

  void OnWindowWillStartLiveResize();
  void OnWindowWillEnterFullScreen();
  void OnWindowWillExitFullScreen();
  void OnWindowDidExitFullScreen();
  void OnWindowDidChangeScreen(bool hasNotch);

 private:
  static OSStatus MenuBarChangedHandler(EventHandlerCallRef handler,
                                        EventRef event,
                                        void* context);

  void FinishMenubarTracker();

  void DispatchFullscreenMenubarChangedEvent(bool shown);

  // NativeWidgetMac overrdies

  void PopulateCreateWindowParams(
      const views::Widget::InitParams& widget_params,
      remote_cocoa::mojom::CreateWindowParams* params) override;

  void OnWindowInitialized() override;
  void OnWindowDestroying(gfx::NativeWindow window) override;
  bool IsMaximized() const override;
  void Maximize() override;
  gfx::Rect GetRestoredBounds() const override;
  void Restore() override;

  // Indirect owner
  raw_ptr<VivaldiBrowserWindow> browser_window_;

  // Used to notify us about certain NSWindow events.
  VivaldiResizeNotificationObserver* __strong nswindow_observer_;

  // The bounds of the window just before it was last maximized.
  NSRect bounds_before_maximize_;

  // Set true during an exit fullscreen transition, so that the live resize
  // event AppKit sends can be distinguished from a zoom-triggered live resize.
  bool in_fullscreen_transition_ = false;

  EventHandlerRef menubar_tracking_handler_ = nullptr;
};

namespace {

bool NSWindowIsMaximized(NSWindow* ns_window) {
  // -[NSWindow isZoomed] only works if the zoom button is enabled.
  if ([[ns_window standardWindowButton:NSWindowZoomButton] isEnabled])
    return [ns_window isZoomed];

  // We don't attempt to distinguish between a window that has been explicitly
  // maximized versus one that has just been dragged by the user to fill the
  // screen. This is the same behavior as -[NSWindow isZoomed] above.
  return NSEqualRects([ns_window frame], [[ns_window screen] visibleFrame]);
}

// Screen notch detection functions.
bool ScreenHasNotch(NSScreen* screen) {
  if (@available(macos 12.0.1, *)) {
    NSEdgeInsets insets = [screen safeAreaInsets];
    if (insets.top != 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

@implementation VivaldiResizeNotificationObserver

- (id)initForNativeWidget:(VivaldiNativeWidgetMac*)native_widget {
  if ((self = [super init])) {
    native_widget_ = native_widget;
    NSWindow* ns_window = native_widget->GetNativeWindow().GetNativeNSWindow();
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onWindowWillStartLiveResize:)
               name:NSWindowWillStartLiveResizeNotification
             object:ns_window];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onWindowWillEnterFullScreen:)
               name:NSWindowWillEnterFullScreenNotification
             object:ns_window];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onWindowWillExitFullScreen:)
               name:NSWindowWillExitFullScreenNotification
             object:ns_window];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onWindowDidExitFullScreen:)
               name:NSWindowDidExitFullScreenNotification
             object:ns_window];
    [[NSNotificationCenter defaultCenter]
       addObserver:self
           selector:@selector(onWindowDidChangeScreen:)
               name:NSWindowDidChangeScreenNotification
             object:ns_window];
  }
  return self;
}

- (void)onWindowWillStartLiveResize:(NSNotification*)notification {
  native_widget_->OnWindowWillStartLiveResize();
}

- (void)onWindowWillEnterFullScreen:(NSNotification*)notification {
  native_widget_->OnWindowWillEnterFullScreen();
}

- (void)onWindowWillExitFullScreen:(NSNotification*)notification {
  native_widget_->OnWindowWillExitFullScreen();
}

- (void)onWindowDidExitFullScreen:(NSNotification*)notification {
  native_widget_->OnWindowDidExitFullScreen();
}

- (void)stopObserving {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  native_widget_ = nullptr;
}

- (void)onWindowDidChangeScreen:(NSNotification*)notification {
  NSWindow* window = (NSWindow*) [notification object];
  bool screen_has_notch = ScreenHasNotch([window screen]);
  native_widget_->OnWindowDidChangeScreen(screen_has_notch);
}

@end

// static
OSStatus VivaldiNativeWidgetMac::MenuBarChangedHandler(
    EventHandlerCallRef handler,
    EventRef event,
    void* context) {
  do {
    bool shown;
    if (GetEventKind(event) == kEventMenuBarShown) {
      shown = true;
    } else if (GetEventKind(event) == kEventMenuBarHidden) {
      shown = false;
    } else {
      break;
    }

    auto* native_widget = static_cast<VivaldiNativeWidgetMac*>(context);
    NSWindow* ns_window = native_widget->GetNativeWindow().GetNativeNSWindow();
    if (!ns_window)
      break;

    if (![ns_window isOnActiveSpace])
      break;

    bool mouse_on_screen =
        NSMouseInRect([NSEvent mouseLocation], [ns_window screen].frame, false);
    if (!mouse_on_screen)
      break;

    native_widget->DispatchFullscreenMenubarChangedEvent(shown);
  } while (false);

  return CallNextEventHandler(handler, event);
}

void VivaldiNativeWidgetMac::PopulateCreateWindowParams(
    const views::Widget::InitParams& widget_params,
    remote_cocoa::mojom::CreateWindowParams* params) {
  if (!browser_window_->with_native_frame()) {
    params->titlebar_appears_transparent = true;
    params->window_class = remote_cocoa::mojom::WindowClass::kFrameless;
    params->style_mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                         NSWindowStyleMaskMiniaturizable |
                         NSWindowStyleMaskResizable |
                         NSWindowStyleMaskFullSizeContentView;
  }
}

void VivaldiNativeWidgetMac::OnWindowInitialized() {
  views::NativeWidgetMac::OnWindowInitialized();
  // TODO: Test properly for problems wrt ARC transition
  nswindow_observer_ =
      [[VivaldiResizeNotificationObserver alloc] initForNativeWidget:this];
}

void VivaldiNativeWidgetMac::OnWindowDestroying(gfx::NativeWindow window) {
  browser_window_ = nullptr;
  FinishMenubarTracker();
  if (nswindow_observer_) {
    [nswindow_observer_ stopObserving];
    // TODO: Test properly for problems wrt ARC transition
    nswindow_observer_ = nullptr;
  }
  views::NativeWidgetMac::OnWindowDestroying(window);
}

void VivaldiNativeWidgetMac::FinishMenubarTracker() {
  if (!menubar_tracking_handler_)
    return;
  RemoveEventHandler(menubar_tracking_handler_);
  menubar_tracking_handler_ = nullptr;
}

void VivaldiNativeWidgetMac::OnWindowWillEnterFullScreen() {
  // Install the Carbon event handler for the menubar show, hide and
  // undocumented reveal event.
  FinishMenubarTracker();

  EventTypeSpec eventSpecs[2];

  eventSpecs[0].eventClass = kEventClassMenu;
  eventSpecs[0].eventKind = kEventMenuBarShown;

  eventSpecs[1].eventClass = kEventClassMenu;
  eventSpecs[1].eventKind = kEventMenuBarHidden;

  InstallApplicationEventHandler(NewEventHandlerUPP(&MenuBarChangedHandler),
                                 std::size(eventSpecs), eventSpecs, this,
                                 &menubar_tracking_handler_);
}

void VivaldiNativeWidgetMac::OnWindowWillStartLiveResize() {
  if (!NSWindowIsMaximized(GetNativeWindow().GetNativeNSWindow()) &&
      !in_fullscreen_transition_)
    bounds_before_maximize_ = [GetNativeWindow().GetNativeNSWindow() frame];
}

void VivaldiNativeWidgetMac::OnWindowWillExitFullScreen() {
  FinishMenubarTracker();

  // Make sure it ends up in hidden state even though we exit fullscreen with
  // the menubar shown.
  DispatchFullscreenMenubarChangedEvent(false);
  in_fullscreen_transition_ = true;
}

void VivaldiNativeWidgetMac::OnWindowDidChangeScreen(bool hasNotch) {
  if (!browser_window_)
    return;
  Browser* browser = browser_window_->browser();
  if (!browser)
    return;
  ::vivaldi::BroadcastEvent(
    extensions::vivaldi::window_private::OnWindowDidChangeScreens::kEventName,
    extensions::vivaldi::window_private::OnWindowDidChangeScreens::Create(
      browser->session_id().id(), hasNotch),
    browser->profile());}

void VivaldiNativeWidgetMac::OnWindowDidExitFullScreen() {
  in_fullscreen_transition_ = false;
}

void VivaldiNativeWidgetMac::DispatchFullscreenMenubarChangedEvent(bool shown) {
  if (!browser_window_)
    return;
  Browser* browser = browser_window_->browser();
  if (!browser)
    return;
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::window_private::OnFullscreenMenubarChanged::
          kEventName,
      extensions::vivaldi::window_private::OnFullscreenMenubarChanged::Create(
          browser->session_id().id(), shown),
      browser->profile());
}

// The method in the parent class always returns false
bool VivaldiNativeWidgetMac::IsMaximized() const {
  return !IsMinimized() && !IsFullscreen() &&
         NSWindowIsMaximized(GetNativeWindow().GetNativeNSWindow());
}

// The method in the parent class calls NOTIMPLEMENTED()
void VivaldiNativeWidgetMac::Maximize() {
  if (IsFullscreen())
    return;

  NSWindow* ns_window = GetNativeWindow().GetNativeNSWindow();
  if (!NSWindowIsMaximized(ns_window)) {
    [ns_window setFrame:[[ns_window screen] visibleFrame]
                display:YES
                animate:YES];
  }

  if (IsMinimized()) {
    [ns_window deminiaturize:nil];
  }
}

gfx::Rect VivaldiNativeWidgetMac::GetRestoredBounds() const {
  NSWindow* ns_window = GetNativeWindow().GetNativeNSWindow();
  if (NSWindowIsMaximized(ns_window))
    return gfx::ScreenRectFromNSRect(bounds_before_maximize_);

  return NativeWidgetMac::GetRestoredBounds();
}

void VivaldiNativeWidgetMac::Restore() {
  NSWindow* ns_window = GetNativeWindow().GetNativeNSWindow();
  if (NSWindowIsMaximized(ns_window)) {
    [ns_window setFrame:bounds_before_maximize_ display:YES animate:YES];
  }

  NativeWidgetMac::Restore();
}

std::unique_ptr<views::NativeWidget> CreateVivaldiNativeWidget(
    VivaldiBrowserWindow* window) {
  return std::make_unique<VivaldiNativeWidgetMac>(window);
}
