// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_FLATPAK_SANDBOX_H_
#define SANDBOX_LINUX_SERVICES_FLATPAK_SANDBOX_H_

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/no_destructor.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "sandbox/linux/services/flatpak_pid_map.h"
#include "sandbox/sandbox_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace vivaldi {
namespace sandbox {

// Manages the state of and access to the Flatpak sandbox.
// Note that there is a distinction between external and internal PIDs:
// - External PIDs are the PIDs relative to the world outside the sandbox.
// - Internal PIDs are the PIDs relative to the current PID namespace.
// Flatpak's sandbox APIs work primarily with external PIDs, and an
// internal PID must be retrieved from the SpawnStarted signal before
// it is known inside the sandbox's PID namespace.
class SANDBOX_EXPORT FlatpakSandbox {
 public:
  class SpawnOptions {
   public:
    SpawnOptions();
    ~SpawnOptions();
    SpawnOptions(const SpawnOptions& other) = delete;
    SpawnOptions(SpawnOptions&& other) = delete;

    bool ExposePathRo(base::FilePath path);

   private:
    friend class FlatpakSandbox;

    std::vector<base::ScopedFD> sandbox_expose_ro;
  };

  static FlatpakSandbox* GetInstance();

  // Represents the level of sandboxing inside a Flatpak. kNone means this is
  // not a Flatpak, kFlatpak means it's inside a Flatpak sandbox, and
  // kRestricted means that this is inside a nested Flatpak sandbox with most
  // permissions revoked.
  enum class SandboxLevel { kNone, kFlatpak, kRestricted };

  // Get the current level of sandboxing in this Flatpak.
  SandboxLevel GetSandboxLevel();

  // Returns whether or not the given PID was spawned via the Flatpak sandbox.
  bool IsPidSandboxed(base::ProcessId relative_pid);

  // Launch the given process inside of a Flatpak sandbox. If allow_x11 is true,
  // then the process will be given access to the host's X11 display. On
  // failure, returns kNullProcessId. Note that the return value is the PID
  // relative to the host i.e. outside the sandbox, to get the internal one call
  // GetRelativePid. This is the reason why a vanilla ProcessId is returned
  // rather than a base::Process instance.
  base::Process LaunchProcess(const base::CommandLine& cmdline,
                              const base::LaunchOptions& launch_options,
                              const SpawnOptions& spawn_options = {});

  // Indefinitely waits for the given process and fills the exit code pointer
  // if given and non-null. Returns false on wait failure.
  bool Wait(base::ProcessId relative_pid, int* exit_code);

  // Skips storing the exit status of the given PID.
  void IgnoreExitStatus(base::ProcessId relative_pid);

 private:
  friend class base::NoDestructor<FlatpakSandbox>;

  FlatpakSandbox();
  FlatpakSandbox(const FlatpakSandbox&) = delete;
  FlatpakSandbox(FlatpakSandbox&&) = delete;
  ~FlatpakSandbox();

  void StartBusThread();
  dbus::Bus* AcquireBusFromBusThread();
  dbus::ObjectProxy* GetPortalObjectProxy();

  void InitializeBusThread();
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool connected);
  void OnSpawnStartedSignal(dbus::Signal* signal);
  void OnSpawnExitedSignal(dbus::Signal* signal);

  base::ProcessId Spawn(const base::CommandLine& cmdline,
                        const base::LaunchOptions& launch_options,
                        const SpawnOptions& spawn_options);
  void SpawnOnBusThread(base::ProcessId* out_external_pid,
                        base::WaitableEvent* event,
                        const base::CommandLine* cmdline,
                        const base::LaunchOptions* launch_options,
                        const SpawnOptions* spawn_options);
  void OnSpawnResponse(base::ProcessId* out_external_pid,
                       base::WaitableEvent* event,
                       dbus::Response* response,
                       dbus::ErrorResponse* error_response);

  base::ProcessId GetRelativePid(base::ProcessId external_pid);

  absl::optional<SandboxLevel> sandbox_level_;
  base::Thread bus_thread_;

  base::Lock process_info_lock_;
  // Note that broadcast is used in the source, because in general
  // very few threads will be contending for the lock.
  base::ConditionVariable process_info_cv_;
  // Set of processes that have no associated relative PID yet.
  base::flat_set<base::ProcessId> unmapped_processes_;
  // Map of running processes.
  FlatpakPidMap running_processes_;
  // Map of a relative process ID that has exited to its waitpid status.
  std::map<base::ProcessId, int> exited_process_statuses_;
  // Relative process IDs that should have their statuses ignored on exit.
  std::set<base::ProcessId> ignore_status_;
};

}  // namespace sandbox
} // namespace vivaldi

#endif  // SANDBOX_LINUX_SERVICES_FLATPAK_SANDBOX_H_
