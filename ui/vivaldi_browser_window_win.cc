// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#include "ui/vivaldi_browser_window.h"

#include <shlobj.h>
#include <shobjidl.h>

#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
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

namespace {

void PinShortcutToTaskbarOnWorkerThread(const std::wstring& app_model_id) {
  const wchar_t kVivaldiKey[] = L"Software\\Vivaldi";
  const wchar_t kVivaldiPinToTaskbarValue[] = L"EnablePinToTaskbar";

  base::win::RegKey key_ptt(HKEY_CURRENT_USER, kVivaldiKey, KEY_ALL_ACCESS);
  if (!key_ptt.Valid())
    return;

  DWORD reg_pin_to_taskbar_enabled = 0;
  key_ptt.ReadValueDW(kVivaldiPinToTaskbarValue, &reg_pin_to_taskbar_enabled);

  if (reg_pin_to_taskbar_enabled != 0) {
    base::FilePath current_exe_path;
    if (!base::PathService::Get(base::FILE_EXE, &current_exe_path)) {
      NOTREACHED();
      return;
    }
    ShellUtil::ShortcutProperties shortcut_properties(ShellUtil::CURRENT_USER);
    ShellUtil::AddDefaultShortcutProperties(current_exe_path,
                                            &shortcut_properties);
    // This is used to identify which jumplist that is updated. See
    // JumpList::CreateNewJumpListAndNotifyOS etc.
    shortcut_properties.set_app_id(app_model_id);
    shortcut_properties.set_shortcut_name(L"Vivaldi");

    const CLSID toast_activator_clsid =
        vivaldi::GetOrGenerateToastActivatorCLSID(&current_exe_path);
    shortcut_properties.set_toast_activator_clsid(toast_activator_clsid);
    shortcut_properties.set_pin_to_taskbar(true);

    ShellUtil::ShortcutLocation location =
        ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT;
    ShellUtil::ShortcutOperation operation =
        ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL;
    bool pinned = false;
    bool success = ShellUtil::CreateOrUpdateShortcut(
        location, shortcut_properties, operation, &pinned);
    if (success && pinned) {
      // only pin once, typically on first run
      key_ptt.WriteValue(kVivaldiPinToTaskbarValue, DWORD(0));
    }
  }
}

// Prepare the window to work with Jump List code in
// chromium/chrome/browser/win/jumplist.h
void InitializeForJumpList(Profile* profile, HWND hwnd) {
  std::wstring app_name = base::UTF8ToWide(
      web_app::GenerateApplicationNameFromAppId(vivaldi::kVivaldiAppId));

  std::wstring app_model_id =
      shell_integration::win::GetAppUserModelIdForBrowser(profile->GetPath());

  ui::win::SetAppIdForWindow(app_model_id, hwnd);
  // web_app::UpdateRelaunchDetailsForApp removed as it would change
  // the name of the running app to vivaldi_proxy.exe. See VB-72821.

  if (CanPinShortcutToTaskbar()) {
    // This is not available on Windows 10 and later.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&PinShortcutToTaskbarOnWorkerThread, app_model_id));
  }
}

}  // namespace

void VivaldiBrowserWindow::SetupShellIntegration(
    const VivaldiBrowserWindowParams& create_params) {
  HWND hwnd = views::HWNDForWidget(widget_->GetTopLevelWidget());

  InitializeForJumpList(GetProfile(), hwnd);

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