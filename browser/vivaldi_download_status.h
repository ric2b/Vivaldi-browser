// Copyright (c) 2016 Vivaldi. All rights reserved.

#if defined(OS_WIN)

namespace extensions {
class AppWindow;
}

namespace vivaldi {

extern  extensions::AppWindow* current_vivaldi_window_;

void UpdateTaskbarProgressBarForVivaldiWindows(int download_count,
                                               bool progress_known,
                                               float progress);

} // namespace vivaldi

#endif // OS_WIN
