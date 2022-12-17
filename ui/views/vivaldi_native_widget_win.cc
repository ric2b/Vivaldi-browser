// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#include "ui/views/vivaldi_native_widget.h"

#include "chrome/browser/ui/browser_window_state.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

#include "ui/views/vivaldi_desktop_window_tree_host_win.h"
#include "ui/vivaldi_browser_window.h"

namespace {

class VivaldiDesktopNativeWidgetWin : public views::DesktopNativeWidgetAura {
 public:
  explicit VivaldiDesktopNativeWidgetWin(VivaldiBrowserWindow* window)
      : views::DesktopNativeWidgetAura(window->GetWidget()), window_(window) {
    GetNativeWindow()->SetName("VivaldiWindowAura");
  }
  ~VivaldiDesktopNativeWidgetWin() override = default;

  VivaldiDesktopNativeWidgetWin(const VivaldiDesktopNativeWidgetWin&) = delete;
  VivaldiDesktopNativeWidgetWin& operator=(
      const VivaldiDesktopNativeWidgetWin&) = delete;

  // views::DesktopNativeWidgetAura overrides

  void InitNativeWidget(views::Widget::InitParams params) override {
    tree_host_ = new VivaldiDesktopWindowTreeHostWin(window_, this);
    params.desktop_window_tree_host = tree_host_;
    DesktopNativeWidgetAura::InitNativeWidget(std::move(params));
  }

  void OnHostWorkspaceChanged(aura::WindowTreeHost* host) override {
    views::DesktopNativeWidgetAura::OnHostWorkspaceChanged(host);

    chrome::SaveWindowWorkspace(window_->browser(), GetWorkspace());
    chrome::SaveWindowVisibleOnAllWorkspaces(
        window_->browser(), window_->IsVisibleOnAllWorkspaces());
  }

 private:
  // The indirect owner
  raw_ptr<VivaldiBrowserWindow> window_;
  // Owned by superclass DesktopNativeWidgetAura.
  raw_ptr<views::DesktopWindowTreeHost> tree_host_;

};

}  // namespace

std::unique_ptr<views::NativeWidget> CreateVivaldiNativeWidget(
    VivaldiBrowserWindow* window) {
  return std::make_unique<VivaldiDesktopNativeWidgetWin>(window);
}
