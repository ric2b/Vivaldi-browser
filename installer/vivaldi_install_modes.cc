// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brand-specific constants and install modes for Google Chrome.

#include <stdlib.h>

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/install_static/install_modes.h"
#include "installer/vivaldi_install_modes.h"

namespace install_static {

const wchar_t kCompanyPathName[] = L"";

const wchar_t kProductPathName[] = L"Vivaldi";

const size_t kProductPathNameLength = _countof(kProductPathName) - 1;

const wchar_t kBinariesAppGuid[] = L"";

const wchar_t kBinariesPathName[] = L"";

const InstallConstants kInstallModes[] = {
    // The primary install mode for stable Google Chrome.
    {
        sizeof(kInstallModes[0]),
        VIVALDI_INDEX,
        "",  // Empty install_suffix for the primary install mode.
        L"",
        L"",  // The empty string means "stable".
        L"",          // Empty app_guid since no integraion with Google Update.
        L"Vivaldi",  // A distinct base_app_name.
        L"Vivaldi",  // A distinct base_app_id.
        L"VivaldiHTM",                             // ProgID prefix.
        L"Vivaldi HTML Document",                  // ProgID description.
        L"{9C142C0C-124C-4467-B117-EBCC62801D7B}",  // Active Setup GUID.
        L"{DAB968E0-3A13-4CCC-A3AF-85578ACBE9AB}",  // CommandExecuteImpl CLSID.
        {0xBCA9D37C,
        0xCA60,
        0x4160,
        {0x91, 0x15, 0x97, 0xA0, 0x0F, 0x24, 0x70,
         0x2D}},  // Toast Activator CLSID.
        {0x412E5152,
        0x7091,
        0x4930,
        {0x92, 0xBD, 0x6A, 0x33, 0x9A, 0xE9, 0x07, 0x06}},  // Elevator CLSID.
        L"",      // Empty default channel name since no update integration.
        ChannelStrategy::UNSUPPORTED,
        true,  // Supports system-level installs.
        true,   // Supports in-product set as default browser UX.
        false,  // Does not support retention experiments.
        false,  // Supports multi-install.
        icon_resources::kApplicationIndex,  // App icon resource index.
        IDR_MAINFRAME,                      // App icon resource id.
        L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
        L"934012048-",
  },
};

static_assert(_countof(kInstallModes) == NUM_INSTALL_MODES,
              "Imbalance between kInstallModes and InstallConstantIndex");

}  // namespace install_static
