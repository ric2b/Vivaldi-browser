// Copyright (c) 2016 Vivaldi. All rights reserved.
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser/vivaldi_download_status.h"

#include <shobjidl.h>
#include <wrl/client.h>

#include "base/win/windows_version.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_win.h"
#include "ui/views/win/hwnd_util.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"

namespace vivaldi {
void UpdateTaskbarProgressBarForVivaldiWindows(int download_count,
                                               bool progress_known,
                                               float progress) {
  VivaldiBrowserWindow* current_vivaldi_window =
      vivaldi::ui_tools::GetActiveAppWindow();

  if (!current_vivaldi_window)
    return;  // Only for vivaldi

  // Taskbar progress bar is only supported on Win7.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  Microsoft::WRL::ComPtr<ITaskbarList3> taskbar;
  HRESULT result =
      CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER,
                       IID_PPV_ARGS(&taskbar));
  if (FAILED(result)) {
    return;
  }

  result = taskbar->HrInit();
  if (FAILED(result)) {
    return;
  }

  HWND frame =
      views::HWNDForNativeWindow(current_vivaldi_window->GetNativeWindow());
  if (download_count == 0 || progress == 1.0f)
    taskbar->SetProgressState(frame, TBPF_NOPROGRESS);
  else if (!progress_known)
    taskbar->SetProgressState(frame, TBPF_INDETERMINATE);
  else
    taskbar->SetProgressValue(frame, static_cast<int>(progress * 100), 100);
}

}  // namespace vivaldi
