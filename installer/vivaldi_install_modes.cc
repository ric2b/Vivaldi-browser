// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brand-specific constants and install modes for Google Chrome.

#include <stdlib.h>

#include "chrome/install_static/install_modes.h"
#include "installer/vivaldi_install_modes.h"

namespace install_static {

const wchar_t kCompanyPathName[] = L"Vivaldi";

const wchar_t kProductPathName[] = L"Vivaldi";

const size_t kProductPathNameLength = _countof(kProductPathName) - 1;

const wchar_t kBinariesAppGuid[] = L"";

const wchar_t kBinariesPathName[] = L"";

const InstallConstants kInstallModes[] = {
    // The primary install mode for stable Google Chrome.
    {
        sizeof(kInstallModes[0]),
        VIVALDI_INDEX,
        L"",  // Empty install_suffix for the primary install mode.
        L"",
        L"",  // The empty string means "stable".
        ChannelStrategy::UNSUPPORTED,
        true,  // Supports system-level installs.
        true,  // Supports multi-install.
    },
};

static_assert(_countof(kInstallModes) == NUM_INSTALL_MODES,
              "Imbalance between kInstallModes and InstallConstantIndex");

}  // namespace install_static
