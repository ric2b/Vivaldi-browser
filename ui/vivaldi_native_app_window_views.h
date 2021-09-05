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
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/draggable_region.h"
#include "ui/base/base_window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_family.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/client_view.h"

class SkRegion;
class VivaldiBrowserWindow;
struct VivaldiBrowserWindowParams;

namespace content {
class RenderViewHost;
}

namespace views {
class WebView;
}

// This is a merge of NativeAppWindowViews and ChromeNativeAppWindowViews.
class VivaldiNativeAppWindowViews : public views::WidgetDelegateView,
                                    public views::WidgetObserver {
 public:
  static std::unique_ptr<VivaldiNativeAppWindowViews> Create();

  VivaldiNativeAppWindowViews();
  ~VivaldiNativeAppWindowViews() override;

  void Init(VivaldiBrowserWindow* window,
            const VivaldiBrowserWindowParams& create_params);

  // Signal that CanHaveTransparentBackground has changed.
  void OnCanHaveAlphaEnabledChanged();

  VivaldiBrowserWindow* window() { return window_; }
  const VivaldiBrowserWindow* window() const { return window_; }

  views::WebView* web_view() { return web_view_; }
  const views::WebView* web_view() const { return web_view_; }

  views::Widget* widget() { return widget_; }
  const views::Widget* widget() const { return widget_; }

  SkRegion* draggable_region() { return draggable_region_.get(); }
  bool is_frameless() const { return frameless_; }

  gfx::NativeWindow GetNativeWindow() const;
  gfx::NativeView GetNativeView();
  bool CanHaveAlphaEnabled() const;

  // Informs modal dialogs that they need to update their positions.
  void OnViewWasResized();

  void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions);

 protected:
  // Called before views::Widget::Init() in InitializeDefaultWindow() to allow
  // subclasses to customize the InitParams that would be passed.
  virtual void OnBeforeWidgetInit(views::Widget::InitParams& init_params) = 0;

  virtual void InitializeDefaultWindow(
      const VivaldiBrowserWindowParams& create_params);
  virtual bool IsOnCurrentWorkspace() const;
  virtual void UpdateEventTargeterWithInset();
  virtual void ShowEmojiPanel();

  virtual ui::WindowShowState GetRestoredState() const;

  virtual bool IsMaximized() const;
  virtual void Maximize();
  virtual gfx::Rect GetRestoredBounds() const;
  virtual void Restore();
  virtual void Show();
  virtual void FlashFrame(bool flash);

  void SetFullscreen(bool is_fullscreen);
  bool IsFullscreenOrPending() const;
  void UpdateWindowIcon();
  void UpdateWindowTitle();
  virtual gfx::Insets GetFrameInsets() const;
  void SetVisibleOnAllWorkspaces(bool always_visible);
  void SetActivateOnPointer(bool activate_on_pointer);

  bool IsActive() const;
  bool IsMinimized() const;
  bool IsFullscreen() const;
  gfx::Rect GetBounds() const;
  void Hide();
  bool IsVisible() const;
  void Close();
  void Activate();
  void Deactivate();
  void Minimize();
  void SetBounds(const gfx::Rect& bounds);
  ui::ZOrderLevel GetZOrderLevel() const;
  void SetZOrderLevel(ui::ZOrderLevel order);

  // WidgetDelegate implementation.
  void OnWidgetMove() override;
  views::View* GetInitiallyFocusedView() override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;
  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::WindowShowState show_state) override;
  bool GetSavedWindowPlacement(const views::Widget* widget,
                               gfx::Rect* bounds,
                               ui::WindowShowState* show_state) const override;
  void DeleteDelegate() override;
  bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) override;
  bool ExecuteWindowsCommand(int command_id) override;
  void HandleKeyboardCode(ui::KeyboardCode code) override;
  gfx::ImageSkia GetWindowAppIcon() override;
  gfx::ImageSkia GetWindowIcon() override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(SkPath* mask) const override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  std::string GetWindowName() const override;

  // WidgetObserver implementation.
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroyed(views::Widget* widget) override;

  // views::View implementation.
  void Layout() override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnFocus() override;

 private:
  class ModalDialogHost : public web_modal::WebContentsModalDialogHost {
   public:
    ModalDialogHost(VivaldiNativeAppWindowViews* views);
    ~ModalDialogHost() override;

    // web_modal::WebContentsModalDialogHost implementation.
    gfx::NativeView GetHostView() const override;
    gfx::Point GetDialogPosition(const gfx::Size& size) override;
    gfx::Size GetMaximumDialogSize() override;
    void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
    void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

    VivaldiNativeAppWindowViews* const views_;
    base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
        observers_;
  };

  friend class ModalDialogHost;
  friend class VivaldiBrowserWindow;

  void SetIconFamily(gfx::ImageFamily image_family);

  VivaldiBrowserWindow* window_;  // Not owned.
  views::WebView* web_view_;
  views::Widget* widget_;

  std::unique_ptr<SkRegion> draggable_region_;
  ModalDialogHost modal_dialog_host_{this};

  bool frameless_;
  gfx::Size minimum_size_;

  // The icon family for the task bar and elsewhere.
  gfx::ImageFamily icon_family_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiNativeAppWindowViews);
};

#endif  // UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_H_
