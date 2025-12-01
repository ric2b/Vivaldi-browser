// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_ARC_MANAGER_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_ARC_MANAGER_DBUS_CONSTANTS_H_

namespace arc_manager {

inline constexpr char kArcManagerInterface[] = "org.chromium.ArcManager";
inline constexpr char kArcManagerServicePath[] = "/org/chromium/ArcManager";
inline constexpr char kArcManagerServiceName[] = "org.chromium.ArcManager";

// Methods
inline constexpr char kArcManagerOnUserSessionStarted[] =
    "OnUserSessionStarted";
inline constexpr char kArcManagerStartArcMiniContainer[] =
    "StartArcMiniContainer";
inline constexpr char kArcManagerUpgradeArcContainer[] = "UpgradeArcContainer";
inline constexpr char kArcManagerStopArcInstance[] = "StopArcInstance";
inline constexpr char kArcManagerSetArcCpuRestriction[] =
    "SetArcCpuRestriction";
inline constexpr char kArcManagerEmitArcBooted[] = "EmitArcBooted";
inline constexpr char kArcManagerGetArcStartTimeTicks[] =
    "GetArcStartTimeTicks";
inline constexpr char kArcManagerEnableAdbSideload[] = "EnableAdbSideload";
inline constexpr char kArcManagerQueryAdbSideload[] = "QueryAdbSideload";

// Signals
inline constexpr char kArcManagerArcInstanceStopped[] = "ArcInstanceStopped";

enum class ContainerCpuRestrictionState {
  kForeground = 0,
  kBackground = 1,
};
inline constexpr size_t kNumContainerCpuRestrictionStates = 2;

enum class ArcContainerStopReason {
  // The ARC container is crashed.
  kCrash = 0,

  // Stopped by the user request, e.g. disabling ARC.
  kUserRequest = 1,

  // Session manager is shut down. So, ARC is also shut down along with it.
  kSessionManagerShutdown = 2,

  // Browser was shut down. ARC is also shut down along with it.
  kBrowserShutdown = 3,

  // Disk space is too small to upgrade ARC.
  kLowDiskSpace = 4,

  // Failed to upgrade ARC mini container into full container.
  // Note that this will be used if the reason is other than low-disk-space.
  kUpgradeFailure = 5,
};

// Note: For historical reasons, currently ArcManager uses the error code
// defined in login_manager namespace for SessionManager.
// TODO(b:390297821): Define ArcManager's error status and migrate.

}  // namespace arc_manager

#endif  // SYSTEM_API_DBUS_ARC_MANAGER_DBUS_CONSTANTS_H_
