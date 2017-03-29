// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_initializer.h"

#include <mfapi.h>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/win/windows_version.h"

namespace media {

namespace {

// Media Foundation version number has last changed with Windows 7, see
// mfapi.h.
const int kMFVersionVista = (0x0001 << 16 | MF_API_VERSION);
const int kMFVersionWin7 = (0x0002 << 16 | MF_API_VERSION);

// LazyInstance to initialize the Media Foundation Library.
class MFInitializer {
 public:
  MFInitializer()
      : mf_started_(MFStartup(base::win::GetVersion() >= base::win::VERSION_WIN7
                                  ? kMFVersionWin7
                                  : kMFVersionVista,
                              MFSTARTUP_LITE) == S_OK) {}

  ~MFInitializer() {
    if (mf_started_)
      MFShutdown();
  }

 private:
  const bool mf_started_;

  DISALLOW_COPY_AND_ASSIGN(MFInitializer);
};

base::LazyInstance<MFInitializer> g_mf_initializer = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void InitializeMediaFoundation() {
  g_mf_initializer.Get();
}

}  // namespace media
