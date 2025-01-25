// Copyright (c) 2015-2022 Vivaldi Technologies AS. All rights reserved
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brand-specific constants and install modes for Google Chrome.

#include <objbase.h>
#include <stdlib.h>

#include <Rpc.h>

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/install_util.h"
#include "installer/vivaldi_install_modes.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_constants.h"

#include "installer/util/vivaldi_install_constants.h"


namespace vivaldi {

CLSID GetOrGenerateToastActivatorCLSID(const base::FilePath* target_path /*null*/) {
  UUID toast_activator_clsid = CLSID_NULL;


  base::FilePath target_exe;
  if (target_path) {
    target_exe = base::FilePath(*target_path);
  }

  // Debug and testing, set to a vivaldi.exe in a installed instance.
  //target_exe = base::FilePath(L"C:\\test_5\\Application\\vivaldi.exe");

  base::win::RegKey registry_key(
      HKEY_CURRENT_USER, vivaldi::constants::kVivaldiToastActivatorCLSID,
      KEY_READ);
  std::wstring clsid;
  bool value_exists =
      registry_key.ReadValue(
          reinterpret_cast<const wchar_t*>(target_exe.AsUTF16Unsafe().c_str()),
          &clsid) == ERROR_SUCCESS;

  if (value_exists) {
    CLSIDFromString(clsid.c_str(), &toast_activator_clsid);
  } else {

    // Only create a valid uuid if we have the target path available. This is to
    // avoid writing registry keys when this method is accessed by the installer
    // using temporary paths.
    if (target_path) {
      UuidCreate(&toast_activator_clsid);

      std::wstring toastActivatorClsid =
          base::win::WStringFromGUID(toast_activator_clsid);

      DLOG(INFO) << " GetOrGenerateToastActivatorCLSID created new value and "
                    "writing to registry "
                 << toastActivatorClsid;

      HKEY root = install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                    : HKEY_CURRENT_USER;
      base::win::RegKey clsidfortarget_key(
          root, vivaldi::constants::kVivaldiToastActivatorCLSID,
          KEY_SET_VALUE);

      LONG ret = clsidfortarget_key.WriteValue(
          reinterpret_cast<const wchar_t*>(target_exe.AsUTF16Unsafe().c_str()),
          toastActivatorClsid.c_str());

      if (ret != ERROR_SUCCESS) {
        LOG(ERROR) << ret
                   << " Failed to write to registry : " << toastActivatorClsid;
      }
    }
  }

  return toast_activator_clsid;
}
}  // namespace vivaldi

namespace install_static {

const wchar_t kCompanyPathName[] = L"";

const wchar_t kProductPathName[] = L"Vivaldi";

const size_t kProductPathNameLength = _countof(kProductPathName) - 1;

const char kSafeBrowsingName[] = "vivaldi";

const InstallConstants kInstallModes[] = {
    // The primary install mode for stable Google Chrome.
    {
        .size = sizeof(kInstallModes[0]),
        .index = VIVALDI_INDEX,
        .install_switch =
            "",  // Empty install_suffix for the primary install mode.
        .install_suffix =
            L"",
        .logo_suffix = L"",         // The empty string means "stable".
        .app_guid =
            L"",         // Empty app_guid since no integraion with Google Update.
        .base_app_name = L"Vivaldi",  // A distinct base_app_name.
        .base_app_id = L"Vivaldi",  // A distinct base_app_id.
        .browser_prog_id_prefix = L"VivaldiHTM",    // ProgID prefix.
        .browser_prog_id_description =
            L"Vivaldi HTML Document",               // ProgID description.
        .pdf_prog_id_prefix = L"VivaldiPPDF",       // PDF ProgID prefix.
        .pdf_prog_id_description =
            L"Vivaldi PDF Document",                // PDF ProgID description.
        .active_setup_guid =
            L"{9C142C0C-124C-4467-B117-EBCC62801D7B}",  // Active Setup GUID.
        .legacy_command_execute_clsid =
            L"{DAB968E0-3A13-4CCC-A3AF-85578ACBE9AB}",  // CommandExecuteImpl CLSID.

        .toast_activator_clsid =
            vivaldi::GetOrGenerateToastActivatorCLSID(),  // Toast Activator CLSID.

        .elevator_clsid = {0x412E5152,
                           0x7091,
                           0x4930,
                           {0x92, 0xBD, 0x6A, 0x33, 0x9A, 0xE9, 0x07,
                            0x06}},  // Elevator CLSID.
        .elevator_iid = {},   // IID elevator_iid;
        .default_channel_name =
            L"",  // Empty default channel name since no update integration.
        .channel_strategy = ChannelStrategy::UNSUPPORTED,
        .supports_system_level = true,   // Supports system-level installs.
        .supports_set_as_default_browser =
            true,   // Supports in-product set as default browser UX.
        .app_icon_resource_index =
            icon_resources::kApplicationIndex,  // App icon resource index.
        .app_icon_resource_id = IDR_MAINFRAME,  // App icon resource id.
        .html_doc_icon_resource_index =
            icon_resources::kHtmlDocIndex,  // HTML doc icon resource index.
        .pdf_doc_icon_resource_index =
            icon_resources::kPDFDocIndex,  // PDF doc icon resource index.
        .sandbox_sid_prefix =
            L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
            L"934012048-",
    },
};

static_assert(_countof(kInstallModes) == NUM_INSTALL_MODES,
              "Imbalance between kInstallModes and InstallConstantIndex");

}  // namespace install_static
