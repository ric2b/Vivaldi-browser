// Copyright (c) 2017-2022 Vivaldi Technologies AS. All rights reserved.
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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/frame/system_menu_insertion_delegate_win.h"
#include "content/public/browser/browser_thread.h"

#include "app/vivaldi_apptools.h"
#include "skia/ext/skia_utils_win.h"

#include "ui/base/win/hwnd_metrics.h"
#include "ui/base/win/shell.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/menu/native_menu_win.h"
#include "ui/views/vivaldi_system_menu_model_builder.h"
#include "ui/vivaldi_browser_window.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

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
  std::optional<std::string> workspace_;

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
      window_(window) {
  prefs_registrar_.Init(window_->GetProfile()->GetPrefs());
  // NOTE(andre@vivaldi.com) : Unretained is safe as this will live along
  // prefs_registrar_.
  // dark or light mode
  prefs_registrar_.Add(
      vivaldiprefs::kSystemDesktopThemeColor,
      base::BindRepeating(&VivaldiDesktopWindowTreeHostWin::OnPrefsChanged,
                          base::Unretained(this)));
}

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

void VivaldiDesktopWindowTreeHostWin::Show(
    ui::mojom::WindowShowState show_state,
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

void VivaldiDesktopWindowTreeHostWin::OnPrefsChanged(const std::string& path) {
  if (path == vivaldiprefs::kSystemDesktopThemeColor) {
    UpdateWindowBorderColor(!IsActive(), true);
  }
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
  if (!window_) {
    return;
  }
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
    case WM_ACTIVATE: {
      UpdateWindowBorderColor(w_param == WA_INACTIVE);
      break;
    }
    case WM_DWMCOLORIZATIONCOLORCHANGED: {
      vivaldi::GetSystemColorsUpdatedCallbackList().Notify();
      break;
    }
  }
  return DesktopWindowTreeHostWin::PostHandleMSG(message, w_param, l_param);
}

void VivaldiDesktopWindowTreeHostWin::Restore() {
  // Enable window shade and rounding
  SetRoundedWindowCorners(true);
  views::DesktopWindowTreeHostWin::Restore();
}

void VivaldiDesktopWindowTreeHostWin::Maximize() {
  // Maximizing on Windows causes the window to be shown. Call Show() first to
  // ensure the content view is also made visible. See http://crbug.com/436867.
  // TODO(jackhou): Make this behavior the same as other platforms, i.e. calling
  // Maximize() does not also show the window.
  if (IsVisible()) {
    Show(ui::mojom::WindowShowState::kNormal, gfx::Rect());
  }

  // Disable shadow and rounding to prevent bleeding cross screens.
  SetRoundedWindowCorners(false);
  views::DesktopWindowTreeHostWin::Maximize();
}

void VivaldiDesktopWindowTreeHostWin::Minimize() {
  // Minimizing on Windows causes the window to be shown. Call Show() first to
  // ensure the content view is also made visible. See http://crbug.com/436867.
  // TODO(jackhou): Make this behavior the same as other platforms, i.e. calling
  // Minimize() does not also show the window.
  if (!IsVisible()) {
    Show(ui::mojom::WindowShowState::kNormal, gfx::Rect());
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
  BOOL is_enabled;
  return SUCCEEDED(DwmIsCompositionEnabled(&is_enabled)) && is_enabled;
}

views::FrameMode VivaldiDesktopWindowTreeHostWin::GetFrameMode() const {
  // "glass" frame is assumed in |GetClientAreaInsets|.
  return views::FrameMode::SYSTEM_DRAWN;
}

bool VivaldiDesktopWindowTreeHostWin::HasFrame() const {
  return window_->with_native_frame();
}

void VivaldiDesktopWindowTreeHostWin::HandleCreate() {
  views::DesktopWindowTreeHostWin::HandleCreate();
  SetRoundedWindowCorners(true);
  UpdateWindowBorderColor(!IsActive(), true);
}

void VivaldiDesktopWindowTreeHostWin::OnAccentColorUpdated() {
  UpdateWindowBorderColor(!IsActive(), true);
}

void VivaldiDesktopWindowTreeHostWin::UpdateWindowBorderColor(
    bool is_inactive,
    bool check_global_accent /*false*/) {
  if (!window_) {
    // |window_| might go away on window close.
    return;
  }
  // NOTE(andre@vivaldi.com) : The accent_color_inactive_ member is only set in
  // the AccentColorObserver when the border accent is set. Use this as a flag.
  if (check_global_accent &&
      base::win::GetVersion() >= base::win::Version::WIN8) {
    dwm_key_ = std::make_unique<base::win::RegKey>(
        HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\DWM", KEY_READ);
    if (dwm_key_->Valid()) {
      DWORD accent_color, color_prevalence;

      has_accent_set_ =
          dwm_key_->ReadValueDW(L"AccentColor", &accent_color) ==
              ERROR_SUCCESS &&
          dwm_key_->ReadValueDW(L"ColorPrevalence", &color_prevalence) ==
              ERROR_SUCCESS &&
          color_prevalence == 1;
    }
  }

  // If the system has accent_color set, this overrides everything.
  if (has_accent_set_) {
    if (is_inactive) {
      std::optional<SkColor> accent_color_inactive =
          ui::AccentColorObserver::Get()->accent_color_inactive();

      if (accent_color_inactive) {
        window_border_color_ = RGB(SkColorGetR(*accent_color_inactive),
                                   SkColorGetG(*accent_color_inactive),
                                   SkColorGetB(*accent_color_inactive));
      } else {
        window_border_color_ = GetSysColor(CTLCOLOR_STATIC);
      }
    } else {
      std::optional<SkColor> accent_color =
          ui::AccentColorObserver::Get()->accent_color();
      window_border_color_ =
          RGB(SkColorGetR(*accent_color), SkColorGetG(*accent_color),
              SkColorGetB(*accent_color));
    }
  } else {
    // System accent border colors are disabled, let dark and light mode decide.
    Profile* profile = window_->GetProfile();
    bool is_dark_mode =
        profile ? (static_cast<int>(
                       vivaldiprefs::SystemDesktopThemeColorValues::kDark) ==
                   profile->GetPrefs()->GetInteger(
                       vivaldiprefs::kSystemDesktopThemeColor))
                : false;
    window_border_color_ =
        is_dark_mode ? VIVALDI_WINDOW_BORDER_DARK : VIVALDI_WINDOW_BORDER_LIGHT;
  }
  SetWindowAccentColor(window_border_color_);
}

void VivaldiDesktopWindowTreeHostWin::SetFullscreen(bool fullscreen,
                                                    int64_t target_display_id) {
  // Disable rounded corners in fullscreen.
  SetRoundedWindowCorners(!fullscreen);
  views::DesktopWindowTreeHostWin::SetFullscreen(fullscreen, target_display_id);
}

bool VivaldiDesktopWindowTreeHostWin::GetDwmFrameInsetsInPixels(
    gfx::Insets* insets) const {
  // System window decorations.
  if (window_->with_native_frame()) {
    return false;
  }
  *insets = gfx::Insets();
  return true;
}

bool VivaldiDesktopWindowTreeHostWin::GetClientAreaInsets(
    gfx::Insets* insets,
    HMONITOR monitor) const {
  // System window decorations, or maximized windows gets a frame drawn
  // regardless. Do not set any insets.
  if (window_->with_native_frame() ) {
    return false;
  }

  // Don't extend the glass in at all if it won't be visible.
  if (GetWidget()->IsFullscreen()) {
    *insets = gfx::Insets();
  } else {
    const int frame_thickness =
        GetWidget()->IsMaximized() ? ui::GetFrameThickness(monitor) : 1;
    const int top_frame_thickness =
        GetWidget()->IsMaximized() ? frame_thickness : 0;
    *insets = gfx::Insets::TLBR(top_frame_thickness, frame_thickness,
                                frame_thickness, frame_thickness);
  }
  return true;
}

void VivaldiDesktopWindowTreeHostWin::SetRoundedWindowCorners(
    bool enable_round_corners) {
  if (base::win::GetVersion() >= base::win::Version::WIN11) {
    DWORD corner_preference =
        enable_round_corners ? DWMWCP_ROUND : DWMWCP_DEFAULT;
    DwmSetWindowAttribute(GetHWND(), DWMWA_WINDOW_CORNER_PREFERENCE,
                          &corner_preference, sizeof(DWORD));
  }
}

void VivaldiDesktopWindowTreeHostWin::SetWindowAccentColor(
    COLORREF bordercolor) {
  if (base::win::GetVersion() >= base::win::Version::WIN11) {
    DwmSetWindowAttribute(GetHWND(), DWMWA_BORDER_COLOR, &bordercolor,
                          sizeof(COLORREF));
  }
}
