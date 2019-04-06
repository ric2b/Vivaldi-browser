// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "ui/views/vivaldi_pin_shortcut.h"

#include "build/build_config.h"
#if defined(OS_WIN)

#include <shlobj.h>

#include <string>

#include "app/vivaldi_apptools.h"
#include "base/path_service.h"
#include "base/task_scheduler/post_task.h"
#include "base/win/registry.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_helpers.h"
#include "chrome/browser/web_applications/web_app_win.h"
#include "ui/vivaldi_browser_window.h"

namespace vivaldi {

void VivaldiShortcutPinToTaskbar(const base::string16& app_id) {
  const wchar_t kVivaldiKey[] = L"Software\\Vivaldi";
  const wchar_t kVivaldiPinToTaskbarValue[] = L"EnablePinToTaskbar";

  if (base::win::GetVersion() >= base::win::VERSION_WIN7 &&
      base::win::GetVersion() < base::win::VERSION_WIN10) {
    base::win::RegKey key_ptt(HKEY_CURRENT_USER, kVivaldiKey, KEY_ALL_ACCESS);
    if (!key_ptt.Valid())
      return;

    DWORD reg_pin_to_taskbar_enabled = 0;
    key_ptt.ReadValueDW(kVivaldiPinToTaskbarValue, &reg_pin_to_taskbar_enabled);
    if (reg_pin_to_taskbar_enabled != 0) {
      wchar_t system_buffer[MAX_PATH] = {0};
      if (FAILED(SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return;

      base::FilePath shortcut_link(system_buffer);
      shortcut_link = shortcut_link.AppendASCII("Vivaldi.lnk");
      // now apply the correct app id for the shortcut link
      base::win::ShortcutProperties props;
      props.set_app_id(app_id);
      props.options = base::win::ShortcutProperties::PROPERTIES_APP_ID;
      bool success = base::win::CreateOrUpdateShortcutLink(
          shortcut_link, props, base::win::SHORTCUT_UPDATE_EXISTING);
      if (success) {
        // pin the modified shortcut link to the taskbar
        success = base::win::PinShortcutToTaskbar(shortcut_link);
        if (success) {
          // only pin once, typically on first run
          key_ptt.WriteValue(kVivaldiPinToTaskbarValue, DWORD(0));
        }
      }
    }
  }
}

void StartPinShortcutToTaskbar(VivaldiBrowserWindow* window) {
  DCHECK(window);
  std::string app_name = web_app::GenerateApplicationNameFromExtensionId(
      window->extension()->id());
  base::string16 app_name_wide;
  app_name_wide.assign(app_name.begin(), app_name.end());
  base::PostTaskWithTraits(
      FROM_HERE, { base::MayBlock() },
      base::Bind(&VivaldiShortcutPinToTaskbar, app_name_wide));
}

}  // namespace vivaldi

#endif  // OS_WIN
