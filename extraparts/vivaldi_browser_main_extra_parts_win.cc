// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_browser_main_extra_parts.h"

#include <shlobj.h>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_switches.h"

#define GOOGLE_CHROME_BASE_DIRECTORY L"Google\\Chrome\\Application"

// Overridden from ChromeBrowserMainExtraParts:
void VivaldiBrowserMainExtraParts::PostEarlyInitializationWin() {
  base::FilePath path;  // chromeWindowsPPapiFlashPath
  // 1 Use Adobes installation if present.
  base::win::RegKey adobepepperflash(
      HKEY_LOCAL_MACHINE, L"SOFTWARE\\Macromedia\\FlashPlayerPepper", KEY_READ);
  if (adobepepperflash.Valid()) {
    base::string16 adobepepper_path;
    adobepepperflash.ReadValue(L"PlayerPath", &adobepepper_path);
    if (base::PathExists(base::FilePath(adobepepper_path))) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchNative(
          switches::kPpapiFlashPath, adobepepper_path);
    }
  } else {
    // 2 Sniff out the newest Chrome version and use the PPAPI Flash player
    //  installed there. Check both "program files" and "program files (x86)"
    wchar_t system_buffer[MAX_PATH];
    system_buffer[0] = 0;
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL,
                                  SHGFP_TYPE_CURRENT, system_buffer))) {
      path = base::FilePath(system_buffer);
      path = path.Append(GOOGLE_CHROME_BASE_DIRECTORY);
      if (!base::PathExists(base::FilePath(path))) {
        if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILESX86, NULL,
                                      SHGFP_TYPE_CURRENT, system_buffer))) {
          path = base::FilePath(system_buffer);
          path = path.Append(GOOGLE_CHROME_BASE_DIRECTORY);
        }
      }
    }
    if (base::PathExists(base::FilePath(path))) {
      // Add the current version, should be the most recent I guess.
      base::win::RegKey versionkey(
          HKEY_CURRENT_USER, L"SOFTWARE\\Google\\Chrome\\BLBeacon", KEY_READ);
      if (versionkey.Valid()) {
        base::string16 current_version;
        versionkey.ReadValue(L"version", &current_version);
        path = path.Append(current_version);
        path = path.Append(L"PepperFlash\\pepflashplayer.dll");
        if (base::PathExists(base::FilePath(path))) {
          base::CommandLine::ForCurrentProcess()->AppendSwitchNative(
              switches::kPpapiFlashPath, path.AsUTF16Unsafe());
        }
      }
    }
  }
}
