// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"

#include "apps/ui/views/app_window_frame_view.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/apps/desktop_keyboard_capture.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "chrome/browser/ui/views/frame/taskbar_decorator.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/ui/zoom/page_zoom.h"
#include "components/ui/zoom/zoom_controller.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/easy_resize_window_targeter.h"
#if defined(OS_WIN)
#include <shlobj.h>
#include "base/win/registry.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/browser/web_applications/web_app_win.h"
#include "ui/base/win/shell.h"
#endif
#include "chrome/browser/ui/browser_commands.h"

using extensions::AppWindow;

namespace {

const int kMinPanelWidth = 100;
const int kMinPanelHeight = 100;
const int kDefaultPanelWidth = 200;
const int kDefaultPanelHeight = 300;

struct AcceleratorMapping {
  ui::KeyboardCode keycode;
  int modifiers;
  int command_id;
};

// 13-11-2014 arnar@vivaldi.com removed the line:
// { ui::VKEY_W, ui::EF_CONTROL_DOWN, IDC_CLOSE_WINDOW },
// from the array kAppWindowAcceleratorMap.
// Vivaldi browser will handle ctrl+w and not close the app
const AcceleratorMapping kAppWindowAcceleratorMap[] = {
    { ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_CLOSE_WINDOW },
    { ui::VKEY_F4, ui::EF_SHIFT_DOWN, IDC_CLOSE_WINDOW },
    { ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN, IDC_TASK_MANAGER },
};

// These accelerators will only be available in kiosk mode. These allow the
// user to manually zoom app windows. This is only necessary in kiosk mode
// (in normal mode, the user can zoom via the screen magnifier).
// TODO(xiyuan): Write a test for kiosk accelerators.
const AcceleratorMapping kAppWindowKioskAppModeAcceleratorMap[] = {
  { ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS },
  { ui::VKEY_OEM_MINUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_ZOOM_MINUS },
  { ui::VKEY_SUBTRACT, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS },
  { ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_ADD, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL },
  { ui::VKEY_NUMPAD0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL },
};

void AddAcceleratorsFromMapping(const AcceleratorMapping mapping[],
                                size_t mapping_length,
                                std::map<ui::Accelerator, int>* accelerators) {
  for (size_t i = 0; i < mapping_length; ++i) {
    ui::Accelerator accelerator(mapping[i].keycode, mapping[i].modifiers);
    (*accelerators)[accelerator] = mapping[i].command_id;
  }
}

const std::map<ui::Accelerator, int>& GetAcceleratorTable() {
  typedef std::map<ui::Accelerator, int> AcceleratorMap;
  CR_DEFINE_STATIC_LOCAL(AcceleratorMap, accelerators, ());
  if (!chrome::IsRunningInForcedAppMode()) {
    if (accelerators.empty()) {
      AddAcceleratorsFromMapping(
          kAppWindowAcceleratorMap,
          arraysize(kAppWindowAcceleratorMap),
          &accelerators);
    }
    return accelerators;
  }

  CR_DEFINE_STATIC_LOCAL(AcceleratorMap, app_mode_accelerators, ());
  if (app_mode_accelerators.empty()) {
    AddAcceleratorsFromMapping(
        kAppWindowAcceleratorMap,
        arraysize(kAppWindowAcceleratorMap),
        &app_mode_accelerators);
    AddAcceleratorsFromMapping(
        kAppWindowKioskAppModeAcceleratorMap,
        arraysize(kAppWindowKioskAppModeAcceleratorMap),
        &app_mode_accelerators);
  }
  return app_mode_accelerators;
}

#if defined(OS_WIN)
void CreateIconAndSetRelaunchDetails(
    const base::FilePath web_app_path,
    const base::FilePath icon_file,
    scoped_ptr<web_app::ShortcutInfo> shortcut_info,
    const HWND hwnd) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  if (base::CommandLine::ForCurrentProcess()->IsRunningVivaldi())
  {
	  // we don't want this info for the Vivaldi shortcut
	  shortcut_info->extension_id.clear();
	  shortcut_info->profile_path.clear();
  }
  // Set the relaunch data so "Pin this program to taskbar" has the app's
  // information.
  base::CommandLine command_line = ShellIntegration::CommandLineArgsForLauncher(
      shortcut_info->url,
      shortcut_info->extension_id,
      shortcut_info->profile_path);

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
     NOTREACHED();
     return;
  }
  command_line.SetProgram(chrome_exe);
  ui::win::SetRelaunchDetailsForWindow(command_line.GetCommandLineString(),
      shortcut_info->title, hwnd);

  if (!base::PathExists(web_app_path) &&
      !base::CreateDirectory(web_app_path)) {
    return;
  }
  ui::win::SetAppIconForWindow(icon_file.value(), hwnd);
  web_app::internals::CheckAndSaveIcon(icon_file, shortcut_info->favicon, false);
}
#endif

}  // namespace

ChromeNativeAppWindowViews::ChromeNativeAppWindowViews()
    : has_frame_color_(false),
      active_frame_color_(SK_ColorBLACK),
      inactive_frame_color_(SK_ColorBLACK) {
}

ChromeNativeAppWindowViews::~ChromeNativeAppWindowViews() {}

#if defined(OS_WIN)
void VivaldiShortcutPinToTaskbar(const base::string16 &app_id)
{
  const wchar_t kVivaldiKey[] = L"Software\\Vivaldi";
  const wchar_t kVivaldiPinToTaskbarValue[] = L"EnablePinToTaskbar";

  if (base::win::GetVersion() >= base::win::VERSION_WIN7 && base::CommandLine::ForCurrentProcess()->IsRunningVivaldi())
  {
    base::win::RegKey key_ptt(HKEY_CURRENT_USER, kVivaldiKey, KEY_ALL_ACCESS);
    if (!key_ptt.Valid())
      return;

    DWORD reg_pin_to_taskbar_enabled = 0;
    key_ptt.ReadValueDW(kVivaldiPinToTaskbarValue, &reg_pin_to_taskbar_enabled);
    if (reg_pin_to_taskbar_enabled != 0)
    {
      wchar_t system_buffer[MAX_PATH] = {0};
      if (FAILED(SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, system_buffer)))
      {
        return;
      }
      base::FilePath shortcut_link(system_buffer);
      shortcut_link = shortcut_link.AppendASCII("Vivaldi.lnk");
      // now apply the correct app id for the shortcut link
      base::win::ShortcutProperties props;
      props.set_app_id(app_id);
      props.options = base::win::ShortcutProperties::PROPERTIES_APP_ID;
      bool success = base::win::CreateOrUpdateShortcutLink(shortcut_link, props, base::win::SHORTCUT_UPDATE_EXISTING);
      if (success)
      {
        // pin the modified shortcut link to the taskbar
        success = base::win::TaskbarPinShortcutLink(shortcut_link);
        if (success)
        {
          // only pin once, typically on first run
          key_ptt.WriteValue(kVivaldiPinToTaskbarValue, DWORD(0));
        }
      }
    }
  }
}
#endif

void ChromeNativeAppWindowViews::OnBeforeWidgetInit(
    const AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
}

void ChromeNativeAppWindowViews::OnBeforePanelWidgetInit(
    bool use_default_bounds,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
}

void ChromeNativeAppWindowViews::InitializeDefaultWindow(
    const AppWindow::CreateParams& create_params) {
#if defined(OS_WIN)
  std::string app_name = web_app::GenerateApplicationNameFromExtensionId(
      app_window()->extension_id());
  base::string16 app_name_wide;
  app_name_wide.assign(app_name.begin(), app_name.end()); // convert to wide string
  content::BrowserThread::PostBlockingPoolTask(FROM_HERE, base::Bind(&VivaldiShortcutPinToTaskbar, app_name_wide));
#endif
  views::Widget::InitParams init_params(views::Widget::InitParams::TYPE_WINDOW);
  init_params.delegate = this;
  init_params.remove_standard_frame = IsFrameless() || has_frame_color_;
  init_params.use_system_default_icon = true;
  if (create_params.alpha_enabled) {
    init_params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;

    // The given window is most likely not rectangular since it uses
    // transparency and has no standard frame, don't show a shadow for it.
    // TODO(skuhne): If we run into an application which should have a shadow
    // but does not have, a new attribute has to be added.
    if (IsFrameless())
      init_params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
  }
  init_params.keep_on_top = create_params.always_on_top;
  init_params.visible_on_all_workspaces =
      create_params.visible_on_all_workspaces;

  init_params.thumbnail_window = create_params.thumbnail_window;

  OnBeforeWidgetInit(create_params, &init_params, widget());
  widget()->Init(init_params);

  // This will be used as window state for the first Show()
  widget()->SetSavedShowState(create_params.state);

  // The frame insets are required to resolve the bounds specifications
  // correctly. So we set the window bounds and constraints now.
  gfx::Insets frame_insets = GetFrameInsets();
  gfx::Rect window_bounds = create_params.GetInitialWindowBounds(frame_insets);
  SetContentSizeConstraints(create_params.GetContentMinimumSize(frame_insets),
                            create_params.GetContentMaximumSize(frame_insets));
  if (!window_bounds.IsEmpty()) {
    using BoundsSpecification = AppWindow::BoundsSpecification;
    bool position_specified =
        window_bounds.x() != BoundsSpecification::kUnspecifiedPosition &&
        window_bounds.y() != BoundsSpecification::kUnspecifiedPosition;
    if (!position_specified)
      widget()->CenterWindow(window_bounds.size());
    else
      widget()->SetBounds(window_bounds);
  }

#if defined(OS_CHROMEOS)
  if (create_params.is_ime_window)
    return;
#endif

  // Register accelarators supported by app windows.
  // TODO(jeremya/stevenjb): should these be registered for panels too?
  views::FocusManager* focus_manager = GetFocusManager();
  const std::map<ui::Accelerator, int>& accelerator_table =
      GetAcceleratorTable();
  const bool is_kiosk_app_mode = chrome::IsRunningInForcedAppMode();

  // Ensures that kiosk mode accelerators are enabled when in kiosk mode (to be
  // future proof). This is needed because GetAcceleratorTable() uses a static
  // to store data and only checks kiosk mode once. If a platform app is
  // launched before kiosk mode starts, the kiosk accelerators will not be
  // registered. This CHECK catches the case.
  CHECK(!is_kiosk_app_mode ||
        accelerator_table.size() ==
            arraysize(kAppWindowAcceleratorMap) +
                arraysize(kAppWindowKioskAppModeAcceleratorMap));

  // Ensure there is a ZoomController in kiosk mode, otherwise the processing
  // of the accelerators will cause a crash. Note CHECK here because DCHECK
  // will not be noticed, as this could only be relevant on real hardware.
  CHECK(!is_kiosk_app_mode ||
        ui_zoom::ZoomController::FromWebContents(web_view()->GetWebContents()));

  for (std::map<ui::Accelerator, int>::const_iterator iter =
           accelerator_table.begin();
       iter != accelerator_table.end(); ++iter) {
    if (is_kiosk_app_mode && !chrome::IsCommandAllowedInAppMode(iter->second))
      continue;

    focus_manager->RegisterAccelerator(
        iter->first, ui::AcceleratorManager::kNormalPriority, this);
  }
}

void ChromeNativeAppWindowViews::InitializePanelWindow(
    const AppWindow::CreateParams& create_params) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_PANEL);
  params.delegate = this;

  gfx::Rect initial_window_bounds =
      create_params.GetInitialWindowBounds(gfx::Insets());
  preferred_size_ = gfx::Size(initial_window_bounds.width(),
                              initial_window_bounds.height());
  if (preferred_size_.width() == 0)
    preferred_size_.set_width(kDefaultPanelWidth);
  else if (preferred_size_.width() < kMinPanelWidth)
    preferred_size_.set_width(kMinPanelWidth);

  if (preferred_size_.height() == 0)
    preferred_size_.set_height(kDefaultPanelHeight);
  else if (preferred_size_.height() < kMinPanelHeight)
    preferred_size_.set_height(kMinPanelHeight);

  // When a panel is not docked it will be placed at a default origin in the
  // currently active target root window.
  bool use_default_bounds = create_params.state != ui::SHOW_STATE_DOCKED;
  // Sanitize initial origin reseting it in case it was not specified.
  using BoundsSpecification = AppWindow::BoundsSpecification;
  bool position_specified =
      initial_window_bounds.x() != BoundsSpecification::kUnspecifiedPosition &&
      initial_window_bounds.y() != BoundsSpecification::kUnspecifiedPosition;
  params.bounds = (use_default_bounds || !position_specified) ?
      gfx::Rect(preferred_size_) :
      gfx::Rect(initial_window_bounds.origin(), preferred_size_);
  OnBeforePanelWidgetInit(use_default_bounds, &params, widget());
  widget()->Init(params);
  widget()->set_focus_on_creation(create_params.focused);
}

views::NonClientFrameView*
ChromeNativeAppWindowViews::CreateStandardDesktopAppFrame() {
  return views::WidgetDelegateView::CreateNonClientFrameView(widget());
}

void ChromeNativeAppWindowViews::UpdateEventTargeterWithInset() {
#if !defined(OS_MACOSX)
  // For non-Ash windows, install an easy resize window targeter, which ensures
  // that the root window (not the app) receives mouse events on the edges.
  if (chrome::GetHostDesktopTypeForNativeWindow(widget()->GetNativeWindow()) !=
    chrome::HOST_DESKTOP_TYPE_ASH) {
    bool is_maximized = IsMaximized();
    aura::Window* window = widget()->GetNativeWindow();
    int resize_inside = is_maximized ? 0 : 5;// frame->resize_inside_bounds_size();
    gfx::Insets inset(
      resize_inside, resize_inside, resize_inside, resize_inside);
    // Add the EasyResizeWindowTargeter on the window, not its root window. The
    // root window does not have a delegate, which is needed to handle the event
    // in Linux.
    scoped_ptr<ui::EventTargeter> old_eventtarget =
      window->SetEventTargeter(scoped_ptr<ui::EventTargeter>(
      new wm::EasyResizeWindowTargeter(window, inset, inset)));
    delete old_eventtarget.release();
  }
#endif
}

// ui::BaseWindow implementation.

gfx::Rect ChromeNativeAppWindowViews::GetRestoredBounds() const {
  return widget()->GetRestoredBounds();
}

ui::WindowShowState ChromeNativeAppWindowViews::GetRestoredState() const {
  if (IsMaximized())
    return ui::SHOW_STATE_MAXIMIZED;
  if (IsFullscreen())
    return ui::SHOW_STATE_FULLSCREEN;

  return ui::SHOW_STATE_NORMAL;
}

bool ChromeNativeAppWindowViews::IsAlwaysOnTop() const {
  // TODO(jackhou): On Mac, only docked panels are always-on-top.
  return app_window()->window_type_is_panel() || widget()->IsAlwaysOnTop();
}

// views::WidgetDelegate implementation.

gfx::ImageSkia ChromeNativeAppWindowViews::GetWindowAppIcon() {
  gfx::Image app_icon = app_window()->app_icon();
  if (app_icon.IsEmpty())
    return GetWindowIcon();
  else
    return *app_icon.ToImageSkia();
}

gfx::ImageSkia ChromeNativeAppWindowViews::GetWindowIcon() {
  content::WebContents* web_contents = app_window()->web_contents();
  if (web_contents) {
    favicon::FaviconDriver* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(web_contents);
    gfx::Image app_icon = favicon_driver->GetFavicon();
    if (!app_icon.IsEmpty())
      return *app_icon.ToImageSkia();
  }
  return gfx::ImageSkia();
}

views::NonClientFrameView* ChromeNativeAppWindowViews::CreateNonClientFrameView(
    views::Widget* widget) {
  return (IsFrameless() || has_frame_color_) ?
      CreateNonStandardAppFrame() : CreateStandardDesktopAppFrame();
}

bool ChromeNativeAppWindowViews::WidgetHasHitTestMask() const {
  return shape_ != NULL;
}

void ChromeNativeAppWindowViews::GetWidgetHitTestMask(gfx::Path* mask) const {
  shape_->getBoundaryPath(mask);
}

// views::View implementation.

gfx::Size ChromeNativeAppWindowViews::GetPreferredSize() const {
  if (!preferred_size_.IsEmpty())
    return preferred_size_;
  return NativeAppWindowViews::GetPreferredSize();
}

bool ChromeNativeAppWindowViews::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  const std::map<ui::Accelerator, int>& accelerator_table =
      GetAcceleratorTable();
  std::map<ui::Accelerator, int>::const_iterator iter =
      accelerator_table.find(accelerator);
  DCHECK(iter != accelerator_table.end());
  int command_id = iter->second;
  switch (command_id) {
    case IDC_CLOSE_WINDOW:
      Close();
      return true;
    case IDC_TASK_MANAGER:
      chrome::OpenTaskManager(NULL);
      return true;

    case IDC_ZOOM_MINUS:
      ui_zoom::PageZoom::Zoom(web_view()->GetWebContents(),
                              content::PAGE_ZOOM_OUT);
      return true;
    case IDC_ZOOM_NORMAL:
      ui_zoom::PageZoom::Zoom(web_view()->GetWebContents(),
                              content::PAGE_ZOOM_RESET);
      return true;
    case IDC_ZOOM_PLUS:
      ui_zoom::PageZoom::Zoom(web_view()->GetWebContents(),
                              content::PAGE_ZOOM_IN);
      return true;
    default:
      NOTREACHED() << "Unknown accelerator sent to app window.";
  }
  return NativeAppWindowViews::AcceleratorPressed(accelerator);
}

// NativeAppWindow implementation.

void ChromeNativeAppWindowViews::SetFullscreen(int fullscreen_types) {
  // Fullscreen not supported by panels.
  if (app_window()->window_type_is_panel())
    return;

  widget()->SetFullscreen(fullscreen_types != AppWindow::FULLSCREEN_TYPE_NONE);
}

bool ChromeNativeAppWindowViews::IsFullscreenOrPending() const {
  return widget()->IsFullscreen();
}

void ChromeNativeAppWindowViews::UpdateShape(scoped_ptr<SkRegion> region) {
  shape_ = region.Pass();
  widget()->SetShape(shape() ? new SkRegion(*shape()) : nullptr);
  widget()->OnSizeConstraintsChanged();
}

bool ChromeNativeAppWindowViews::HasFrameColor() const {
  return has_frame_color_;
}

SkColor ChromeNativeAppWindowViews::ActiveFrameColor() const {
  return active_frame_color_;
}

SkColor ChromeNativeAppWindowViews::InactiveFrameColor() const {
  return inactive_frame_color_;
}

void ChromeNativeAppWindowViews::SetInterceptAllKeys(bool want_all_keys) {
  if (want_all_keys && (desktop_keyboard_capture_.get() == NULL)) {
    desktop_keyboard_capture_.reset(new DesktopKeyboardCapture(widget()));
  } else if (!want_all_keys) {
    desktop_keyboard_capture_.reset(NULL);
  }
}

// NativeAppWindowViews implementation.

void ChromeNativeAppWindowViews::InitializeWindow(
    AppWindow* app_window,
    const AppWindow::CreateParams& create_params) {
  DCHECK(widget());
  has_frame_color_ = create_params.has_frame_color;
  active_frame_color_ = create_params.active_frame_color;
  inactive_frame_color_ = create_params.inactive_frame_color;
  if (create_params.window_type == AppWindow::WINDOW_TYPE_PANEL ||
      create_params.window_type == AppWindow::WINDOW_TYPE_V1_PANEL) {
    InitializePanelWindow(create_params);
  } else {
    InitializeDefaultWindow(create_params);
  }
  extension_keybinding_registry_.reset(new ExtensionKeybindingRegistryViews(
      Profile::FromBrowserContext(app_window->browser_context()),
      widget()->GetFocusManager(),
      extensions::ExtensionKeybindingRegistry::PLATFORM_APPS_ONLY,
      NULL));
}
