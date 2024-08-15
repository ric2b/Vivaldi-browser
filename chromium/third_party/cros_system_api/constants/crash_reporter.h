// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_CONSTANTS_CRASH_REPORTER_H_
#define SYSTEM_API_CONSTANTS_CRASH_REPORTER_H_

namespace crash_reporter {

// Subdirectory that Chrome browser writes to in order to indicate that
// crashpad is ready and ChromeOS's crash reporter doesn't need to use early
// crash handler in UserCollector.
inline constexpr char kCrashpadReadyDirectory[] =
    "/run/crash_reporter/crashpad_ready";

}  // namespace crash_reporter

#endif  // SYSTEM_API_CONSTANTS_CRASH_REPORTER_H_
