// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include <string>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

#include "extensions/api/auto_update/auto_update_api.h"
#include "extensions/tools/vivaldi_tools.h"

#include "base/task/thread_pool.h"

namespace extensions {

static const std::string kFFMPEGFuturePath = "VIVALDI_FFMPEG_FUTURE_PATH";

bool getFFMPEGFuturePath(std::string &target) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return env->GetVar(kFFMPEGFuturePath, &target) && !target.empty();
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
  task_runner_->PostTask(FROM_HERE, base::BindOnce(
                                        [](AutoUpdateAPI* api) {
                                          api->executable_file_watcher_.reset();
                                          api->ffmpeg_file_watcher_.reset();
                                        },
                                        this));
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
  return RespondNow(Error("Not implemented"));
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

}  // namespace extensions
