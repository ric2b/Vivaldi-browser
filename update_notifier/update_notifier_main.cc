// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include <Windows.h>

#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "update_notifier/update_notifier_manager.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  base::AtExitManager at_exit;
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);

  return vivaldi_update_notifier::UpdateNotifierManager::GetInstance()
                 ->RunNotifier(instance)
             ? 0
             : 1;
}
