// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_SCOPED_WAKE_LOCK_H_
#define UTIL_SCOPED_WAKE_LOCK_H_

#include <memory>

#include "platform/api/task_runner.h"
#include "platform/api/task_runner_deleter.h"

namespace openscreen {

// Ensures that the device does not got to sleep. This is used, for example,
// while Open Screen is communicating with peers over the network for things
// like media streaming.
//
// The wake lock is RAII: It is automatically engaged when the ScopedWakeLock is
// created and released when the ScopedWakeLock is destroyed. Open Screen code
// may sometimes create multiple instances. In that case, the wake lock should
// be engaged upon creating the first instance, and then held until all
// instances have been destroyed.
//
// TODO(issuetracker.google.com/288311411): Implement for Linux.

class ScopedWakeLock;
using ScopedWakeLockPtr = std::unique_ptr<ScopedWakeLock, TaskRunnerDeleter>;

class ScopedWakeLock {
 public:
  static ScopedWakeLockPtr Create(TaskRunner& task_runner);

  // Instances are not copied nor moved.
  ScopedWakeLock(const ScopedWakeLock&) = delete;
  ScopedWakeLock(ScopedWakeLock&&) noexcept = delete;
  ScopedWakeLock& operator=(const ScopedWakeLock&) = delete;
  ScopedWakeLock& operator=(ScopedWakeLock&&) noexcept = delete;

  ScopedWakeLock();
  virtual ~ScopedWakeLock();
};

}  // namespace openscreen

#endif  // UTIL_SCOPED_WAKE_LOCK_H_
