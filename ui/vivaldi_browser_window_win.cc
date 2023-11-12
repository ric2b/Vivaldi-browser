// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#include "ui/vivaldi_browser_window.h"

#include <shlobj.h>
#include <shobjidl.h>

#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/installer/util/taskbar_util.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"

#include "app/vivaldi_constants.h"
#include "chrome/installer/util/shell_util.h"
#include "installer/vivaldi_install_modes.h"


// Prepare the window to work with Jump List code in
// chromium/chrome/browser/win/jumplist.h
void VivaldiBrowserWindow::SetupShellIntegration(
    const VivaldiBrowserWindowParams& create_params) {
  HWND hwnd = views::HWNDForWidget(widget_->GetTopLevelWidget());

  std::wstring app_model_id =
      shell_integration::win::GetAppUserModelIdForBrowser(GetProfile()->GetPath());

  ui::win::SetAppIdForWindow(app_model_id, hwnd);

  if (!create_params.alpha_enabled) {
    // Windows seems to have issues maximizing windows without WS_CAPTION.
    // The default views / Aura implementation will remove this if we are using
    // frameless or colored windows, so we put it back here.
    int current_style = ::GetWindowLong(hwnd, GWL_STYLE);
    ::SetWindowLong(hwnd, GWL_STYLE, current_style | WS_CAPTION);
  }
}

int VivaldiBrowserWindow::GetCommandIDForAppCommandID(
    int app_command_id) const {
  // See BrowserView::GetCommandIDForAppCommandID()
  switch (app_command_id) {
    case APPCOMMAND_BROWSER_REFRESH:
      return IDC_RELOAD;
    case APPCOMMAND_BROWSER_HOME:
      return IDC_HOME;
    case APPCOMMAND_BROWSER_STOP:
      return IDC_STOP;
    case APPCOMMAND_BROWSER_SEARCH:
      return IDC_FOCUS_SEARCH;
    case APPCOMMAND_HELP:
      return IDC_HELP_PAGE_VIA_KEYBOARD;
    case APPCOMMAND_NEW:
      return IDC_NEW_TAB;
    case APPCOMMAND_OPEN:
      return IDC_OPEN_FILE;
    case APPCOMMAND_CLOSE:
      return IDC_CLOSE_TAB;
    case APPCOMMAND_SAVE:
      return IDC_SAVE_PAGE;
    case APPCOMMAND_PRINT:
      return IDC_PRINT;
    case APPCOMMAND_COPY:
      return IDC_COPY;
    case APPCOMMAND_CUT:
      return IDC_CUT;
    case APPCOMMAND_PASTE:
      return IDC_PASTE;
      // TODO(pkasting): http://b/1113069 Handle these.
    case APPCOMMAND_UNDO:
    case APPCOMMAND_REDO:
    case APPCOMMAND_SPELL_CHECK:
    // Handled in WebViewGuest::HandleKeyboardShortcuts:
    case APPCOMMAND_BROWSER_BACKWARD:
    case APPCOMMAND_BROWSER_FORWARD:
    default:
      break;
  }
  return -1;
}