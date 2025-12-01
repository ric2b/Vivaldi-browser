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

#ifndef FCP_TRACING_TRACING_RECORDER_H_
#define FCP_TRACING_TRACING_RECORDER_H_

#include "fcp/tracing/tracing_span.h"
namespace fcp {

// Interface to be implemented by tracing recorders, which are responsible
// for implementation behind the TracingSpan API.
// A tracing recorder provides ability to create root span.
class TracingRecorder {
 public:
  // TracingRecorder is neither copyable nor movable.
  TracingRecorder(const TracingRecorder&) = delete;
  TracingRecorder& operator=(const TracingRecorder&) = delete;

  TracingRecorder() = default;

  // It is OK to destruct this facade API object anytime, since underlying
  // implementation lifetime is independent from the facade and automatically
  // prolonged by active tracing span (in this or other threads)
  virtual ~TracingRecorder() = default;

  // Installs tracing recorder as global instance.
  // It uninstalls automatically upon destruction of underlying implementation.
  // Only one instance can be installed and this operation will fail if other
  // recorder is installed as global.
  virtual void InstallAsGlobal() = 0;

  // Uninstalls tracing recorder as global instance. Allowed to be called only
  // if InstallAsGlobal() was called.
  // NOTE: if some concurrent threads have active tracing spans on their stacks,
  // they can continue tracing with the tracing recorder even after uninstalling
  // it as global.
  virtual void UninstallAsGlobal() = 0;

  // Installs tracing recorder as thread local instance.
  // Only one instance can be installed per thread, and this operation will fail
  // if other recorder is installed for the current thread.
  virtual void InstallAsThreadLocal() = 0;

  // Uninstalls tracing recorder as thread local instance. Allowed to be called
  // only if InstallAsThreadLocal has been called.
  virtual void UninstallAsThreadLocal() = 0;
};

}  // namespace fcp

#endif  // FCP_TRACING_TRACING_RECORDER_H_
