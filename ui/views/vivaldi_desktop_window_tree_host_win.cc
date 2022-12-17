// Copyright (c) 2017-2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/vivaldi_desktop_window_tree_host_win.h"

#include <dwmapi.h>
#include <windows.h>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/windows_version.h"
#include "chrome/browser/ui/views/frame/system_menu_insertion_delegate_win.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/theme_provider.h"
#include "ui/views/controls/menu/native_menu_win.h"

#include "app/vivaldi_apptools.h"
#include "ui/views/vivaldi_system_menu_model_builder.h"
#include "ui/vivaldi_browser_window.h"

// This is a copy of the class VirtualDesktopHelper in
// chromium\chrome\browser\ui\views\frame\browser_desktop_window_tree_host_win.cc
// See chromium\docs\windows_virtual_desktop_handling.md for documentation.
class VivaldiVirtualDesktopHelper
    : public base::RefCountedDeleteOnSequence<VivaldiVirtualDesktopHelper> {
 public:
  using WorkspaceChangedCallback = base::OnceCallback<void()>;

  explicit VivaldiVirtualDesktopHelper(const std::string& initial_workspace);
  VivaldiVirtualDesktopHelper(const VivaldiVirtualDesktopHelper&) = delete;
  VivaldiVirtualDesktopHelper& operator=(const VivaldiVirtualDesktopHelper&) =
      delete;

  // public methods are all called on the UI thread.
  void Init(HWND hwnd);

  std::string GetWorkspace();

  // |callback| is called when the task to get the desktop id of |hwnd|
  // completes, if the workspace has changed.
  void UpdateWindowDesktopId(HWND hwnd, WorkspaceChangedCallback callback);

  bool GetInitialWorkspaceRemembered() const;

  void SetInitialWorkspaceRemembered(bool remembered);

 private:
  friend class base::RefCountedDeleteOnSequence<VivaldiVirtualDesktopHelper>;
  friend class base::DeleteHelper<VivaldiVirtualDesktopHelper>;

  ~VivaldiVirtualDesktopHelper();

  // Called on the UI thread as a task reply.
  void SetWorkspace(WorkspaceChangedCallback callback,
                    const std::string& workspace);

  void InitImpl(HWND hwnd, const std::string& initial_workspace);

  static std::string GetWindowDesktopIdImpl(
      HWND hwnd,
      Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager);

  // All member variables, except where noted, are only accessed on the ui
  // thead.

  // Workspace browser window was opened on. This is used to tell the
  // BrowserWindowState about the initial workspace, which has to happen after
  // |this| is fully set up.
  const std::string initial_workspace_;

  // On Windows10, this is the virtual desktop the browser window was on,
  // last we checked. This is used to tell if the window has moved to a
  // different desktop, and notify listeners. It will only be set if
  // we created |virtual_desktop_helper_|.
  absl::optional<std::string> workspace_;

  bool initial_workspace_remembered_ = false;

  // Only set on Windows 10. This is created and accessed on a separate
  // COMSTAT thread. It will be null if creation failed.
  Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager_;

  base::WeakPtrFactory<VivaldiVirtualDesktopHelper> weak_factory_{this};
};

VivaldiVirtualDesktopHelper::VivaldiVirtualDesktopHelper(
    const std::string& initial_workspace)
    : base::RefCountedDeleteOnSequence<VivaldiVirtualDesktopHelper>(
          base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()})),
      initial_workspace_(initial_workspace) {}

void VivaldiVirtualDesktopHelper::Init(HWND hwnd) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  owning_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VivaldiVirtualDesktopHelper::InitImpl, this,
                                hwnd, initial_workspace_));
}

VivaldiVirtualDesktopHelper::~VivaldiVirtualDesktopHelper() {}

std::string VivaldiVirtualDesktopHelper::GetWorkspace() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!workspace_.has_value())
    workspace_ = initial_workspace_;

  return workspace_.value_or(std::string());
}

void VivaldiVirtualDesktopHelper::UpdateWindowDesktopId(
    HWND hwnd,
    WorkspaceChangedCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  owning_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&VivaldiVirtualDesktopHelper::GetWindowDesktopIdImpl, hwnd,
                     virtual_desktop_manager_),
      base::BindOnce(&VivaldiVirtualDesktopHelper::SetWorkspace, this,
                     std::move(callback)));
}

bool VivaldiVirtualDesktopHelper::GetInitialWorkspaceRemembered() const {
  return initial_workspace_remembered_;
}

void VivaldiVirtualDesktopHelper::SetInitialWorkspaceRemembered(
    bool remembered) {
  initial_workspace_remembered_ = remembered;
}

void VivaldiVirtualDesktopHelper::SetWorkspace(
    WorkspaceChangedCallback callback,
    const std::string& workspace) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // If GetWindowDesktopId() fails, |workspace| will be empty, and it's most
  // likely that the current value of |workspace_| is still correct, so don't
  // overwrite it.
  if (workspace.empty())
    return;

  bool workspace_changed = workspace != workspace_.value_or(std::string());
  workspace_ = workspace;
  if (workspace_changed)
    std::move(callback).Run();
}

void VivaldiVirtualDesktopHelper::InitImpl(
    HWND hwnd,
    const std::string& initial_workspace) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Virtual Desktops on Windows are best-effort and may not always be
  // available.
  if (FAILED(::CoCreateInstance(__uuidof(::VirtualDesktopManager), nullptr,
                                CLSCTX_ALL,
                                IID_PPV_ARGS(&virtual_desktop_manager_))) ||
      initial_workspace.empty()) {
    return;
  }
  GUID guid = GUID_NULL;
  HRESULT hr =
      CLSIDFromString(base::UTF8ToWide(initial_workspace).c_str(), &guid);
  if (SUCCEEDED(hr)) {
    // There are valid reasons MoveWindowToDesktop can fail, e.g.,
    // the desktop was deleted. If it fails, the window will open on the
    // current desktop.
    hr = virtual_desktop_manager_->MoveWindowToDesktop(hwnd, guid);
    if (FAILED(hr)) {
      LOG(WARNING) << "Error moving window to virtual desktop "
                   << initial_workspace;
    }
  }
}

// static
std::string VivaldiVirtualDesktopHelper::GetWindowDesktopIdImpl(
    HWND hwnd,
    Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!virtual_desktop_manager)
    return std::string();

  GUID workspace_guid;
  HRESULT hr =
      virtual_desktop_manager->GetWindowDesktopId(hwnd, &workspace_guid);
  if (FAILED(hr) || workspace_guid == GUID_NULL)
    return std::string();

  LPOLESTR workspace_widestr;
  StringFromCLSID(workspace_guid, &workspace_widestr);
  std::string workspace_id = base::WideToUTF8(workspace_widestr);
  CoTaskMemFree(workspace_widestr);
  return workspace_id;
}

////////////////////////////////////////////////////////////////////////////////
// VivaldiDesktopWindowTreeHostWin
////////////////////////////////////////////////////////////////////////////////

VivaldiDesktopWindowTreeHostWin::VivaldiDesktopWindowTreeHostWin(
    VivaldiBrowserWindow* window,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura)
    : DesktopWindowTreeHostWin(window->GetWidget(), desktop_native_widget_aura),
      window_(window) {}

VivaldiDesktopWindowTreeHostWin::~VivaldiDesktopWindowTreeHostWin() {}

void VivaldiDesktopWindowTreeHostWin::Init(
    const views::Widget::InitParams& params) {
  DesktopWindowTreeHostWin::Init(params);

  if (base::win::GetVersion() >= base::win::Version::WIN10) {
    // VirtualDesktopManager isn't supported pre Win-10.
    virtual_desktop_helper_ = new VivaldiVirtualDesktopHelper(params.workspace);
    virtual_desktop_helper_->Init(GetHWND());
  }
}

void VivaldiDesktopWindowTreeHostWin::Show(ui::WindowShowState show_state,
                                           const gfx::Rect& restore_bounds) {
  // This will make BrowserWindowState remember the initial workspace.
  // It has to be called after DesktopNativeWidgetAura is observing the host
  // and the session service is tracking the window.
  if (virtual_desktop_helper_ &&
      !virtual_desktop_helper_->GetInitialWorkspaceRemembered()) {
    // If |virtual_desktop_helper_| has an empty workspace, kick off an update,
    // which will eventually call OnHostWorkspaceChanged.
    if (virtual_desktop_helper_->GetWorkspace().empty()) {
      UpdateWorkspace();
    } else {
      OnHostWorkspaceChanged();
    }
  }
  DesktopWindowTreeHostWin::Show(show_state, restore_bounds);
}

std::string VivaldiDesktopWindowTreeHostWin::GetWorkspace() const {
  return virtual_desktop_helper_ ? virtual_desktop_helper_->GetWorkspace()
                                 : std::string();
}

int VivaldiDesktopWindowTreeHostWin::GetInitialShowState() const {
  STARTUPINFO si = {0};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  GetStartupInfo(&si);
  return si.wShowWindow;
}

void VivaldiDesktopWindowTreeHostWin::HandleFrameChanged() {
  window_->OnNativeWindowChanged();
  DesktopWindowTreeHostWin::HandleFrameChanged();
}

views::NativeMenuWin* VivaldiDesktopWindowTreeHostWin::GetSystemMenu() {
  if (!system_menu_) {
    menu_model_builder_ = std::make_unique<VivaldiSystemMenuModelBuilder>(
        window_->GetAcceleratorProvider(), window_->browser());
    menu_model_builder_->Init();

    system_menu_ = std::make_unique<views::NativeMenuWin>(
        menu_model_builder_->menu_model(), GetHWND());

    SystemMenuInsertionDelegateWin insertion_delegate;
    system_menu_->Rebuild(&insertion_delegate);
  }
  return system_menu_.get();
}

bool VivaldiDesktopWindowTreeHostWin::PreHandleMSG(UINT message,
                                                   WPARAM w_param,
                                                   LPARAM l_param,
                                                   LRESULT* result) {
  switch (message) {
    case WM_INITMENUPOPUP:
      GetSystemMenu()->UpdateStates();
      return true;
  }
  return DesktopWindowTreeHostWin::PreHandleMSG(message, w_param, l_param,
                                                result);
}

void VivaldiDesktopWindowTreeHostWin::PostHandleMSG(UINT message,
                                                    WPARAM w_param,
                                                    LPARAM l_param) {
  switch (message) {
    case WM_SETFOCUS: {
      // Virtual desktop is only updated after the window has been focused at
      // least once as Windows provides no event for when a window is moved to a
      // different virtual desktop, so we handle it here.
      UpdateWorkspace();
      break;
    }
    case WM_DWMCOLORIZATIONCOLORCHANGED: {
      vivaldi::GetSystemColorsUpdatedCallbackList().Notify();
      break;
    }
  }
  return DesktopWindowTreeHostWin::PostHandleMSG(message, w_param, l_param);
}

void VivaldiDesktopWindowTreeHostWin::Maximize() {
  // Maximizing on Windows causes the window to be shown. Call Show() first to
  // ensure the content view is also made visible. See http://crbug.com/436867.
  // TODO(jackhou): Make this behavior the same as other platforms, i.e. calling
  // Maximize() does not also show the window.
  if (IsVisible()) {
    Show(ui::SHOW_STATE_NORMAL, gfx::Rect());
  }
  views::DesktopWindowTreeHostWin::Maximize();
}

void VivaldiDesktopWindowTreeHostWin::Minimize() {
  // Minimizing on Windows causes the window to be shown. Call Show() first to
  // ensure the content view is also made visible. See http://crbug.com/436867.
  // TODO(jackhou): Make this behavior the same as other platforms, i.e. calling
  // Minimize() does not also show the window.
  if (!IsVisible()) {
    Show(ui::SHOW_STATE_NORMAL, gfx::Rect());
  }
  views::DesktopWindowTreeHostWin::Minimize();
}

////////////////////////////////////////////////////////////////////////////////
// VivaldiDesktopWindowTreeHostWin, private:
void VivaldiDesktopWindowTreeHostWin::UpdateWorkspace() {
  if (!virtual_desktop_helper_)
    return;
  virtual_desktop_helper_->UpdateWindowDesktopId(
      GetHWND(),
      base::BindOnce(&VivaldiDesktopWindowTreeHostWin::OnHostWorkspaceChanged,
                     weak_factory_.GetWeakPtr()));
}

bool VivaldiDesktopWindowTreeHostWin::ShouldUseNativeFrame() const {
  // NOTE(igor@vivaldi.com): It is unclear what exactly this function is
  // supposed to return as it is still possible to have the native
  // Windows frame with the function returning false and have emulated look of
  // Windows 7, not the native frame, when returning true. This depends on the
  // interaction with GetFrameMode() result and
  // views::Widget::InitParams::remove_standard_frame value in
  // VivaldiBrowserWindow::InitWidget.
  return window_->with_native_frame();
}

views::FrameMode VivaldiDesktopWindowTreeHostWin::GetFrameMode() const {
  if (window_->with_native_frame())
    return views::FrameMode::SYSTEM_DRAWN;

  // TODO(igor@vivaldi.com): Use FrameMode::CUSTOM_DRAWN here. Presently it is
  // not used as for some reason it prevents autohide Taskbar to appear in a
  // maximized window despite the workaround in Chromium's
  // HWNDMessageHandler::OnNCCalcSize().
  return views::FrameMode::SYSTEM_DRAWN_NO_CONTROLS;
}

void VivaldiDesktopWindowTreeHostWin::HandleCreate() {
  views::DesktopWindowTreeHostWin::HandleCreate();
  if (base::win::GetVersion() >= base::win::Version::WIN11) {
    // NOTE(igor@vivaldi.com) - Tell Windows 11 that we really want rounded
    // corners around a Vivaldi window. Chromium does not need to do that as
    // heuristics in Windows to guess if the window wants rounded corners work
    // for them. But they fail for a Vivaldi window.
    //
    // SDK in Chromium does not yet support the necessary constants, so use
    // explicit numeric values.
    //
    // DWM_WINDOW_CORNER_PREFERENCE corner_preference = DWMWCP_ROUND;
    DWORD corner_preference = 2;
    DwmSetWindowAttribute(GetHWND(), 33 /*DWMWA_WINDOW_CORNER_PREFERENC*/,
                          &corner_preference, sizeof(corner_preference));
  }
}