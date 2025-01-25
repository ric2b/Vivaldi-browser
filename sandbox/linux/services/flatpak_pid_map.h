// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_FLATPAK_PID_MAP_H_
#define SANDBOX_LINUX_SERVICES_FLATPAK_PID_MAP_H_

#include "base/containers/flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace vivaldi {
namespace sandbox {

// A bidirectional map of external PIDs and relative PIDs for the Flatpak
// sandbox. "External" PIDs are the PID values that Flatpak's Spawn API returns,
// relative to the host system, and "relative" PIDs are the PIDs those processes
// are known by from inside the sandbox.
class FlatpakPidMap {
 public:
  struct PidPair {
    pid_t external;
    pid_t relative;
  };

  FlatpakPidMap();
  ~FlatpakPidMap();

  bool Insert(PidPair pair);

  absl::optional<pid_t> FindRelativeByExternal(pid_t external);
  absl::optional<pid_t> FindExternalByRelative(pid_t relative);

  absl::optional<pid_t> DeleteByRelative(pid_t relative);
  absl::optional<pid_t> DeleteByExternal(pid_t external);

 private:
  absl::optional<pid_t> FindImpl(base::flat_map<pid_t, pid_t>* map, pid_t key);
  absl::optional<pid_t> DeleteImpl(base::flat_map<pid_t, pid_t>* map,
                                   base::flat_map<pid_t, pid_t>* reversed,
                                   pid_t key);

  base::flat_map<pid_t, pid_t> external_to_relative_;
  base::flat_map<pid_t, pid_t> relative_to_external_;
};  // namespace sandbox

}  // namespace sandbox
}  // namespace vivaldi

#endif
