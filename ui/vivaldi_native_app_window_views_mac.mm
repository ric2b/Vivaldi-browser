// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//

#import "ui/vivaldi_native_app_window_views_mac.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "chrome/browser/profiles/profile.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/schema/window_private.h"
#include "extensions/tools/vivaldi_tools.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/vivaldi_browser_window.h"
#import "ui/vivaldi_native_app_window_frame_view_mac.h"

// This observer is used to get NSWindow notifications. We need to monitor
// zoom and full screen events to store the correct bounds to Restore() to.
@interface VivaldiResizeNotificationObserver : NSObject {
 @private
  // Weak. Owns us.
  VivaldiNativeAppWindowViewsMac* nativeAppWindow_;
}
- (id)initForNativeAppWindow:(VivaldiNativeAppWindowViewsMac*)nativeAppWindow;
- (void)onWindowWillStartLiveResize:(NSNotification*)notification;
- (void)onWindowWillEnterFullScreen:(NSNotification*)notification;
- (void)onWindowWillExitFullScreen:(NSNotification*)notification;
- (void)onWindowDidExitFullScreen:(NSNotification*)notification;
- (void)stopObserving;
@end

@implementation VivaldiResizeNotificationObserver

- (id)initForNativeAppWindow:(VivaldiNativeAppWindowViewsMac*)nativeAppWindow {
  if ((self = [super init])) {
    nativeAppWindow_ = nativeAppWindow;
    NSWindow* ns_window =
        nativeAppWindow->GetNativeWindow().GetNativeNSWindow();
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
  }
  return self;
}

- (void)onWindowWillStartLiveResize:(NSNotification*)notification {
  nativeAppWindow_->OnWindowWillStartLiveResize();
}

- (void)onWindowWillEnterFullScreen:(NSNotification*)notification {
  nativeAppWindow_->OnWindowWillEnterFullScreen();
}

- (void)onWindowWillExitFullScreen:(NSNotification*)notification {
  nativeAppWindow_->OnWindowWillExitFullScreen();
}

- (void)onWindowDidExitFullScreen:(NSNotification*)notification {
  nativeAppWindow_->OnWindowDidExitFullScreen();
}

- (void)stopObserving {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  nativeAppWindow_ = nullptr;
}

@end

namespace {

// This is copied from
// chromium/chrome/browser/ui/views/apps/app_window_native_widget_mac.h
//
// This implements features specific to vivaldi windows, e.g. frameless windows
// that behave like normal windows.
class VivaldiWindowNativeWidgetMac : public views::NativeWidgetMac {
 public:
  VivaldiWindowNativeWidgetMac(views::Widget* widget, bool is_frameless)
      : NativeWidgetMac(widget), is_frameless_(is_frameless) {}
  ~VivaldiWindowNativeWidgetMac() override {}

 protected:
  // NativeWidgetMac:
  void PopulateCreateWindowParams(
      const views::Widget::InitParams& widget_params,
      remote_cocoa::mojom::CreateWindowParams* params) override {
    if (is_frameless_) {
      params->window_class = remote_cocoa::mojom::WindowClass::kFrameless;
      params->style_mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                           NSWindowStyleMaskMiniaturizable |
                           NSWindowStyleMaskResizable |
                           NSWindowStyleMaskFullSizeContentView;
    }
  }

 private:
  bool is_frameless_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiWindowNativeWidgetMac);
};

bool NSWindowIsMaximized(NSWindow* window) {
  // -[NSWindow isZoomed] only works if the zoom button is enabled.
  if ([[window standardWindowButton:NSWindowZoomButton] isEnabled])
    return [window isZoomed];

  // We don't attempt to distinguish between a window that has been explicitly
  // maximized versus one that has just been dragged by the user to fill the
  // screen. This is the same behavior as -[NSWindow isZoomed] above.
  return NSEqualRects([window frame], [[window screen] visibleFrame]);
}

}  //namespace

// static
std::unique_ptr<VivaldiNativeAppWindowViews>
VivaldiNativeAppWindowViews::Create() {
  return std::make_unique<VivaldiNativeAppWindowViewsMac>();
}

VivaldiNativeAppWindowViewsMac::VivaldiNativeAppWindowViewsMac() {}

VivaldiNativeAppWindowViewsMac::~VivaldiNativeAppWindowViewsMac() {
  [nswindow_observer_ stopObserving];
}

void VivaldiNativeAppWindowViewsMac::OnWindowWillEnterFullScreen() {
    menubarTracker_.reset([[VivaldiFullscreenMenubarTracker alloc]
        initWithVivaldiNativeAppWindow:this]);
}

void VivaldiNativeAppWindowViewsMac::OnWindowWillStartLiveResize() {
  if (!NSWindowIsMaximized(GetNativeWindow().GetNativeNSWindow()) &&
      !in_fullscreen_transition_)
    bounds_before_maximize_ = [GetNativeWindow().GetNativeNSWindow() frame];
}

void VivaldiNativeAppWindowViewsMac::OnWindowWillExitFullScreen() {
  menubarTracker_.reset();
  // Make sure it ends up in hidden state even though we exit fullscreen with
  // the menubar shown.
  DispatchFullscreenMenubarChangedEvent(false);
  in_fullscreen_transition_ = true;
}

void VivaldiNativeAppWindowViewsMac::OnWindowDidExitFullScreen() {
  in_fullscreen_transition_ = false;
}

void VivaldiNativeAppWindowViewsMac::OnBeforeWidgetInit(
    views::Widget::InitParams& init_params) {
  DCHECK(!init_params.native_widget);
  init_params.remove_standard_frame = is_frameless();
  init_params.native_widget =
      new VivaldiWindowNativeWidgetMac(widget(), is_frameless());
}

views::NonClientFrameView*
VivaldiNativeAppWindowViewsMac::CreateNonClientFrameView(
    views::Widget* widget) {
  return new VivaldiNativeAppWindowFrameViewMac(this);
}

bool VivaldiNativeAppWindowViewsMac::IsMaximized() const {
  return GetWidget() && !IsMinimized() && !IsFullscreen() &&
         NSWindowIsMaximized(GetNativeWindow().GetNativeNSWindow());
}

gfx::Rect VivaldiNativeAppWindowViewsMac::GetRestoredBounds() const {
  if (NSWindowIsMaximized(GetNativeWindow().GetNativeNSWindow()))
    return gfx::ScreenRectFromNSRect(bounds_before_maximize_);

  return VivaldiNativeAppWindowViews::GetRestoredBounds();
}

void VivaldiNativeAppWindowViewsMac::Show() {
  [GetNativeWindow().GetNativeNSWindow()
      makeFirstResponder:web_view()
                             ->web_contents()
                             ->GetRenderWidgetHostView()
                             ->GetNativeView()
                             .GetNativeNSView()];
  VivaldiNativeAppWindowViews::Show();
}

void VivaldiNativeAppWindowViewsMac::Maximize() {
  if (IsFullscreen())
    return;

  NSWindow* window = GetNativeWindow().GetNativeNSWindow();
  if (!NSWindowIsMaximized(window))
    [window setFrame:[[window screen] visibleFrame] display:YES animate:YES];

  if (IsMinimized())
    [window deminiaturize:nil];
}

void VivaldiNativeAppWindowViewsMac::Restore() {
  NSWindow* window = GetNativeWindow().GetNativeNSWindow();
  if (NSWindowIsMaximized(window))
    [window setFrame:bounds_before_maximize_ display:YES animate:YES];

  VivaldiNativeAppWindowViews::Restore();
}

void VivaldiNativeAppWindowViewsMac::FlashFrame(bool flash) {}

void VivaldiNativeAppWindowViewsMac::OnWidgetCreated(views::Widget* widget) {
  nswindow_observer_.reset(
      [[VivaldiResizeNotificationObserver alloc] initForNativeAppWindow:this]);
}

void VivaldiNativeAppWindowViewsMac::OnWidgetDestroyed(views::Widget* widget) {
  if (GetWidget() == widget) {
    // We do not reset the widget pointer as it is needed in DeleteDelegate()
    widget->RemoveObserver(this);
    [nswindow_observer_ stopObserving];
    menubarTracker_.reset();
  }
}

void VivaldiNativeAppWindowViewsMac::DeleteDelegate() {
  // Call base class function. For Mac it is necessary that the widget exists
  // at this point. See OnWidgetDestroyed()
  DCHECK(GetWidget());
  VivaldiNativeAppWindowViews::DeleteDelegate();
  // Call base class function with the sole purpose to reset the widget pointer.
  VivaldiNativeAppWindowViews::OnWidgetDestroyed(GetWidget());
}

void VivaldiNativeAppWindowViewsMac::DispatchFullscreenMenubarChangedEvent(
    bool shown) {
  Browser* browser = window()->browser();
  if (!browser) {
    return;
  }
  ::vivaldi::BroadcastEvent(
    extensions::vivaldi::window_private::OnFullscreenMenubarChanged::kEventName,
    extensions::vivaldi::window_private::OnFullscreenMenubarChanged::Create(
        browser->session_id().id(), shown),
    browser->profile());
}