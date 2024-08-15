// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#include "ui/views/vivaldi_native_widget.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "chrome/browser/profiles/profile.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "content/public/browser/browser_thread.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget_observer.h"

#include "extensions/schema/window_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_browser_window.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

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
- (void)onWindowDidEnterFullScreen:(NSNotification*)notification;
- (void)onWindowWillExitFullScreen:(NSNotification*)notification;
- (void)onWindowDidExitFullScreen:(NSNotification*)notification;
- (void)stopObserving;
- (void)onWindowDidChangeScreen:(NSNotification*)notification;
- (void)onWindowDidResize:(NSNotification*)notification;
- (void)onWindowDidDeminiaturize:(NSNotification*)notification;
- (void)onSetControlButtonsPadding:(NSNotification*)notification;
@end

class VivaldiNativeWidgetObserver : public views::WidgetObserver {
 public:
  VivaldiNativeWidgetObserver(VivaldiNativeWidgetMac* native_widget)
      : native_widget_(native_widget) {}

  ~VivaldiNativeWidgetObserver() override = default;

  void OnWidgetThemeChanged(views::Widget* widget) override;

 private:
  // Weak. Owns us.
  raw_ptr<VivaldiNativeWidgetMac> native_widget_;
};

class VivaldiNativeWidgetMac : public views::NativeWidgetMac {
 public:
  VivaldiNativeWidgetMac(VivaldiBrowserWindow* browser_window)
      : NativeWidgetMac(browser_window->GetWidget()),
        browser_window_(browser_window),
        native_widget_observer_(
            std::make_unique<VivaldiNativeWidgetObserver>(this)) {
    browser_window->GetWidget()->AddObserver(native_widget_observer_.get());
  }

  ~VivaldiNativeWidgetMac() override {}
  VivaldiNativeWidgetMac(const VivaldiNativeWidgetMac&) = delete;
  VivaldiNativeWidgetMac& operator=(const VivaldiNativeWidgetMac&) = delete;

  void OnWindowWillStartLiveResize();
  void OnWindowWillEnterFullScreen();
  void OnWindowDidEnterFullScreen();
  void OnWindowWillExitFullScreen();
  void OnWindowDidExitFullScreen();
  void OnWindowDidChangeScreen(bool hasNotch);
  bool SetWindowTitle(const std::u16string& title) override;
  enum class TrafficLightPosition {
    Native,
    LightPadding,
    MediumPadding,
    HeavyPadding,
  };
  void SetTrafficLightPosition(TrafficLightPosition position);
  void RedrawTrafficLight();

 private:
  // NativeWidgetMac overrdies

  void PopulateCreateWindowParams(
      const views::Widget::InitParams& widget_params,
      remote_cocoa::mojom::CreateWindowParams* params) override;

  void OnWindowInitialized() override;
  void OnWindowDestroying(gfx::NativeWindow window) override;
  bool IsMaximized() const override;
  void Maximize() override;
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

  std::unique_ptr<VivaldiNativeWidgetObserver> native_widget_observer_;
  bool titlebar_appears_transparent_ = false;
  std::optional<gfx::Point> traffic_light_position_;
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
           selector:@selector(onWindowDidEnterFullScreen:)
               name:NSWindowDidEnterFullScreenNotification
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
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onWindowDidResize:)
               name:NSWindowDidResizeNotification
             object:ns_window];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onWindowDidDeminiaturize:)
               name:NSWindowDidDeminiaturizeNotification
             object:ns_window];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onSetControlButtonsPadding:)
               name:@"VivaldiSetControlButtonsPadding"
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

- (void)onWindowDidEnterFullScreen:(NSNotification*)notification {
  native_widget_->OnWindowDidEnterFullScreen();
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
  NSWindow* window = (NSWindow*)[notification object];
  bool screen_has_notch = ScreenHasNotch([window screen]);
  native_widget_->OnWindowDidChangeScreen(screen_has_notch);
}

- (void)onWindowDidResize:(NSNotification*)notification {
  native_widget_->RedrawTrafficLight();
}

- (void)onWindowDidDeminiaturize:(NSNotification*)notification {
  native_widget_->RedrawTrafficLight();
}

- (void)onSetControlButtonsPadding:(NSNotification*)notification {
  NSDictionary* userInfo = notification.userInfo;
  NSString* padding = userInfo[@"padding"];

  VivaldiNativeWidgetMac::TrafficLightPosition position =
      VivaldiNativeWidgetMac::TrafficLightPosition::Native;
  if ([padding isEqual:@"lightPadding"]) {
    position = VivaldiNativeWidgetMac::TrafficLightPosition::LightPadding;
  } else if ([padding isEqual:@"mediumPadding"]) {
    position = VivaldiNativeWidgetMac::TrafficLightPosition::MediumPadding;
  } else if ([padding isEqual:@"heavyPadding"]) {
    position = VivaldiNativeWidgetMac::TrafficLightPosition::HeavyPadding;
  }
  native_widget_->SetTrafficLightPosition(position);
}

@end

void VivaldiNativeWidgetMac::PopulateCreateWindowParams(
    const views::Widget::InitParams& widget_params,
    remote_cocoa::mojom::CreateWindowParams* params) {
  if (!browser_window_->with_native_frame()) {
    params->animation_enabled = true;
    params->titlebar_appears_transparent = titlebar_appears_transparent_ = true;
    params->window_title_hidden = true;
    params->window_class = remote_cocoa::mojom::WindowClass::kBrowser;
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
  if (nswindow_observer_) {
    [nswindow_observer_ stopObserving];
    // TODO: Test properly for problems wrt ARC transition
    nswindow_observer_ = nullptr;
  }
  views::NativeWidgetMac::OnWindowDestroying(window);
}

void VivaldiNativeWidgetMac::OnWindowWillStartLiveResize() {
  if (!NSWindowIsMaximized(GetNativeWindow().GetNativeNSWindow()) &&
      !in_fullscreen_transition_)
    bounds_before_maximize_ = [GetNativeWindow().GetNativeNSWindow() frame];
}

void VivaldiNativeWidgetMac::OnWindowWillEnterFullScreen() {
  in_fullscreen_transition_ = true;
  RedrawTrafficLight();
}

void VivaldiNativeWidgetMac::OnWindowDidEnterFullScreen() {
  in_fullscreen_transition_ = false;
  RedrawTrafficLight();
}

void VivaldiNativeWidgetMac::OnWindowWillExitFullScreen() {
  in_fullscreen_transition_ = true;
  RedrawTrafficLight();
}

void VivaldiNativeWidgetMac::OnWindowDidExitFullScreen() {
  in_fullscreen_transition_ = false;
  RedrawTrafficLight();
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

void VivaldiNativeWidgetMac::Restore() {
  NSWindow* ns_window = GetNativeWindow().GetNativeNSWindow();
  if (NSWindowIsMaximized(ns_window)) {
    [ns_window setFrame:bounds_before_maximize_ display:YES animate:YES];
  }

  NativeWidgetMac::Restore();
}

bool VivaldiNativeWidgetMac::SetWindowTitle(const std::u16string& title) {
  bool ret = views::NativeWidgetMac::SetWindowTitle(title);
  RedrawTrafficLight();
  return ret;
}

void VivaldiNativeWidgetMac::SetTrafficLightPosition(
    TrafficLightPosition position) {
  switch (position) {
    case TrafficLightPosition::Native:
      traffic_light_position_ = {20, 12};
      break;
    case TrafficLightPosition::LightPadding:
      traffic_light_position_ = {7, 12};
      break;
    case TrafficLightPosition::MediumPadding:
      traffic_light_position_ = {12, 25};
      break;
    case TrafficLightPosition::HeavyPadding:
      traffic_light_position_ = {20, 35};
      break;
  }

  RedrawTrafficLight();
}

void VivaldiNativeWidgetMac::RedrawTrafficLight() {
  if (!titlebar_appears_transparent_)
    return;

  NSWindow* window = GetNativeWindow().GetNativeNSWindow();
  NSButton* close = [window standardWindowButton:NSWindowCloseButton];
  NSButton* miniaturize =
      [window standardWindowButton:NSWindowMiniaturizeButton];
  NSButton* zoom = [window standardWindowButton:NSWindowZoomButton];
  // Safety check just in case apple changes the view structure in a macOS
  // update
  DCHECK(close.superview);
  DCHECK(close.superview.superview);
  if (!close.superview || !close.superview.superview)
    return;
  NSView* titleBarContainerView = close.superview.superview;

  if (in_fullscreen_transition_ || !traffic_light_position_) {
    [titleBarContainerView setHidden:YES];
    return;
  }
  [titleBarContainerView setHidden:NO];
  CGFloat buttonHeight = [close frame].size.height;
  CGFloat titleBarFrameHeight = buttonHeight + traffic_light_position_->y();
  CGRect titleBarRect = titleBarContainerView.frame;
  CGFloat titleBarWidth = NSWidth(titleBarRect);
  titleBarRect.size.height = titleBarFrameHeight;
  titleBarRect.origin.y = window.frame.size.height - titleBarFrameHeight;
  [titleBarContainerView setFrame:titleBarRect];

  BOOL isRTL = [titleBarContainerView userInterfaceLayoutDirection] ==
               NSUserInterfaceLayoutDirectionRightToLeft;
  NSArray* windowButtons = @[ close, miniaturize, zoom ];
  const CGFloat space_between =
      [miniaturize frame].origin.x - [close frame].origin.x;
  for (NSUInteger i = 0; i < windowButtons.count; i++) {
    NSView* view = [windowButtons objectAtIndex:i];
    CGRect rect = [view frame];
    if (isRTL) {
      CGFloat buttonWidth = NSWidth(rect);
      // origin is always top-left, even in RTL
      rect.origin.x = titleBarWidth - traffic_light_position_->x() +
                      (i * space_between) - buttonWidth;
    } else {
      rect.origin.x = traffic_light_position_->x() + (i * space_between);
    }
    rect.origin.y = (titleBarFrameHeight - rect.size.height) / 2;
    [view setFrameOrigin:rect.origin];
  }
}

std::unique_ptr<views::NativeWidget> CreateVivaldiNativeWidget(
    VivaldiBrowserWindow* window) {
  return std::make_unique<VivaldiNativeWidgetMac>(window);
}

void VivaldiNativeWidgetObserver::OnWidgetThemeChanged(views::Widget*) {
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&VivaldiNativeWidgetMac::RedrawTrafficLight,
                                base::Unretained(native_widget_)));
}
