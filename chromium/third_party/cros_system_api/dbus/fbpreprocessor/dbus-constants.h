// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_FBPREPROCESSOR_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_FBPREPROCESSOR_DBUS_CONSTANTS_H_

namespace fbpreprocessor {

inline constexpr char kFbPreprocessorServiceName[] =
    "org.chromium.FbPreprocessor";
inline constexpr char kFbPreprocessorServicePath[] =
    "/org/chromium/FbPreprocessor";

inline constexpr char kDaemonName[] = "fbpreprocessord";

// Daemon-store directories for fbpreprocessord
inline constexpr char kDaemonStorageRoot[] =
    "/run/daemon-store/fbpreprocessord";
inline constexpr char kInputDirectory[] = "raw_dumps";
inline constexpr char kProcessedDirectory[] = "processed_dumps";
inline constexpr char kScratchDirectory[] = "scratch";

}  // namespace fbpreprocessor

#endif  // SYSTEM_API_DBUS_FBPREPROCESSOR_DBUS_CONSTANTS_H_
