// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TASK_RUNNER_DELETER_H_
#define PLATFORM_API_TASK_RUNNER_DELETER_H_

#include <memory>
#include <utility>

#include "platform/api/task_runner.h"

namespace openscreen {

// Helper that deletes an object on the provided TaskRunner.
//
// Usage with std::unique_ptr:
//
// std::unique_ptr<Foo, TaskRunnerDeleter> some_foo;
// ...
// some_foo = TaskRunnerDeleter::MakeUnique(
//  task_runner, foo_arg1, foo_arg2, ...);
struct TaskRunnerDeleter {
  TaskRunnerDeleter();
  explicit TaskRunnerDeleter(TaskRunner& task_runner);
  ~TaskRunnerDeleter();

  TaskRunnerDeleter(const TaskRunnerDeleter&);
  TaskRunnerDeleter& operator=(const TaskRunnerDeleter&);
  TaskRunnerDeleter(TaskRunnerDeleter&&) noexcept;
  TaskRunnerDeleter& operator=(TaskRunnerDeleter&&) noexcept;

  // For compatibility with std:: deleters.
  template <typename T>
  void operator()(const T* ptr) {
    if (task_runner_ && ptr)
      task_runner_->DeleteSoon(ptr);
  }

  template <typename Type, typename Deleter = TaskRunnerDeleter>
  static std::unique_ptr<Type, Deleter> WrapUnique(TaskRunner& task_runner,
                                                   Type* t) {
    return std::unique_ptr<Type, Deleter>(t, TaskRunnerDeleter(task_runner));
  }

  template <typename Type,
            typename Deleter = TaskRunnerDeleter,
            typename... Args>
  static std::unique_ptr<Type, Deleter> MakeUnique(TaskRunner& task_runner,
                                                   Args&&... args) {
    return std::unique_ptr<Type, Deleter>(
        new Type(std::forward<Args>(args)...),
        TaskRunnerDeleter(task_runner));  // NOLINT
  }

  TaskRunner* task_runner_{nullptr};
};

}  // namespace openscreen

#endif  // PLATFORM_API_TASK_RUNNER_DELETER_H_
