// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved.

#include "content/browser/zygote_host/vivaldi_zygote_host_linux_utils.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"  // nogncheck
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"
#include "sandbox/linux/services/flatpak_sandbox.h"
#include "third_party/widevine/cdm/buildflags.h"  // nogncheck

namespace vivaldi {
namespace {

constexpr const char* kLdPreloadEnvVar = "LD_PRELOAD";
constexpr const char* kPreloadDelimiters = ": ";

void HandleLdPreloadEnvVar(
    base::LaunchOptions* options,
    sandbox::FlatpakSandbox::SpawnOptions* spawn_options) {
  auto env = base::Environment::Create();
  std::string ld_preload;

  if (!env->GetVar(kLdPreloadEnvVar, &ld_preload))
    return;

  VLOG(3) << "Propagating LD_PRELOAD into flatpak sandbox: " << ld_preload;

  // Propagate LD_PRELOAD.
  options->environment[kLdPreloadEnvVar] = ld_preload;

  // We also have to expose the path(s) in the var to the sandbox.
  for (const auto& piece : base::SplitStringPiece(
           ld_preload, kPreloadDelimiters, base::KEEP_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    spawn_options->ExposePathRo(base::FilePath{piece});
  }
}

} // namespace

base::Process LaunchFlatpakZygote(const base::CommandLine &cmd_line,
                                  base::LaunchOptions* options) {
  // Based on code by refi64 (chromium flatpak patches).
  // Original code to be found here:
  // https://github.com/flathub/org.chromium.Chromium/blob/master/patches/chromium/
  sandbox::FlatpakSandbox::SpawnOptions spawn_options;

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // Expose the CDM paths into the sandbox. This is similar to PreSandboxInit
  // in content_main_runner_impl.cc.
  std::vector<content::CdmInfo> cdms;
  content::GetContentClient()->AddContentDecryptionModules(&cdms, nullptr);
  for (const auto& cdm : cdms) {
    if (!spawn_options.ExposePathRo(cdm.path)) {
      LOG(ERROR) << "Failed to expose CDM module";
    }
  }
#endif

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
  // Make sure we also expose the full Widevine CDM folder so it can be
  // detected.
  // TODO: Remove the explicit dependencies on chrome::.
  base::FilePath widevine_cdm_path;
  if (!base::PathService::Get(chrome::DIR_COMPONENT_UPDATED_WIDEVINE_CDM,
                              &widevine_cdm_path)) {
    LOG(ERROR) << "Failed to get Widevine CDM folder for sandbox forwarding";
  }

  LOG(INFO) << "Widevine CDM path IS: " << widevine_cdm_path;

  if (!widevine_cdm_path.empty() && base::PathExists(widevine_cdm_path)) {
    if (!spawn_options.ExposePathRo(widevine_cdm_path)) {
      LOG(ERROR) << "Failed to expose updated Widevine CDM path";
    }
  }

  // The Widevine data is found relative to $XDG_CONFIG_HOME, which is not set
  // by default when running a sandboxed process.
  auto env = base::Environment::Create();
  base::FilePath xdgConfigHome = base::nix::GetXDGDirectory(
      env.get(), base::nix::kXdgConfigHomeEnvVar, nullptr);
  if (!xdgConfigHome.empty()) {
    options->environment[base::nix::kXdgConfigHomeEnvVar] =
        xdgConfigHome.value();
  }
#endif

  // If we encounter LD_PRELOAD, we propagate and expose paths to sandbox.
  HandleLdPreloadEnvVar(options, &spawn_options);

  return sandbox::FlatpakSandbox::GetInstance()->LaunchProcess(
      cmd_line, *options, spawn_options);
}

}  // namespace vivaldi
