// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include <Windows.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/process/launch.h"
#include "base/task/single_thread_task_executor.h"
#include "chrome/install_static/product_install_details.h"

#include "update_notifier/update_notifier_manager.h"

namespace {

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  base::CommandLine::Init(0, nullptr);
  base::AtExitManager at_exit;
  base::SingleThreadTaskExecutor executor(base::MessagePumpType::UI);

  install_static::InitializeProductDetailsForPrimaryModule();

  vivaldi_update_notifier::UpdateNotifierManager manager;
  bool ok = manager.RunNotifier();

  // Directly call ExitProcess() to skip any C++ global destructors in wxWidgets
  // and terminate all threads immediately. This also ensures that the manager
  // instance is alive and the process terminats.
  ExitProcess(ok ? 0 : 1);
}
