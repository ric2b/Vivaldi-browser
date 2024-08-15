// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_UPDATE_NOTIFIER_MANAGER_H_
#define UPDATE_NOTIFIER_UPDATE_NOTIFIER_MANAGER_H_

#include <atomic>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/win/scoped_handle.h"

#include "update_notifier/thirdparty/winsparkle/src/appcast.h"
#include "update_notifier/thirdparty/winsparkle/src/error.h"
#include "update_notifier/thirdparty/winsparkle/src/ui.h"
#include "update_notifier/thirdparty/winsparkle/src/updatedownloader.h"
#include "update_notifier/update_notifier_switches.h"

namespace base {
class MessageLoop;
}

namespace vivaldi_update_notifier {

class UpdateNotifierWindow;

class UpdateNotifierManager : public UIDelegate {
 public:
  UpdateNotifierManager();
  ~UpdateNotifierManager() override;
  UpdateNotifierManager(const UpdateNotifierManager&) = delete;
  UpdateNotifierManager& operator=(const UpdateNotifierManager&) = delete;

  static UpdateNotifierManager& GetInstance();

  ExitCode RunNotifier();

  void StartUpdateCheck();
  static void OnNotificationAcceptance();

 private:
  class UpdateCheckThread;
  class UpdateDownloadThread;
  friend class UpdateCheckThread;
  friend class UpdateDownloadThread;

  // See comments for download_job_id_.
  using JobId = unsigned;

  void InitEvents(bool& already_runs);
  void OnCheckForUpdatesEvent(base::WaitableEvent* waitable_event);
  void OnQuitEvent(base::WaitableEvent* waitable_event);

  void OnUpdateCheckResult(std::unique_ptr<Appcast> appcast, Error error);
  void StartDownload();
  void OnUpdateDownloadReport(JobId job_id, DownloadReport report);
  void OnUpdateDownloadResult(JobId job_id,
                              std::unique_ptr<InstallerLaunchData> launch_data,
                              Error error);
  void LaunchInstaller();
  void FinishCheck();

  void ShowUpdateNotification(const base::Version& version);

  // UIDelegate implementation.

  void WinsparkleStartDownload() override;
  void WinsparkleLaunchInstaller() override;
  void WinsparkleOnUIClose() override;

  base::FilePath exe_dir_;

  std::unique_ptr<UpdateNotifierWindow> update_notifier_window_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;
  base::RunLoop run_loop_;

  std::optional<base::WaitableEvent> check_for_updates_event_;
  base::WaitableEventWatcher check_for_updates_event_watch_;

  std::optional<base::WaitableEvent> quit_event_;
  base::WaitableEventWatcher quit_event_watch_;

  std::optional<base::WaitableEvent> global_quit_event_;
  base::WaitableEventWatcher global_quit_event_watch_;

  base::Time check_start_time_;
  bool active_winsparkle_ui_ = false;
  bool active_version_check_ = false;
  bool active_download_ = false;
  std::unique_ptr<Appcast> appcast_;
  std::unique_ptr<InstallerLaunchData> launch_data_;

  // When active_download_ is true, this id denotes the current download job to
  // track cancellations. Each time the user cancels an active download via
  // WinSparkle UI the id is incremented. Then the background thread see that
  // its id does not match the current one and cancel the download. Similarly
  // the main thread ignores the results of the download with mismatched id and
  // remove any partially downloaded data.
  //
  // Note that the background job that checks for a new version does not need
  // such id as the check does not store anything on the disc and the result of
  // any check can be used to decide about updates.
  std::atomic<JobId> download_job_id_{0};
};

}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_UPDATE_NOTIFIER_MANAGER_H_
