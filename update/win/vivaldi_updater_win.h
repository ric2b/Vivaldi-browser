// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_WIN_VIVALDI_UPDATER_WIN_H_
#define UPDATE_WIN_VIVALDI_UPDATER_WIN_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

#include "extensions/api/auto_update/auto_update_status.h"
#include "update/update_backend_notifier.h"

#include "update_notifier/thirdparty/winsparkle/src/download.h"
#include "update_notifier/thirdparty/winsparkle/src/error.h"
#include "update_notifier/thirdparty/winsparkle/src/updatedownloader.h"

namespace update {

using vivaldi_update_notifier::Appcast;
using vivaldi_update_notifier::DownloadReport;
using vivaldi_update_notifier::Error;
using vivaldi_update_notifier::FileDownloader;
using vivaldi_update_notifier::InstallerLaunchData;

class UpdaterCheckVivaldi {
 public:
  UpdaterCheckVivaldi(const base::WeakPtr<UpdateBackendNotifier>& delegate);

  UpdaterCheckVivaldi(const UpdaterCheckVivaldi&) = delete;
  UpdaterCheckVivaldi& operator=(const UpdaterCheckVivaldi&) = delete;
  ~UpdaterCheckVivaldi();

  // Starts an update check.
  void BeginUpdateCheck(bool should_install_update);

 private:
  class UpdateCheckThreads;
  class UpdateDownloadThread;
  friend class UpdateCheckThreads;
  friend class UpdateDownloadThread;

  void OnUpdateCheckResult(std::unique_ptr<Appcast> appcast, Error error);
  void OnUpdateDownloadResult(std::unique_ptr<InstallerLaunchData> launch_data,
                              Error error);

  void NotifyUpdateCheckComplete(const std::string& new_version);

  void BackendProgress(const AutoUpdateStatus& status,
                       const std::string& appcast_version,
                       int progress);

  void OnUpdateDownloadReport(DownloadReport report);

  // The caller's task runner, on which methods of the |update_backend_| will be
  // invoked.
  scoped_refptr<base::SequencedTaskRunner> result_runner_;

  bool install_update_if_possible_;
  base::WeakPtr<UpdateBackendNotifier> update_backend_;

  base::WeakPtrFactory<UpdaterCheckVivaldi> weak_ptr_factory_{this};
};

}  // namespace update

#endif  // UPDATE_WIN_VIVALDI_UPDATER_WIN_H_
