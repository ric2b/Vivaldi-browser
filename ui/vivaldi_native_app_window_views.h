// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_H_
#define UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/app_window/size_constraints.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_family.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/client_view.h"
#include "ui/vivaldi_native_app_window.h"

class SkRegion;
class VivaldiBrowserWindow;
class ExtensionKeybindingRegistryViews;

namespace content {
class RenderViewHost;
}

namespace views {
class WebView;
}

// Make sure we answer correctly for ClientView::CanClose to make sure the exit
// sequence is started when closing a BrowserWindow. See comment in
// fast_unload_controller.h.
class VivaldiAppWindowClientView : public views::ClientView {
 public:
  VivaldiAppWindowClientView(views::Widget* widget, views::View *contents_view,
                             VivaldiBrowserWindow *browser_window)
      : views::ClientView(widget, contents_view),
      browser_window_(browser_window) {}

  // views::ClientView:
  bool CanClose() override;

 private:
  VivaldiBrowserWindow* browser_window_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiAppWindowClientView);
};

// A VivaldiNativeAppWindow backed by a views::Widget and is based on the
// NativeAppWindow, but decoupled from AppWindow. This is a merge of
// NativeAppWindowViews and ChromeNativeAppWindowViews.
class VivaldiNativeAppWindowViews : public VivaldiNativeAppWindow,
                                    public views::WidgetDelegateView,
                                    public views::WidgetObserver {
 public:
  VivaldiNativeAppWindowViews();
  ~VivaldiNativeAppWindowViews() override;

  void Init(VivaldiBrowserWindow* window,
            const extensions::AppWindow::CreateParams& create_params);

  // Signal that CanHaveTransparentBackground has changed.
  void OnCanHaveAlphaEnabledChanged();

  VivaldiBrowserWindow* window() { return window_; }
  const VivaldiBrowserWindow* window() const { return window_; }

  views::WebView* web_view() { return web_view_; }
  const views::WebView* web_view() const { return web_view_; }

  views::Widget* widget() { return widget_; }
  const views::Widget* widget() const { return widget_; }

  SkRegion* shape() { return shape_.get(); }
  ShapeRects* shape_rects() { return shape_rects_.get(); }

 protected:
  // Called before views::Widget::Init() in InitializeDefaultWindow() to allow
  // subclasses to customize the InitParams that would be passed.
  virtual void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget);
  // Called before views::Widget::Init() in InitializeDefaultWindow() to allow
  // subclasses to customize the InitParams that would be passed.
  virtual void OnBeforePanelWidgetInit(views::Widget::InitParams* init_params,
                                       views::Widget* widget);

  virtual void InitializeDefaultWindow(
      const extensions::AppWindow::CreateParams& create_params);
  virtual void InitializePanelWindow(
      const extensions::AppWindow::CreateParams& create_params);
  virtual views::NonClientFrameView* CreateStandardDesktopAppFrame();
  virtual views::NonClientFrameView* CreateNonStandardAppFrame() = 0;

  // Initializes |widget_| for |window|.
  virtual void InitializeWindow(
      VivaldiBrowserWindow* window,
      const extensions::AppWindow::CreateParams& create_params);

  // ui::BaseWindow implementation.
  bool IsActive() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool IsFullscreen() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  void Show() override;
  void ShowInactive() override;
  void Hide() override;
  bool IsVisible() const override;
  void Close() override;
  void Activate() override;
  void Deactivate() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void FlashFrame(bool flash) override;
  bool IsAlwaysOnTop() const override;
  void SetAlwaysOnTop(bool always_on_top) override;

  // WidgetDelegate implementation.
  void OnWidgetMove() override;
  views::View* GetInitiallyFocusedView() override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;
  bool ShouldShowWindowIcon() const override;
  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::WindowShowState show_state) override;
  bool GetSavedWindowPlacement(const views::Widget* widget,
                               gfx::Rect* bounds,
                               ui::WindowShowState* show_state) const override;
  void DeleteDelegate() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) override;
  bool ExecuteWindowsCommand(int command_id) override;
  void HandleKeyboardCode(ui::KeyboardCode code) override;
  gfx::ImageSkia GetWindowAppIcon() override;
  gfx::ImageSkia GetWindowIcon() override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(gfx::Path* mask) const override;
  void OnWindowBeginUserBoundsChange() override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  std::string GetWindowName() const override;

  // WidgetObserver implementation.
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroyed(views::Widget* widget) override;

  // WebContentsObserver implementation.
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

  // views::View implementation.
  void Layout() override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnFocus() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // NativeAppWindow implementation.
  void SetFullscreen(int fullscreen_types) override;
  bool IsFullscreenOrPending() const override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions) override;
  SkRegion* GetDraggableRegion() override;
  void UpdateShape(std::unique_ptr<ShapeRects> rects) override;
  void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  bool IsFrameless() const override;
  bool HasFrameColor() const override;
  SkColor ActiveFrameColor() const override;
  SkColor InactiveFrameColor() const override;
  gfx::Insets GetFrameInsets() const override;
  void HideWithApp() override;
  void ShowWithApp() override;
  gfx::Size GetContentMinimumSize() const override;
  gfx::Size GetContentMaximumSize() const override;
  void SetContentSizeConstraints(const gfx::Size& min_size,
                                 const gfx::Size& max_size) override;
  bool CanHaveAlphaEnabled() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  void UpdateEventTargeterWithInset() override;
  void SetActivateOnPointer(bool activate_on_pointer) override;

  // web_modal::WebContentsModalDialogHost implementation.
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

 protected:
  // The icon family for the task bar and elsewhere.
  gfx::ImageFamily icon_family_;

 private:
  friend class VivaldiBrowserWindow;

  // Informs modal dialogs that they need to update their positions.
  void OnViewWasResized();

  void OnImagesLoaded(gfx::ImageFamily image_family);

  // Custom shape of the window. If this is not set then the window has a
  // default shape, usually rectangular.
  std::unique_ptr<SkRegion> shape_;
  std::unique_ptr<ShapeRects> shape_rects_;
  bool has_frame_color_;
  SkColor active_frame_color_;
  SkColor inactive_frame_color_;

  VivaldiBrowserWindow* window_;  // Not owned.
  views::WebView* web_view_;
  views::Widget* widget_;

  std::unique_ptr<SkRegion> draggable_region_;

  bool frameless_;
  bool resizable_;
  bool shown_ = false;
  extensions::SizeConstraints size_constraints_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  base::ObserverList<web_modal::ModalDialogHostObserver> observer_list_;

  // The class that registers for keyboard shortcuts for extension commands.
  std::unique_ptr<ExtensionKeybindingRegistryViews>
      extension_keybinding_registry_;

  base::WeakPtrFactory<VivaldiNativeAppWindowViews> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiNativeAppWindowViews);
};

#endif  // UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_H_
