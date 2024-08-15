// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/android_image_reader_compat.h"

// Vivaldi
#if defined(VIVALDI_BUILD) && defined(OEM_AUTOMOTIVE_BUILD)
#include "base/android/build_info.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include <string>
#endif // VIVALDI_BUILD && OEM_AUTOMOTIVE_BUILD

namespace base {
namespace android {

bool EnableAndroidImageReader() {
  // AImageReader is causing https://bugs.vivaldi.com/browse/AUTO-82.
  // Set as disabled for Polestar and Volvo.
  #if defined(VIVALDI_BUILD) && defined(OEM_AUTOMOTIVE_BUILD)
  std::string brand(BuildInfo::GetInstance()->brand());
  if (StartsWith(brand, "Polestar", CompareCase::INSENSITIVE_ASCII) ||
          StartsWith(brand, "Volvo", CompareCase::INSENSITIVE_ASCII)) {
    LOG(WARNING) << __func__
        << ": AndroidImageReader unsupported on " << brand;
    return false;
  }
  #endif // VIVALDI_BUILD && OEM_AUTOMOTIVE_BUILD
  // Currently we want to enable AImageReader only for android P+ devices.
  if (__builtin_available(android 28, *)) {
    return true;
  }
  return false;
}

}  // namespace android
}  // namespace base
