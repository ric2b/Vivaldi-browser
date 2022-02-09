/*
 *  This file is part of WinSparkle (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2016 Vaclav Slavik
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

#ifndef UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_UI_H_
#define UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_UI_H_

#include "base/check.h"

#include <string>

namespace vivaldi_update_notifier {

struct Appcast;
struct DownloadReport;

class Error;

class UIDelegate {
 public:
  virtual ~UIDelegate() = default;

  virtual void WinsparkleStartDownload() = 0;
  virtual void WinsparkleLaunchInstaller() = 0;
  virtual void WinsparkleOnUIClose() = 0;
};

class UI {
 public:
  static void Init(UIDelegate& delegate);

  static void BringToFocus();

  static void NotifyCheckingUpdates();

  static void NotifyUpdateCheckDone(const Appcast* appcast,
                                    const Error& error,
                                    bool pending_update);

  static void NotifyDownloadProgress(const DownloadReport& report);

  static void NotifyDownloadResult(const Error& download_error);

  static void NotifyStartedInstaller(const Error& error);
};

}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_UI_H_
