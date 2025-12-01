// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fcp/tracing/tracing_recorder_impl.h"

#include <memory>

#include "absl/synchronization/mutex.h"
#include "fcp/base/monitoring.h"
#include "fcp/tracing/text_tracing_recorder_impl.h"

namespace fcp::tracing_internal {

class TracingState {
  absl::Mutex mutex_;
  bool using_thread_local_state_ = false;
  TracingRecorderImpl* global_tracing_recorder_ = nullptr;

  struct ThreadLocalState {
    TracingRecorderImpl* tracing_recorder = nullptr;
    // Ref count is used to track the number of times the same
    // TracingRecorderImpl has been set in case of re-entrancy.
    int ref_count = 0;
  };

  static ThreadLocalState& GetThreadLocalState() {
    thread_local static ThreadLocalState instance;
    return instance;
  }

  static std::shared_ptr<TracingRecorderImpl> GetOrCreateDefaultRecorder() {
    static auto lazy_init_instance =
        new std::shared_ptr<TextTracingRecorderImpl>(
            new TextTracingRecorderImpl(absl::LocalTimeZone()));
    return *lazy_init_instance;
  }

 public:
  std::shared_ptr<TracingRecorderImpl> GetRecorderImpl() {
    absl::ReaderMutexLock lock(&mutex_);
    TracingRecorderImpl* tracing_recorder =
        using_thread_local_state_ ? GetThreadLocalState().tracing_recorder
                                  : global_tracing_recorder_;
    return tracing_recorder ? tracing_recorder->shared_from_this()
                            : GetOrCreateDefaultRecorder();
  }

  void SetGlobalRecorderImpl(TracingRecorderImpl* impl) {
    absl::WriterMutexLock lock(&mutex_);
    FCP_CHECK(!using_thread_local_state_)
        << "Global and thread local tracing recorders can't be used at the "
           "same time";
    FCP_CHECK(global_tracing_recorder_ == nullptr || impl == nullptr)
        << "Only one global tracing recorder instance is supported";
    FCP_LOG(INFO) << "Setting global";
    global_tracing_recorder_ = impl;
  }

  void SetThreadLocalRecorderImpl(TracingRecorderImpl* impl) {
    FCP_CHECK(impl != nullptr);
    absl::WriterMutexLock lock(&mutex_);
    auto& thread_local_state = GetThreadLocalState();
    FCP_CHECK(global_tracing_recorder_ == nullptr)
        << "Global and thread local tracing recorders can't be used at the "
           "same time";
    FCP_CHECK(thread_local_state.tracing_recorder == nullptr ||
              thread_local_state.tracing_recorder == impl)
        << "Only one tracing recorder instance per thread is supported";
    thread_local_state.tracing_recorder = impl;
    thread_local_state.ref_count++;
    using_thread_local_state_ = true;
  }

  void ResetThreadLocalRecorderImpl(TracingRecorderImpl* impl) {
    FCP_CHECK(impl != nullptr);
    absl::WriterMutexLock lock(&mutex_);
    auto& thread_local_state = GetThreadLocalState();
    FCP_CHECK(thread_local_state.tracing_recorder == impl &&
              thread_local_state.ref_count > 0)
        << "Attempting to uninstall thread local tracing recorder that isn't "
           "currently installed";
    if (--thread_local_state.ref_count == 0) {
      thread_local_state.tracing_recorder = nullptr;
    }
  }

  void EnsureNotSet(TracingRecorderImpl* impl) {
    absl::WriterMutexLock lock(&mutex_);
    FCP_CHECK(global_tracing_recorder_ != impl)
        << "Trace recorder must not be set as global at destruction time";
    if (using_thread_local_state_) {
      FCP_CHECK(GetThreadLocalState().tracing_recorder != impl)
          << "Trace recorder must not be set as thread local at destruction "
             "time";
    }
  }

  static TracingState& GetInstance() {
    static TracingState* instance = new TracingState();
    return *instance;
  }
};

std::shared_ptr<TracingRecorderImpl> TracingRecorderImpl::GetCurrent() {
  return TracingState::GetInstance().GetRecorderImpl();
}

void TracingRecorderImpl::InstallAsGlobal() {
  FCP_CHECK(!is_global_);
  TracingState::GetInstance().SetGlobalRecorderImpl(this);
  is_global_ = true;
}

void TracingRecorderImpl::UninstallAsGlobal() {
  FCP_CHECK(is_global_);
  TracingState::GetInstance().SetGlobalRecorderImpl(nullptr);
  is_global_ = false;
}

void TracingRecorderImpl::InstallAsThreadLocal() {
  TracingState::GetInstance().SetThreadLocalRecorderImpl(this);
}

void TracingRecorderImpl::UninstallAsThreadLocal() {
  TracingState::GetInstance().ResetThreadLocalRecorderImpl(this);
}

TracingRecorderImpl::~TracingRecorderImpl() {
  if (is_global_) {
    UninstallAsGlobal();
  }
  TracingState::GetInstance().EnsureNotSet(this);
}

}  // namespace fcp::tracing_internal
