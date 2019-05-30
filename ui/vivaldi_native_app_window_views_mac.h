// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//

#ifndef UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_MAC_H_
#define UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_MAC_H_

#import <Foundation/Foundation.h>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "ui/vivaldi_fullscreen_menubar_tracker_mac.h"
#include "ui/vivaldi_native_app_window_views.h"

@class VivaldiResizeNotificationObserver;

// Mac-specific parts of VivaldiNativeAppWindowViews.
class VivaldiNativeAppWindowViewsMac : public VivaldiNativeAppWindowViews {
 public:
  VivaldiNativeAppWindowViewsMac();
  ~VivaldiNativeAppWindowViewsMac() override;

  // Called by |nswindow_observer_| for window resize events.
  void OnWindowWillStartLiveResize();
  void OnWindowWillEnterFullScreen();
  void OnWindowWillExitFullScreen();
  void OnWindowDidExitFullScreen();

  void DispatchFullscreenMenubarChangedEvent(bool shown);

 protected:
  // VivaldiNativeAppWindowView implementation.
  void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget) override;
  views::NonClientFrameView* CreateStandardDesktopAppFrame() override;
  views::NonClientFrameView* CreateNonStandardAppFrame() override;

  // ui::BaseWindow implementation.
  bool IsMaximized() const override;
  gfx::Rect GetRestoredBounds() const override;
  void Show() override;
  void ShowInactive() override;
  void Activate() override;
  void Maximize() override;
  void Restore() override;
  void FlashFrame(bool flash) override;

  // WidgetObserver implementation.
  void OnWidgetCreated(views::Widget* widget) override;

  void OnWidgetDestroyed(views::Widget* widget) override;

  // NativeAppWindow implementation.
  // These are used to simulate Mac-style hide/show. Since windows can be hidden
  // and shown using the app.window API, this sets is_hidden_with_app_ to
  // differentiate the reason a window was hidden.
  void ShowWithApp() override;
  void HideWithApp() override;

 private:
  // Unset is_hidden_with_app_ and tell the shim to unhide.
  void UnhideWithoutActivation();

  // Used to notify us about certain NSWindow events.
  base::scoped_nsobject<VivaldiResizeNotificationObserver> nswindow_observer_;

  // The bounds of the window just before it was last maximized.
  NSRect bounds_before_maximize_;

  // Whether this window last became hidden due to a request to hide the entire
  // app, e.g. via the dock menu or Cmd+H. This is set by Hide/ShowWithApp.
  bool is_hidden_with_app_ = false;

  // Set true during an exit fullscreen transition, so that the live resize
  // event AppKit sends can be distinguished from a zoom-triggered live resize.
  bool in_fullscreen_transition_ = false;

  base::scoped_nsobject<VivaldiFullscreenMenubarTracker> menubarTracker_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiNativeAppWindowViewsMac);
};

#endif  // UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_MAC_H_
