// Copyright (c) 2016 Vivaldi. All rights reserved.

#ifndef BROWSER_VIVALDI_DOWNLOAD_STATUS_H_
#define BROWSER_VIVALDI_DOWNLOAD_STATUS_H_

#if defined(OS_WIN)

namespace extensions {
class AppWindow;
}

namespace vivaldi {

void UpdateTaskbarProgressBarForVivaldiWindows(int download_count,
                                               bool progress_known,
                                               float progress);

}  // namespace vivaldi

#endif  // OS_WIN

#endif  // BROWSER_VIVALDI_DOWNLOAD_STATUS_H_
