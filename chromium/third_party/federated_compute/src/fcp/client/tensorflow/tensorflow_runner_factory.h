/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef FCP_CLIENT_TENSORFLOW_TENSORFLOW_RUNNER_FACTORY_H_
#define FCP_CLIENT_TENSORFLOW_TENSORFLOW_RUNNER_FACTORY_H_

#include <memory>

#include "fcp/base/function_registry.h"
#include "fcp/client/tensorflow/tensorflow_runner.h"

namespace fcp::client {
// There's only one TensorflowRunner implementation (the one in this file),
// so we have an enum with only a single entry.
enum class TensorflowRunnerImplementation {
  kTensorflowRunnerImpl,
};

// A registry of a factory function for creating TensorflowRunner
// instances, allowing us to conditionally link in the TensorflowRunner
// implementation by simply adding a BUILD dep.
using TensorflowRunnerFactoryRegistry =
    ::fcp::FunctionRegistry<TensorflowRunnerImplementation,
                            std::unique_ptr<TensorflowRunner>()>;

// Returns the TensorflowRunner factory.
TensorflowRunnerFactoryRegistry& GetGlobalTensorflowRunnerFactoryRegistry();
}  // namespace fcp::client

#endif  // FCP_CLIENT_TENSORFLOW_TENSORFLOW_RUNNER_FACTORY_H_
