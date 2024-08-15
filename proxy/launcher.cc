//
// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.
//
#include "proxy/launcher.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#endif

#define CERTIFICATENAME "relayproxy.pem"

#if BUILDFLAG(IS_MAC)
  #define PLATFORMSPECIFIC_PROXYNAME "relayproxy-darwin"
#endif
#if BUILDFLAG(IS_LINUX)
  #define PLATFORMSPECIFIC_PROXYNAME "relayproxy-linux"
#endif
#if BUILDFLAG(IS_WIN)
  #define PLATFORMSPECIFIC_PROXYNAME L"relayproxy.exe"
#endif

namespace {

class ProcessWrapper {
public:
  ProcessWrapper() {}
  ~ProcessWrapper() {
    if (process_.IsValid()) {
      process_.Terminate(0, false);
    }
  }
  base::Process process_;
};

static ProcessWrapper process_wrapper;

}  // namespace

namespace vivaldi {
namespace proxy {

ConnectSettings::ConnectSettings() {}
ConnectSettings::~ConnectSettings() {}

bool connect(const ConnectSettings& settings, ConnectState& state) {

  disconnect();

#if BUILDFLAG(IS_LINUX)
  // On Linux, the launch script sets this environment variable. $HERE has to be
  // set for development as well and point to the directory where you have the
  // proxy binary.
  const char* here = getenv("HERE");
  if (!here) {
    state.message = "Can not locate proxy application (environment variable $HERE not set).";
    return false;
  }
  std::string exe_filename_with_path = std::string(here) + "/"
      + PLATFORMSPECIFIC_PROXYNAME;
  base::FilePath exe_file_path = base::FilePath(exe_filename_with_path);
  if (!base::PathExists(exe_file_path)) {
    state.message = "No such file " + exe_filename_with_path;
    return false;
  }
  int mode;
  if (
    !base::GetPosixFilePermissions(exe_file_path, &mode) ||
    !(mode & base::FILE_PERMISSION_EXECUTE_BY_USER)
  ) {
    state.message = "Not executable " + exe_filename_with_path;
    return false;
  }

  std::string cert_filename_with_path = std::string(here) + "/"
      + CERTIFICATENAME;
  base::FilePath cert_file_path = base::FilePath(cert_filename_with_path);
  if (!base::PathExists(cert_file_path)) {
    state.message = "No such file " + cert_filename_with_path;
    return false;
  }
  mode = 0;
  if (
    !base::GetPosixFilePermissions(cert_file_path, &mode) ||
    !(mode & base::FILE_PERMISSION_READ_BY_USER)
  ) {
    state.message = "Not readable " + cert_filename_with_path;
    return false;
  }

  base::CommandLine launch_command(exe_file_path);
#elif BUILDFLAG(IS_MAC)
  base::FilePath framework_bundle_path = base::apple::FrameworkBundlePath();
  base::FilePath file_path =
      framework_bundle_path.Append("Helpers").Append(
          PLATFORMSPECIFIC_PROXYNAME);
  base::FilePath cert_file_path =
      framework_bundle_path.Append("Resources").Append(
          CERTIFICATENAME);
  base::CommandLine launch_command(file_path);
#else // IS_WIN
  base::CommandLine launch_command(base::FilePath(PLATFORMSPECIFIC_PROXYNAME));
#endif

#if BUILDFLAG(IS_WIN)
  launch_command.AppendArg("-invisvRelay " + settings.remote_host);
  launch_command.AppendArg("-invisvRelayPort " + settings.remote_port);
  launch_command.AppendArg("-listenPort " + settings.local_port);
  launch_command.AppendArg("-certDataFile " + std::string(CERTIFICATENAME));
  launch_command.AppendArg("-token " + settings.token);
#else
  launch_command.AppendSwitchASCII("-invisvRelay", settings.remote_host);
  launch_command.AppendSwitchASCII("-invisvRelayPort", settings.remote_port);
  launch_command.AppendSwitchASCII("-listenPort", settings.local_port);
  launch_command.AppendSwitchASCII("-certDataFile", cert_file_path.value());
  launch_command.AppendSwitchASCII("-token", settings.token);
#endif
  process_wrapper.process_ =
    base::LaunchProcess(launch_command, base::LaunchOptions());
  state.pid = process_wrapper.process_.Pid();

  return state.pid != 0;
}

void disconnect() {
  if (process_wrapper.process_.IsValid()) {
    process_wrapper.process_.Terminate(0, false);
  }
}

}  // proxy
}  // vivaldi
