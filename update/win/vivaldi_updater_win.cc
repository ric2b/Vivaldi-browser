// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vivaldi_updater_win.h"

#include "app/vivaldi_version_info.h"
#include "base/command_line.h"
#include "base/version.h"
#include "base/vivaldi_switches.h"
#include "browser/init_sparkle.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "extensions/api/auto_update/auto_update_api.h"
#include "extensions/api/auto_update/auto_update_status.h"
#include "installer/win/detached_thread.h"
#include "installer/win/vivaldi_install_l10n.h"
#include "ui/base/l10n/l10n_util.h"

#include "update_notifier/thirdparty/winsparkle/src/appcast.h"
#include "update_notifier/thirdparty/winsparkle/src/config.h"

using vivaldi_update_notifier::Appcast;
using vivaldi_update_notifier::DownloadReport;
using vivaldi_update_notifier::Error;
using vivaldi_update_notifier::FileDownloader;
using vivaldi_update_notifier::g_install_dir;
using vivaldi_update_notifier::InstallerLaunchData;

namespace update {

using ResultCallback =
    base::OnceCallback<void(std::unique_ptr<vivaldi_update_notifier::Appcast>,
                            vivaldi_update_notifier::Error)>;

using DownloadCallback =
    base::OnceCallback<void(std::unique_ptr<InstallerLaunchData> launch_data,
                            Error error)>;

using OnDownloadReportCallback =
    base::RepeatingCallback<void(DownloadReport report)>;

constexpr base::win::i18n::LanguageSelector::LangToOffset
    kLanguageOffsetPairs[] = {
#define HANDLE_LANGUAGE(l_, o_) {L## #l_, o_},
        DO_LANGUAGES
#undef HANDLE_LANGUAGE
};

UpdaterCheckVivaldi::~UpdaterCheckVivaldi() {}

UpdaterCheckVivaldi::UpdaterCheckVivaldi(
    const base::WeakPtr<UpdateBackendNotifier>& delegate)
    : result_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      update_backend_(delegate),
      weak_ptr_factory_(this) {
  vivaldi::InitInstallerLanguage(kLanguageOffsetPairs);
}

class UpdaterCheckVivaldi::UpdateCheckThreads : public vivaldi::DetachedThread {
 public:
  UpdateCheckThreads() = default;
  UpdateCheckThreads(scoped_refptr<base::SequencedTaskRunner> result_runner,
                     ResultCallback callback)
      : result_runner_(std::move(result_runner)),
        result_callback_(std::move(callback)) {}

  void Run() override {
    Error error;
    std::unique_ptr<Appcast> appcast = CheckForUpdates(error);
    result_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback_),
                                  std::move(appcast), std::move(error)));
  }

  std::unique_ptr<Appcast> CheckForUpdates(Error& error) {
    if (error)
      return nullptr;

    GURL url = init_sparkle::GetAppcastUrl();
    downloader_.DisableCaching();
    downloader_.Connect(url, error);
    std::string appcast_xml = downloader_.FetchAll(error);
    if (error)
      return nullptr;
    if (appcast_xml.size() == 0) {
      error.set(Error::kFormat, "Appcast XML data incomplete.");
      return nullptr;
    }

    std::unique_ptr<Appcast> appcast = Appcast::Load(appcast_xml, error);
    if (!appcast)
      return nullptr;
    DCHECK(appcast->IsValid());
    if (!appcast->IsValid())
      return nullptr;

    return appcast;
  }

  FileDownloader downloader_;
  const scoped_refptr<base::SequencedTaskRunner> result_runner_;

  ResultCallback result_callback_;
};

class UpdaterCheckVivaldi::UpdateDownloadThread
    : public vivaldi::DetachedThread,
      public vivaldi_update_notifier::DownloadUpdateDelegate {
 public:
  UpdateDownloadThread(scoped_refptr<base::SequencedTaskRunner> result_runner,
                       DownloadCallback download_callback,
                       OnDownloadReportCallback report_callback,
                       const Appcast& appcast)
      : result_runner_(std::move(result_runner)),
        download_callback_(std::move(download_callback)),
        report_callback_(std::move(report_callback)),
        appcast_(std::move(appcast)) {}

 protected:
  void Run() override {
    Error error;
    std::unique_ptr<InstallerLaunchData> launch_data =
        DownloadUpdate(appcast_, *this, error);
    if (launch_data) {
      launch_data->cmdline.AppendSwitch(switches::kVivaldiSilentUpdate);

      result_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(download_callback_),
                                    std::move(launch_data), std::move(error)));
    }
  }

  // DownloadUpdateDelegate implementation.
  void SendReport(const DownloadReport& report, Error& error) override {
    if (error)
      return;

    if (report.kind == DownloadReport::kMoreData) {
      // Only update at most 10 times/sec so that we don't flood the UI.
      clock_t now = clock();
      if (report.downloaded_length != report.content_length &&
          double(now - last_more_data_time_) / CLOCKS_PER_SEC < 0.1) {
        return;
      }
      last_more_data_time_ = now;
    } else {
      last_more_data_time_ = 0;
    }

    result_runner_->PostTask(
        FROM_HERE, base::BindOnce(report_callback_, std::move(report)));
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> result_runner_;
  DownloadCallback download_callback_;
  OnDownloadReportCallback report_callback_;
  Appcast appcast_;
  clock_t last_more_data_time_ = 0;
};

void UpdaterCheckVivaldi::OnUpdateCheckResult(std::unique_ptr<Appcast> appcast,
                                              Error error) {
  base::Version app_version(::vivaldi::GetVivaldiVersion());
  base::FilePath current_exe_path = vivaldi::GetPathOfCurrentExe();
  vivaldi_update_notifier::g_app_version =
      vivaldi::GetInstallVersion(vivaldi_update_notifier::GetExeDir());

  if (g_install_dir.empty()) {
    base::FilePath update_install_dir = current_exe_path.DirName().DirName();
    g_install_dir = std::move(update_install_dir);
  }

  if (!appcast && error) {
    update_backend_->NotifyUpdateProgress(AutoUpdateStatus::kError, "", 0);
    return;
  }

  if (appcast && appcast->Version > app_version) {
    // Update is needed.
    if (install_update_if_possible_) {
      auto download_thread = std::make_unique<UpdateDownloadThread>(
          result_runner_,
          base::BindOnce(&UpdaterCheckVivaldi::OnUpdateDownloadResult,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindRepeating(&UpdaterCheckVivaldi::OnUpdateDownloadReport,
                              weak_ptr_factory_.GetWeakPtr()),
          *appcast);

      vivaldi::DetachedThread::Start(std::move(download_thread));
    } else {
      // Update needed. Requires manual download.
      result_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&UpdaterCheckVivaldi::NotifyUpdateCheckComplete,
                         base::Unretained(this), appcast->Version.GetString()));
    }

  } else {
    // No update is needed.
    // Reporting empty version means Vivaldi is up to date.
    result_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&UpdaterCheckVivaldi::NotifyUpdateCheckComplete,
                       base::Unretained(this), ""));
  }
  appcast.reset();
}

void UpdaterCheckVivaldi::BeginUpdateCheck(bool should_install_update) {
  install_update_if_possible_ = should_install_update;

  auto update_check = std::make_unique<UpdateCheckThreads>(
      result_runner_, base::BindOnce(&UpdaterCheckVivaldi::OnUpdateCheckResult,
                                     weak_ptr_factory_.GetWeakPtr()));

  vivaldi::DetachedThread::Start(std::move(update_check));
}

void UpdaterCheckVivaldi::OnUpdateDownloadReport(DownloadReport report) {
  int percentage = 0;
  if (report.downloaded_length != 0) {
    percentage =
        static_cast<int>((static_cast<double>(report.downloaded_length) /
                          report.content_length) *
                         100);
  }

  if (update_backend_) {
    update_backend_->NotifyUpdateProgress(AutoUpdateStatus::kWillDownloadUpdate,
                                          "", percentage);
  }
}

void UpdaterCheckVivaldi::NotifyUpdateCheckComplete(
    const std::string& appcast_version) {
  DCHECK(result_runner_->RunsTasksInCurrentSequence());

  if (appcast_version.empty()) {
    if (update_backend_) {
      // Empty version means updated.
      BackendProgress(AutoUpdateStatus::kNoUpdate, "", 0);
    }

  } else {
    if (update_backend_) {
      BackendProgress(install_update_if_possible_
                          ? AutoUpdateStatus::kDidDownloadUpdate
                          : AutoUpdateStatus::kDidFindValidUpdate,
                      appcast_version, 0);
    }
  }
}

void UpdaterCheckVivaldi::BackendProgress(const AutoUpdateStatus& status,
                                          const std::string& appcast_version,
                                          int progress) {
  update_backend_->NotifyUpdateProgress(status, appcast_version, progress);
}

void UpdaterCheckVivaldi::OnUpdateDownloadResult(
    std::unique_ptr<InstallerLaunchData> launch_data,
    Error error) {
  auto version = launch_data->version.GetString();

  result_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UpdaterCheckVivaldi::NotifyUpdateCheckComplete,
                                base::Unretained(this), std::move(version)));
  base::Process process = RunInstaller(std::move(launch_data), error);
}

}  // namespace update
