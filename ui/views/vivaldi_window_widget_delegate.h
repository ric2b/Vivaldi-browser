// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIVALDI_WINDOW_WIDGET_DELEGATE_H_
#define UI_VIEWS_VIVALDI_WINDOW_WIDGET_DELEGATE_H_

#include <memory>
#include <vector>

#include "ui/views/widget/widget_delegate.h"

class VivaldiBrowserWindow;

// Helper for VivaldiBrowserWindow to implement widget-related subclasses.
// Compared with views::BrowserView that implements those directly itself, we do
// it in a separated source to keep vivaldi_browser_window.cc manageable.
class VivaldiWindowWidgetDelegate final : public views::WidgetDelegate {
 public:
  explicit VivaldiWindowWidgetDelegate(VivaldiBrowserWindow* window);
  ~VivaldiWindowWidgetDelegate() override;
  VivaldiWindowWidgetDelegate(const VivaldiWindowWidgetDelegate&) = delete;
  VivaldiWindowWidgetDelegate& operator=(const VivaldiWindowWidgetDelegate&) =
      delete;

  // WidgetDelegate implementation.
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  void OnWidgetMove() override;
  views::View* GetInitiallyFocusedView() override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  std::u16string GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;
  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::mojom::WindowShowState show_state) override;
  bool GetSavedWindowPlacement(
      const views::Widget* widget,
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const override;
  bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) override;
  void HandleKeyboardCode(ui::KeyboardCode code) override;
  ui::ImageModel GetWindowAppIcon() override;
  ui::ImageModel GetWindowIcon() override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(SkPath* mask) const override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  std::string GetWindowName() const override;
  bool ExecuteWindowsCommand(int command_id) override;
  void WindowClosing() override;

 private:
  friend VivaldiBrowserWindow;

  const raw_ptr<VivaldiBrowserWindow> window_;  // The owner of this.
};

#endif  // UI_VIEWS_VIVALDI_WINDOW_WIDGET_DELEGATE_H_
