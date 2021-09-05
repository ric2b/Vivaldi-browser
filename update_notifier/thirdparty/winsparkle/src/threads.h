// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_THREADS_H_
#define UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_THREADS_H_

#include <memory>

namespace winsparkle {

// Thread that runs independently from the parent.
//
// NOTE(igor@vivaldi.com): This is based directly on Win32 API, not on
// base/threading/simple_thread. It takes more efforts to customize the latter
// to get a type-safe wrapper that exposes only the needed functionality.
class DetachedThread {
 public:
  DetachedThread() = default;
  virtual ~DetachedThread() noexcept = default;
  DetachedThread(const DetachedThread&) = delete;
  DetachedThread& operator=(const DetachedThread&) = delete;

  // Calls thread.Run() in the new thread's context and transfer the ownership
  // of the thread object to it.
  static void Start(std::unique_ptr<DetachedThread> thread);

  virtual void Run() = 0;
};

}  // namespace winsparkle

#endif  // UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_THREADS_H_
