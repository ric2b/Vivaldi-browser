// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/flatpak_sandbox.h"

#include <fcntl.h>
#include <signal.h>
#include <sstream>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "sandbox/linux/services/flatpak_pid_map.h"

namespace vivaldi {
namespace sandbox {

namespace {
const base::FilePath kFlatpakAppPath("/app");
const base::FilePath kFlatpakInfoPath("/.flatpak-info");

const char kFlatpakPortalServiceName[] = "org.freedesktop.portal.Flatpak";
const char kFlatpakPortalObjectPath[] = "/org/freedesktop/portal/Flatpak";
const char kFlatpakPortalInterfaceName[] = "org.freedesktop.portal.Flatpak";

#ifndef NDEBUG
const char kDisableFullFlatpakSandbox[] = "disable-full-flatpak-sandbox";
#endif

struct PortalProperties : dbus::PropertySet {
  dbus::Property<uint32_t> version;
  dbus::Property<uint32_t> supports;

  enum FlatpakPortalSupports {
    kFlatpakPortal_ExposePids = 1 << 0,
  };

  explicit PortalProperties(dbus::ObjectProxy* object_proxy)
      : dbus::PropertySet(object_proxy, kFlatpakPortalInterfaceName, {}) {
    RegisterProperty("version", &version);
    RegisterProperty("supports", &supports);
  }

  ~PortalProperties() override = default;
};

void WriteStringAsByteArray(dbus::MessageWriter* writer,
                            const std::string& str) {
  writer->AppendArrayOfBytes(base::make_span(
      reinterpret_cast<const uint8_t*>(str.c_str()), str.size() + 1));
}

void WriteFdPairMap(dbus::MessageWriter* writer, int source_fd, int dest_fd) {
  dbus::MessageWriter entry_writer(nullptr);
  writer->OpenDictEntry(&entry_writer);

  entry_writer.AppendUint32(dest_fd);
  entry_writer.AppendFileDescriptor(source_fd);

  writer->CloseContainer(&entry_writer);
}

}  // namespace

enum FlatpakSpawnFlags {
  kFlatpakSpawn_ClearEnvironment = 1 << 0,
  kFlatpakSpawn_Latest = 1 << 1,
  kFlatpakSpawn_Sandbox = 1 << 2,
  kFlatpakSpawn_NoNetwork = 1 << 3,
  kFlatpakSpawn_WatchBus = 1 << 4,
  kFlatpakSpawn_ExposePids = 1 << 5,
  kFlatpakSpawn_NotifyStart = 1 << 6,
};

enum FlatpakSpawnSandboxFlags {
  kFlatpakSpawnSandbox_ShareDisplay = 1 << 0,
  kFlatpakSpawnSandbox_ShareSound = 1 << 1,
  kFlatpakSpawnSandbox_ShareGpu = 1 << 2,
  kFlatpakSpawnSandbox_ShareSessionBus = 1 << 3,
  kFlatpakSpawnSandbox_ShareA11yBus = 1 << 4,
};

bool FlatpakSandbox::SpawnOptions::ExposePathRo(base::FilePath path) {
  base::ScopedFD fd(
      HANDLE_EINTR(open(path.value().c_str(), O_PATH | O_NOFOLLOW)));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to expose path " << path;
    return false;
  }

  sandbox_expose_ro.push_back(std::move(fd));
  return true;
}

FlatpakSandbox::FlatpakSandbox()
    : bus_thread_("FlatpakPortalBus"), process_info_cv_(&process_info_lock_) {}

FlatpakSandbox::FlatpakSandbox::SpawnOptions::SpawnOptions() = default;
FlatpakSandbox::FlatpakSandbox::SpawnOptions::~SpawnOptions() = default;

// static
FlatpakSandbox* FlatpakSandbox::GetInstance() {
  static base::NoDestructor<FlatpakSandbox> instance;
  return instance.get();
}

FlatpakSandbox::SandboxLevel FlatpakSandbox::GetSandboxLevel() {
  if (sandbox_level_) {
    return *sandbox_level_;
  }

  // XXX: These operations shouldn't actually have a major blocking time,
  // as .flatpak-info is on a tmpfs.
  base::VivaldiScopedAllowBlocking scoped_allow_blocking;

  if (!base::PathExists(kFlatpakInfoPath)) {
    sandbox_level_ = SandboxLevel::kNone;
  } else {
    // chrome has an INI parser, but sandbox can't depend on anything inside
    // chrome, so the .flatpak-info INI is manually checked for the sandbox
    // option.

    std::string contents;
    CHECK(ReadFileToString(kFlatpakInfoPath, &contents));
    DCHECK(!contents.empty());

    std::istringstream iss(contents);
    std::string line;
    bool in_instance = false;
    while (std::getline(iss, line)) {
      if (!line.empty() && line[0] == '[') {
        DCHECK(line.back() == ']');

        if (line == "[Instance]") {
          DCHECK(!in_instance);
          in_instance = true;
        } else if (in_instance) {
          // Leaving the Instance section, sandbox=true can't come now.
          break;
        }
      } else if (in_instance && line == "sandbox=true") {
        sandbox_level_ = SandboxLevel::kRestricted;
        break;
      }
    }

    if (!sandbox_level_) {
      sandbox_level_ = SandboxLevel::kFlatpak;
    }
  }

#ifndef NDEBUG
  if (sandbox_level_ == SandboxLevel::kFlatpak &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableFullFlatpakSandbox)) {
    sandbox_level_ = SandboxLevel::kRestricted;
  }
#endif

  return *sandbox_level_;
}

bool FlatpakSandbox::IsPidSandboxed(base::ProcessId relative_pid) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::AutoLock locker(process_info_lock_);

  return running_processes_.FindExternalByRelative(relative_pid).has_value();
}

base::Process FlatpakSandbox::LaunchProcess(
    const base::CommandLine& cmdline,
    const base::LaunchOptions& launch_options,
    const SpawnOptions& spawn_options /*= {}*/) {
  base::ProcessId external_pid = Spawn(cmdline, launch_options, spawn_options);
  if (external_pid == base::kNullProcessId) {
    return base::Process();
  }

  base::ProcessId relative_pid = GetRelativePid(external_pid);
  if (relative_pid == base::kNullProcessId) {
    // Treat early stops as a launch failure.
    return base::Process();
  }

  return base::Process(relative_pid);
}

bool FlatpakSandbox::Wait(base::ProcessId relative_pid, int* exit_code) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::AutoLock locker(process_info_lock_);

  for (;;) {
    if (running_processes_.FindExternalByRelative(relative_pid)) {
      // Process is still running.
      process_info_cv_.Wait();
      continue;
    }

    auto it = exited_process_statuses_.find(relative_pid);
    if (it == exited_process_statuses_.end()) {
      // This should only happen if another caller had marked the exit status
      // to be ignored. Treat it like waitpid returning ESRCH.
      LOG(ERROR) << "PID " << relative_pid << " had no exit status";
      return false;
    }

    if (exit_code) {
      *exit_code = it->second;
    }
    exited_process_statuses_.erase(it);
    return true;
  }
}

void FlatpakSandbox::IgnoreExitStatus(base::ProcessId relative_pid) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::AutoLock locker(process_info_lock_);

  CHECK(running_processes_.FindExternalByRelative(relative_pid));
  ignore_status_.insert(relative_pid);
}

void FlatpakSandbox::StartBusThread() {
  if (!bus_thread_.IsRunning()) {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    CHECK(bus_thread_.StartWithOptions(std::move(options)));

    bus_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FlatpakSandbox::InitializeBusThread,
                                  base::Unretained(this)));
  }
}

dbus::Bus* FlatpakSandbox::AcquireBusFromBusThread() {
  // Note that destruction of the bus is not a concern, because once the
  // thread dies its bus connection will be terminated anyway and the
  // portal will notice.
  static base::NoDestructor<scoped_refptr<dbus::Bus>> bus([] {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    options.connection_type = dbus::Bus::PRIVATE;
    options.dbus_task_runner = base::SequencedTaskRunner::GetCurrentDefault();

    return base::MakeRefCounted<dbus::Bus>(options);
  }());

  return bus->get();
}

dbus::ObjectProxy* FlatpakSandbox::GetPortalObjectProxy() {
  return AcquireBusFromBusThread()->GetObjectProxy(
      kFlatpakPortalServiceName, dbus::ObjectPath(kFlatpakPortalObjectPath));
}

void FlatpakSandbox::InitializeBusThread() {
  dbus::ObjectProxy* object_proxy = GetPortalObjectProxy();

  PortalProperties properties(object_proxy);
  properties.ConnectSignals();

  CHECK(properties.GetAndBlock(&properties.version))
      << "Failed to get portal version";
  CHECK(properties.GetAndBlock(&properties.supports))
      << "Failed to get portal supports";

  if (properties.version.value() < 4) {
    LOG(FATAL) << "Your Flatpak version is too old, please update it";
  }

  if (!(properties.supports.value() &
        PortalProperties::kFlatpakPortal_ExposePids)) {
    LOG(FATAL) << "Your Flatpak installation is setuid, which is not supported";
  }

  object_proxy->ConnectToSignal(
      kFlatpakPortalInterfaceName, "SpawnStarted",
      base::BindRepeating(&FlatpakSandbox::OnSpawnStartedSignal,
                          base::Unretained(this)),
      base::BindOnce(&FlatpakSandbox::OnSignalConnected,
                     base::Unretained(this)));

  object_proxy->ConnectToSignal(
      kFlatpakPortalInterfaceName, "SpawnExited",
      base::BindRepeating(&FlatpakSandbox::OnSpawnExitedSignal,
                          base::Unretained(this)),
      base::BindOnce(&FlatpakSandbox::OnSignalConnected,
                     base::Unretained(this)));
}

void FlatpakSandbox::OnSignalConnected(const std::string& interface,
                                       const std::string& signal,
                                       bool connected) {
  // It's not safe to spawn processes without being able to track their deaths.
  CHECK(connected) << "Failed to connect to signal " << signal;
}

void FlatpakSandbox::OnSpawnStartedSignal(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  uint32_t external_pid, relative_pid;

  if (!reader.PopUint32(&external_pid) || !reader.PopUint32(&relative_pid)) {
    LOG(ERROR) << "Invalid SpawnStarted signal";
    return;
  }

  VLOG(1) << "Received SpawnStarted: " << external_pid << ' ' << relative_pid;

  base::AutoLock locker(process_info_lock_);

  auto it = unmapped_processes_.find(external_pid);
  if (it == unmapped_processes_.end()) {
    LOG(ERROR) << "Process " << external_pid
               << " is already dead or not tracked";
    return;
  }

  unmapped_processes_.erase(it);

  // Don't try to map them if the process died too quickly (which is the cause
  // of relative_pid == 0).
  if (relative_pid != 0) {
    FlatpakPidMap::PidPair pair;
    pair.external = external_pid;
    pair.relative = relative_pid;
    running_processes_.Insert(pair);
  }

  process_info_cv_.Broadcast();
}

void FlatpakSandbox::OnSpawnExitedSignal(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  uint32_t external_pid, exit_status;

  if (!reader.PopUint32(&external_pid) || !reader.PopUint32(&exit_status)) {
    LOG(ERROR) << "Invalid SpawnExited signal";
    return;
  }

  VLOG(1) << "Received SpawnExited: " << external_pid << ' ' << exit_status;

  base::AutoLock locker(process_info_lock_);

  auto relative_pid = running_processes_.DeleteByExternal(external_pid);
  // If this isn't found, it likely never ran long enough for SpawnStarted to be
  // emitted, so we never bother saving the exit status.
  if (relative_pid) {
    auto ignore_it = ignore_status_.find(*relative_pid);
    if (ignore_it != ignore_status_.end()) {
      // Make sure the exit status is not set.
      relative_pid.reset();
      ignore_status_.erase(ignore_it);
    }
  }

  if (relative_pid) {
    exited_process_statuses_[*relative_pid] = exit_status;
  }

  process_info_cv_.Broadcast();
}

base::ProcessId FlatpakSandbox::Spawn(const base::CommandLine& cmdline,
                                      const base::LaunchOptions& launch_options,
                                      const SpawnOptions& spawn_options) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedAllowBaseSyncPrimitives allow_wait;

  StartBusThread();

  VLOG(1) << "Running via Flatpak: " << cmdline.GetCommandLineString();

  DCHECK(GetSandboxLevel() != SandboxLevel::kNone);

  // These options are not supported with the Flatpak sandbox.
  DCHECK(launch_options.clone_flags == 0);
  DCHECK(!launch_options.wait);
  DCHECK(!launch_options.allow_new_privs);
  DCHECK(launch_options.real_path.empty());
  DCHECK(launch_options.pre_exec_delegate == nullptr);
  DCHECK(launch_options.maximize_rlimits == nullptr);

  base::ProcessId external_pid = base::kNullProcessId;
  base::WaitableEvent event;

  bus_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FlatpakSandbox::SpawnOnBusThread, base::Unretained(this),
                     base::Unretained(&external_pid), base::Unretained(&event),
                     base::Unretained(&cmdline),
                     base::Unretained(&launch_options),
                     base::Unretained(&spawn_options)));
  event.Wait();

  return external_pid;
}

void FlatpakSandbox::SpawnOnBusThread(base::ProcessId* out_external_pid,
                                      base::WaitableEvent* event,
                                      const base::CommandLine* cmdline,
                                      const base::LaunchOptions* launch_options,
                                      const SpawnOptions* spawn_options) {
  dbus::ObjectProxy* object_proxy = GetPortalObjectProxy();
  dbus::MethodCall method_call(kFlatpakPortalInterfaceName, "Spawn");
  dbus::MessageWriter writer(&method_call);

  const base::FilePath& current_directory =
      !launch_options->current_directory.empty()
          ? launch_options->current_directory
          // Change to /app since it's guaranteed to always be present in
          // the sandbox.
          : kFlatpakAppPath;
  WriteStringAsByteArray(&writer, current_directory.value());

  dbus::MessageWriter argv_writer(nullptr);
  writer.OpenArray("ay", &argv_writer);

  for (const std::string& arg : cmdline->argv()) {
    WriteStringAsByteArray(&argv_writer, arg);
  }

#ifndef NDEBUG
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableFullFlatpakSandbox)) {
    std::string arg = "--";
    arg += kDisableFullFlatpakSandbox;
    WriteStringAsByteArray(&argv_writer, arg);
  }
#endif

  writer.CloseContainer(&argv_writer);

  dbus::MessageWriter fds_writer(nullptr);
  writer.OpenArray("{uh}", &fds_writer);

  WriteFdPairMap(&fds_writer, STDIN_FILENO, STDIN_FILENO);
  WriteFdPairMap(&fds_writer, STDOUT_FILENO, STDOUT_FILENO);
  WriteFdPairMap(&fds_writer, STDERR_FILENO, STDERR_FILENO);

  for (const auto& pair : launch_options->fds_to_remap) {
    WriteFdPairMap(&fds_writer, pair.first, pair.second);
  }

  writer.CloseContainer(&fds_writer);

  dbus::MessageWriter env_writer(nullptr);
  writer.OpenArray("{ss}", &env_writer);

  for (const auto& pair : launch_options->environment) {
    dbus::MessageWriter entry_writer(nullptr);
    env_writer.OpenDictEntry(&entry_writer);

    entry_writer.AppendString(pair.first);
    entry_writer.AppendString(pair.second);

    env_writer.CloseContainer(&entry_writer);
  }

  writer.CloseContainer(&env_writer);

  int spawn_flags = kFlatpakSpawn_Sandbox | kFlatpakSpawn_ExposePids |
                    kFlatpakSpawn_NotifyStart;
  int sandbox_flags = 0;

#ifndef NDEBUG
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableFullFlatpakSandbox)) {
    spawn_flags &= ~kFlatpakSpawn_Sandbox;
  }
#else
#endif

  if (launch_options->clear_environment) {
    spawn_flags |= kFlatpakSpawn_ClearEnvironment;
  }

  if (launch_options->kill_on_parent_death) {
    spawn_flags |= kFlatpakSpawn_WatchBus;
  }

  writer.AppendUint32(spawn_flags);

  dbus::MessageWriter options_writer(nullptr);
  writer.OpenArray("{sv}", &options_writer);

  if (!spawn_options->sandbox_expose_ro.empty()) {
    dbus::MessageWriter entry_writer(nullptr);
    options_writer.OpenDictEntry(&entry_writer);

    entry_writer.AppendString("sandbox-expose-fd-ro");

    dbus::MessageWriter variant_writer(nullptr);
    entry_writer.OpenVariant("ah", &variant_writer);

    dbus::MessageWriter ro_fds_writer(nullptr);
    variant_writer.OpenArray("h", &ro_fds_writer);

    for (const base::ScopedFD& fd : spawn_options->sandbox_expose_ro) {
      CHECK(fd.is_valid()) << "Invalid spawn expose fd";
      ro_fds_writer.AppendFileDescriptor(fd.get());
    }

    variant_writer.CloseContainer(&ro_fds_writer);
    entry_writer.CloseContainer(&variant_writer);
    options_writer.CloseContainer(&entry_writer);
  }

  if (sandbox_flags != 0) {
    dbus::MessageWriter entry_writer(nullptr);
    options_writer.OpenDictEntry(&entry_writer);

    entry_writer.AppendString("sandbox-flags");

    dbus::MessageWriter variant_writer(nullptr);
    entry_writer.OpenVariant("u", &variant_writer);

    variant_writer.AppendUint32(sandbox_flags);

    entry_writer.CloseContainer(&variant_writer);
    options_writer.CloseContainer(&entry_writer);
  }

  writer.CloseContainer(&options_writer);

  object_proxy->CallMethodWithErrorResponse(
      &method_call, dbus::ObjectProxy::TIMEOUT_INFINITE,
      base::BindOnce(&FlatpakSandbox::OnSpawnResponse, base::Unretained(this),
                     base::Unretained(out_external_pid),
                     base::Unretained(event)));
}

void FlatpakSandbox::OnSpawnResponse(base::ProcessId* out_external_pid,
                                     base::WaitableEvent* event,
                                     dbus::Response* response,
                                     dbus::ErrorResponse* error_response) {
  if (response) {
    dbus::MessageReader reader(response);
    uint32_t external_pid;
    if (!reader.PopUint32(&external_pid)) {
      LOG(ERROR) << "Invalid Spawn() response";
    } else {
      VLOG(1) << "Spawn() returned PID " << external_pid;
      if (out_external_pid != nullptr) {
        *out_external_pid = external_pid;
      }

      base::AutoLock locker(process_info_lock_);
      unmapped_processes_.insert(external_pid);
    }
  } else if (error_response) {
    std::string error_name = error_response->GetErrorName();
    std::string error_message;
    dbus::MessageReader reader(error_response);
    reader.PopString(&error_message);

    LOG(ERROR) << "Error calling Spawn(): " << error_name << ": "
               << error_message;
  } else {
    LOG(ERROR) << "Unknown error occurred calling Spawn()";
  }

  if (event != nullptr) {
    event->Signal();
  }
}

base::ProcessId FlatpakSandbox::GetRelativePid(base::ProcessId external_pid) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedAllowBaseSyncPrimitives allow_wait;

  base::AutoLock locker(process_info_lock_);

  for (;;) {
    auto unmapped_it = unmapped_processes_.find(external_pid);
    if (unmapped_it != unmapped_processes_.end()) {
      // No relative PID is known yet.
      VLOG(1) << "Waiting for " << external_pid;
      process_info_cv_.Wait();
      continue;
    }

    auto relative_pid = running_processes_.FindRelativeByExternal(external_pid);
    if (!relative_pid) {
      exited_process_statuses_.erase(external_pid);

      LOG(INFO) << "Already died: " << external_pid;
      return base::kNullProcessId;
    }

    VLOG(1) << "Got " << external_pid << " => " << *relative_pid;
    return *relative_pid;
  }
}

}  // namespace sandbox
}  // namespace vivaldi
