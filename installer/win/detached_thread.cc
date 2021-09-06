// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "installer/win/detached_thread.h"

#include <process.h>

#include "base/check.h"

namespace vivaldi {

namespace {

void __cdecl DetachedThreadEntryPoint(void* data) {
  // This thread owns the Thread object. Make sure we delete it after Run().
  std::unique_ptr<DetachedThread> thread(static_cast<DetachedThread*>(data));
  thread->Run();
}

}  // namespace

/*static*/
void DetachedThread::Start(std::unique_ptr<DetachedThread> thread) {
  // Use _beginthread that closes the thread handle automatically.
  uintptr_t result = _beginthread(&DetachedThreadEntryPoint,
                                  0,            // default stack size
                                  thread.get()  // arguments
  );

  // The result can only be used for checking a success as the thread can
  // terminate at this point and release its resources.
  CHECK(result);

  (void)thread.release();
}

}  // namespace vivaldi
