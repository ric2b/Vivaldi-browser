/*
 *  This file is part of WinSparkle (https://winsparkle.org)
 *
 *  Copyright (C) 2012-2016 Vaclav Slavik
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */
#ifndef UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_UPDATEDOWNLOADER_H_
#define UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_UPDATEDOWNLOADER_H_

#include "update_notifier/thirdparty/winsparkle/src/error.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/process.h"
#include "base/version.h"

#include <inttypes.h>
#include <ctime>
#include <memory>

namespace vivaldi_update_notifier {

struct Appcast;

// Reports to notify about various statges of the update process.
struct DownloadReport {
  enum Kind {
    // Fetched connection headers, will try to get data
    kConnected,

    // Received more data
    kMoreData,

    // About to start the cignature verification
    kVerificationStart,

    // Unpacking the downloaded data
    kUnpacking,
  };

  Kind kind = kConnected;

  // If fetching or processing a delta download
  bool delta = false;

  // Expected length of received data as reported by the Content-Length header
  int content_length = 0;

  // Number of downloaded so far bytes.
  int downloaded_length = 0;
};

// Interface to delegate the sending of download reports.
class DownloadUpdateDelegate {
 public:
  virtual ~DownloadUpdateDelegate() = default;

  virtual void SendReport(const DownloadReport& report, Error& error) = 0;
};

// Structure with data to start the installer. In its destructor it deletes the
// downloaded data unless an installer successfully started.
struct InstallerLaunchData {
  InstallerLaunchData(bool delta,
                      const base::Version& version,
                      base::CommandLine command_line);
  ~InstallerLaunchData();
  InstallerLaunchData(const InstallerLaunchData&) = delete;
  InstallerLaunchData& operator=(const InstallerLaunchData&) = delete;

  bool delta = false;
  base::Version version;
  base::FilePath download_dir;
  base::CommandLine cmdline;
};

// Download the update.
std::unique_ptr<InstallerLaunchData> DownloadUpdate(
    const Appcast& appcast,
    DownloadUpdateDelegate& delegate,
    Error& error);

base::Process RunInstaller(std::unique_ptr<InstallerLaunchData> launch_data,
                           Error& error);

// Perform any necessary cleanup after previous updates.
//
// Should be called on launch to get rid of leftover junk from previous
// updates, such as the installer files. Call it before the first call to
// DownloadUpdate().
void CleanDownloadLeftovers();

// Get unique installation hash. Return an empty string on errors.
std::wstring GetInstallHash(bool for_task_scheduler);

}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_UPDATEDOWNLOADER_H_
