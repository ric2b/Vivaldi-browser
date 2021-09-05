// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include <Windows.h>
#include <shellscalingapi.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "base/win/windows_version.h"
#include "chrome/install_static/product_install_details.h"

#include "update_notifier/update_notifier_manager.h"


// NOTE(jarle@vivaldi.com): High DPI enabling functions source code is borrowed
// from chrome_exe_main_win.cc.

bool SetProcessDpiAwarenessWrapper(PROCESS_DPI_AWARENESS value) {
  typedef HRESULT(WINAPI *SetProcessDpiAwarenessPtr)(PROCESS_DPI_AWARENESS);
  SetProcessDpiAwarenessPtr set_process_dpi_awareness_func =
      reinterpret_cast<SetProcessDpiAwarenessPtr>(
          GetProcAddress(GetModuleHandleA("user32.dll"),
                         "SetProcessDpiAwarenessInternal"));
  if (set_process_dpi_awareness_func) {
    HRESULT hr = set_process_dpi_awareness_func(value);
    if (SUCCEEDED(hr)) {
      VLOG(1) << "SetProcessDpiAwareness succeeded.";
      return true;
    } else if (hr == E_ACCESSDENIED) {
      LOG(ERROR) << "Access denied error from SetProcessDpiAwareness. "
          "Function called twice, or manifest was used.";
    }
  }
  return false;
}

void EnableHighDPISupport() {
  // Enable per-monitor DPI for Win10 or above instead of Win8.1 since Win8.1
  // does not have EnableChildWindowDpiMessage, necessary for correct non-client
  // area scaling across monitors.
  bool allowed_platform = base::win::GetVersion() >= base::win::Version::WIN10;
  PROCESS_DPI_AWARENESS process_dpi_awareness =
      allowed_platform ? PROCESS_PER_MONITOR_DPI_AWARE
                       : PROCESS_SYSTEM_DPI_AWARE;
  if (!SetProcessDpiAwarenessWrapper(process_dpi_awareness)) {
    SetProcessDPIAware();
  }
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(0, nullptr);
  base::SingleThreadTaskExecutor executor(base::MessagePumpType::UI);

  install_static::InitializeProductDetailsForPrimaryModule();
  EnableHighDPISupport();

  vivaldi_update_notifier::UpdateNotifierManager manager;
  bool ok = manager.RunNotifier();

  // Directly call ExitProcess() to skip any C++ global destructors in wxWidgets
  // and terminate all threads immediately. This also ensures that the manager
  // instance is alive and the process terminats.
  ExitProcess(ok ? 0 : 1);
}
