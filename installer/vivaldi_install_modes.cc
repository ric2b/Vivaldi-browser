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
        L"",  // Empty default channel name since no update integration.
        ChannelStrategy::UNSUPPORTED,
        true,  // Supports system-level installs.
        true,   // Supports in-product set as default browser UX.
        false,  // Does not support retention experiments.
        false,  // Supports multi-install.
        icon_resources::kApplicationIndex,  // App icon resource index.
        IDR_MAINFRAME,                      // App icon resource id.
  },
};

static_assert(_countof(kInstallModes) == NUM_INSTALL_MODES,
              "Imbalance between kInstallModes and InstallConstantIndex");

}  // namespace install_static
