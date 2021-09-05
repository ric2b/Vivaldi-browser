// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_initializer.h"

#include <mfapi.h>

#include "base/logging.h"
#include "base/win/windows_version.h"

#include "base/optional.h"

namespace media {
// Media Foundation version number has last changed with Windows 7, see
// mfapi.h.
const int kMFVersionVista = (0x0001 << 16 | MF_API_VERSION);
const int kMFVersionWin7 = (0x0002 << 16 | MF_API_VERSION);

MFSessionLifetime InitializeMediaFoundation() {
  if (MFStartup(base::win::GetVersion() >= base::win::Version::WIN7
                    ? kMFVersionWin7
                    : kMFVersionVista,
                MFSTARTUP_LITE) == S_OK)
    return std::make_unique<MFSession>();
  DVLOG(1) << "Media Foundation unavailable or it failed to initialize";
  return nullptr;
}

MFSession::~MFSession() {
  MFShutdown();
}

}  // namespace media
