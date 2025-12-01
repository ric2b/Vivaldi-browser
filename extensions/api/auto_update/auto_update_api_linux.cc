// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include <string>

#include "app/vivaldi_version_info.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "components/version_info/version_info.h"
#include "extensions/api/auto_update/auto_update_api.h"
#include "extensions/tools/vivaldi_tools.h"

#include "extensions/api/auto_update/linux_update_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"

namespace extensions {

static constexpr char kFFMPEGFuturePath[] = "VIVALDI_FFMPEG_FUTURE_PATH";
static constexpr char kVivaldiDistroName[] = "VIVALDI_DISTRO_NAME";
static constexpr char kVivaldiDistroVersion[] = "VIVALDI_DISTRO_VERSION_NUMBER";

bool getFFMPEGFuturePath(std::string &target) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  target = env->GetVar(kFFMPEGFuturePath).value_or(std::string());
  return !target.empty();
}

bool getLinuxDistroVersion(std::string &distro, std::string &version) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());

  distro = env->GetVar(kVivaldiDistroName).value_or(std::string());
  version = env->GetVar(kVivaldiDistroVersion).value_or(std::string());

  return !distro.empty();
}

bool DetectNeedCodecRestart() {
  std::string ffmpeg_future_path;

  if (getFFMPEGFuturePath(ffmpeg_future_path)) {
    auto path = base::FilePath(ffmpeg_future_path);
    return base::PathExists(path);
  }

  return false;
}

/* static */
void AutoUpdateAPI::HandleCodecRestartPreconditions() {
  if (DetectNeedCodecRestart()) {
    // to be sure we don't attempt to handle the codec update after restart
    // we have to remove the env variable - otherwise restart incentive
    // would repeat itself even though file was already there when restarting.
    // We only do this when the codec was already installed -
    // otherwise restart would erase the detection while still in progress.
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    env->UnSetVar(kFFMPEGFuturePath);
  }
}

void AutoUpdateAPI::InitUpgradeDetection() {
  // task runner used for both file watching operations
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE, base::MayBlock()});

  // watches the vivaldi executable for changes
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::BindOnce(
          [](AutoUpdateAPI* api) {
            base::FilePath executable_path;
            if (base::PathService::Get(base::FILE_EXE, &executable_path)) {
              LOG(INFO) << "got executable";
              api->executable_file_watcher_ = std::make_unique<base::FilePathWatcher>();
              api->executable_file_watcher_->Watch(
                  executable_path, base::FilePathWatcher::Type::kNonRecursive,
                  base::BindRepeating([](const base::FilePath&, bool) {
                    content::GetUIThreadTaskRunner()->PostTask(
                        FROM_HERE, base::BindOnce([]() {
                          LOG(INFO) << "Binary changed";
                          SendWillInstallUpdateOnQuit(base::Version{});
                        }));
                  }));
            } else {
              LOG(INFO) << "executable not found";
            }
          },
          this)));

  // We also place a FFMPEG file watch if env variable VIVALDI_FFMPEG_FUTURE_PATH
  // is present and not empty.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](AutoUpdateAPI* api) {
            std::string ffmpeg_future_path;
            if (getFFMPEGFuturePath(ffmpeg_future_path)) {
              auto ffmpeg_path = base::FilePath(ffmpeg_future_path);
              LOG(INFO) << "FFMPEG file watch enabled for " << ffmpeg_path;
              api->ffmpeg_file_watcher_ = std::make_unique<base::FilePathWatcher>();
              api->ffmpeg_file_watcher_->Watch(
                  ffmpeg_path, base::FilePathWatcher::Type::kNonRecursive,
                  base::BindRepeating([](const base::FilePath&, bool) {
                    content::GetUIThreadTaskRunner()->PostTask(
                        FROM_HERE, base::BindOnce([]() {
                          SendNeedRestartToReloadCodecs();
                        }));
                  }));
            }
          },
          this));
}

void AutoUpdateAPI::ShutdownUpgradeDetection() {
  // VB-121593: Cancel and destroy the file watchers on their task runner.
  // We must wait for this to complete synchronously before Shutdown() returns,
  // otherwise the Profile's task runner may become invalid while the watchers
  // are still trying to use it during destruction.
  base::WaitableEvent done;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](AutoUpdateAPI* api, base::WaitableEvent* done) {
            api->executable_file_watcher_.reset();
            api->ffmpeg_file_watcher_.reset();
            done->Signal();
          },
          this, &done));
  done.Wait();
}

ExtensionFunction::ResponseAction AutoUpdateStartUpdateFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction AutoUpdateCheckForUpdatesFunction::Run() {
  using vivaldi::auto_update::CheckForUpdates::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction
AutoUpdateIsUpdateNotifierEnabledFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction
AutoUpdateEnableUpdateNotifierFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction
AutoUpdateDisableUpdateNotifierFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction
AutoUpdateInstallUpdateAndRestartFunction::Run() {
  ::vivaldi::RestartBrowser();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AutoUpdateGetAutoInstallUpdatesFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction
AutoUpdateSetAutoInstallUpdatesFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction AutoUpdateGetLastCheckTimeFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction AutoUpdateGetUpdateStatusFunction::Run() {
  // Create update checker and start the check
  auto update_checker = std::make_unique<LinuxUpdateChecker>();

  // Get the URL loader factory from the browser context
  auto* profile = Profile::FromBrowserContext(browser_context());

  // Keep the checker alive by capturing it in the callback
  auto* checker_ptr = update_checker.get();

  // Create a callback that handles the result and cleans up
  auto callback = base::BindOnce(
      [](scoped_refptr<AutoUpdateGetUpdateStatusFunction> function,
         std::unique_ptr<LinuxUpdateChecker> checker,
         const UpdateResult& result) {
        // Convert the result to the appropriate format and send it
        std::optional<AutoUpdateStatus> status;

        switch (result.status) {
          case AutoUpdateStatus::kDidFindValidUpdate:
          case AutoUpdateStatus::kNoUpdate:
          case AutoUpdateStatus::kError:
            status = result.status;
            break;
          default:
            status = std::nullopt;
            break;
        }

        function->SendResult(status, result.version, result.release_notes_url);
        // checker is automatically destroyed here
      },
      scoped_refptr<AutoUpdateGetUpdateStatusFunction>(this),
      std::move(update_checker));

  checker_ptr->CheckForUpdates(
      profile->GetDefaultStoragePartition()->GetURLLoaderFactoryForBrowserProcess().get(),
      std::move(callback));

  return RespondLater();
}

bool AutoUpdateHasAutoUpdatesFunction::HasAutoUpdates() {
  return false;
}

ExtensionFunction::ResponseAction AutoUpdateNeedsCodecRestartFunction::Run() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&DetectNeedCodecRestart),
      base::BindOnce(&AutoUpdateNeedsCodecRestartFunction::DeliverResult,
                     this));

  return RespondLater();
}

ExtensionFunction::ResponseAction AutoUpdateRunStartupChecksFunction::Run() {
#if defined(ARCH_CPU_ARMEL)
  // In some specific situations, we want the frontend to
  // be notified that "autoupdate" didn't finish. We are
  // basically hijacking this system to notify frontend the
  // OS is End-Of-Life'd - in this case linux is implied by the source code,
  // and the cpu type is gated by the ifdef.
  extensions::AutoUpdateAPI::SendUpdaterDidNotFindUpdate(
                           "SystemIsTooOld");
#endif
  return RespondNow(NoArguments());
}

std::string AutoUpdateGetAboutPathsInfoFunction::GetPlatformOSVersion() {
  std::string version(version_info::GetOSType());

  // Also try gather info from env if possible, and append.
  std::string distro_name, distro_version;
  if (getLinuxDistroVersion(distro_name, distro_version)) {
    version += " - " + distro_name;
    if (!distro_version.empty()) {
      version += + " " + distro_version;
    }
  }

  return version;
}

}  // namespace extensions
