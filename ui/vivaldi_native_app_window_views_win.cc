// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_native_app_window_views_win.h"

#include "apps/ui/views/app_window_frame_view.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/apps/glass_app_window_frame_view_win.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_helpers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_win.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/win/shell.h"
#include "ui/views/vivaldi_pin_shortcut.h"
#include "ui/views/vivaldi_system_menu_model_builder.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/win/hwnd_util.h"
#include "ui/vivaldi_app_window_desktop_native_widget_aura_win.h"
#include "ui/vivaldi_browser_window.h"

VivaldiNativeAppWindowViewsWin::VivaldiNativeAppWindowViewsWin()
    : glass_frame_view_(NULL), is_translucent_(false), weak_ptr_factory_(this) {
}

VivaldiNativeAppWindowViewsWin::~VivaldiNativeAppWindowViewsWin() {
}

HWND VivaldiNativeAppWindowViewsWin::GetNativeAppWindowHWND() const {
  return views::HWNDForWidget(widget()->GetTopLevelWidget());
}

void VivaldiNativeAppWindowViewsWin::EnsureCaptionStyleSet() {
  // Windows seems to have issues maximizing windows without WS_CAPTION.
  // The default views / Aura implementation will remove this if we are using
  // frameless or colored windows, so we put it back here.
  HWND hwnd = GetNativeAppWindowHWND();
  int current_style = ::GetWindowLong(hwnd, GWL_STYLE);
  ::SetWindowLong(hwnd, GWL_STYLE, current_style | WS_CAPTION);
}

gfx::Insets VivaldiNativeAppWindowViewsWin::GetFrameInsets() const {
  if (IsFrameless())
    return VivaldiNativeAppWindowViews::GetFrameInsets();
  else
    return gfx::Insets();
}

void VivaldiNativeAppWindowViewsWin::OnBeforeWidgetInit(
    const extensions::AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
  VivaldiNativeAppWindowViewsAura::OnBeforeWidgetInit(create_params, init_params,
                                                     widget);
  init_params->native_widget =
      new VivaldiAppWindowDesktopNativeWidgetAuraWin(this);

  is_translucent_ =
      init_params->opacity == views::Widget::InitParams::TRANSLUCENT_WINDOW;
}

void VivaldiNativeAppWindowViewsWin::InitializeDefaultWindow(
    const extensions::AppWindow::CreateParams& create_params) {
  VivaldiNativeAppWindowViewsAura::InitializeDefaultWindow(create_params);

  const extensions::Extension* extension = window()->extension();
  if (!extension)
    return;

  vivaldi::StartPinShortcutToTaskbar(window());

  std::string app_name =
      web_app::GenerateApplicationNameFromExtensionId(extension->id());
  base::string16 app_name_wide = base::UTF8ToWide(app_name);
  HWND hwnd = GetNativeAppWindowHWND();
  Profile* profile = window()->GetProfile();
  app_model_id_ = shell_integration::win::GetAppModelIdForProfile(
      app_name_wide, profile->GetPath());
  ui::win::SetAppIdForWindow(app_model_id_, hwnd);
  web_app::UpdateRelaunchDetailsForApp(profile, extension, hwnd);

  if (!create_params.alpha_enabled)
    EnsureCaptionStyleSet();
}

views::NonClientFrameView*
VivaldiNativeAppWindowViewsWin::CreateStandardDesktopAppFrame() {
  return VivaldiNativeAppWindowViewsAura::CreateStandardDesktopAppFrame();
}

bool VivaldiNativeAppWindowViewsWin::CanMinimize() const {
  // Resizing on Windows breaks translucency if the window also has shape.
  // See http://crbug.com/417947.
  return VivaldiNativeAppWindowViewsAura::CanMinimize() &&
         !(WidgetHasHitTestMask() && is_translucent_);
}

void VivaldiNativeAppWindowViewsWin::UpdateEventTargeterWithInset() {
  VivaldiNativeAppWindowViews::UpdateEventTargeterWithInset();
}

ui::MenuModel* VivaldiNativeAppWindowViewsWin::GetSystemMenuModel() {
  if (!menu_model_builder_.get()) {
    menu_model_builder_.reset(
      new VivaldiSystemMenuModelBuilder(window(), window()->browser()));
    menu_model_builder_->Init();
  }
  return menu_model_builder_->menu_model();
}

int VivaldiNativeAppWindowViewsWin::GetCommandIDForAppCommandID(int app_command_id) const {
  switch (app_command_id) {
    // NOTE: The order here matches the APPCOMMAND declaration order in the
    // Windows headers.
  case APPCOMMAND_BROWSER_BACKWARD: return IDC_BACK;
  case APPCOMMAND_BROWSER_FORWARD:  return IDC_FORWARD;
  case APPCOMMAND_BROWSER_REFRESH:  return IDC_RELOAD;
  case APPCOMMAND_BROWSER_HOME:     return IDC_HOME;
  case APPCOMMAND_BROWSER_STOP:     return IDC_STOP;
  case APPCOMMAND_BROWSER_SEARCH:   return IDC_FOCUS_SEARCH;
  case APPCOMMAND_HELP:             return IDC_HELP_PAGE_VIA_KEYBOARD;
  case APPCOMMAND_NEW:              return IDC_NEW_TAB;
  case APPCOMMAND_OPEN:             return IDC_OPEN_FILE;
  case APPCOMMAND_CLOSE:            return IDC_CLOSE_TAB;
  case APPCOMMAND_SAVE:             return IDC_SAVE_PAGE;
  case APPCOMMAND_PRINT:            return IDC_PRINT;
  case APPCOMMAND_COPY:             return IDC_COPY;
  case APPCOMMAND_CUT:              return IDC_CUT;
  case APPCOMMAND_PASTE:            return IDC_PASTE;
    // TODO(pkasting): http://b/1113069 Handle these.
  case APPCOMMAND_UNDO:
  case APPCOMMAND_REDO:
  case APPCOMMAND_SPELL_CHECK:
  default:                          return -1;
  }
}

bool VivaldiNativeAppWindowViewsWin::ExecuteWindowsCommand(int command_id) {
  // This function handles WM_SYSCOMMAND, WM_APPCOMMAND, and WM_COMMAND.
  // Translate WM_APPCOMMAND command ids into a command id that the browser
  // knows how to handle.
  int command_id_from_app_command = GetCommandIDForAppCommandID(command_id);
  if (command_id_from_app_command != -1)
    command_id = command_id_from_app_command;

  return chrome::ExecuteCommand(window()->browser(), command_id);
}

// NOTE(pettern@vivaldi.com): Returning empty icons will make Windows
// grab the icons from the resource section instead, fixing VB-34191.
gfx::ImageSkia VivaldiNativeAppWindowViewsWin::GetWindowAppIcon() {
  return gfx::ImageSkia();
}

gfx::ImageSkia VivaldiNativeAppWindowViewsWin::GetWindowIcon() {
  return gfx::ImageSkia();
}
