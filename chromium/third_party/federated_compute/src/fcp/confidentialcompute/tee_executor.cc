/*
 * Copyright 2025 Google LLC
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

#include "fcp/confidentialcompute/tee_executor.h"

#include <cstdint>
#include <future>  // NOLINT
#include <memory>
#include <optional>
#include <thread>  // NOLINT
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "fcp/base/monitoring.h"
#include "fcp/confidentialcompute/constants.h"
#include "fcp/confidentialcompute/lambda_runner.h"
#include "fcp/confidentialcompute/tee_executor_value.h"
#include "fcp/protos/confidentialcompute/file_info.pb.h"
#include "federated_language/proto/array.pb.h"
#include "federated_language/proto/computation.pb.h"
#include "tensorflow_federated/cc/core/impl/executors/executor.h"
#include "tensorflow_federated/cc/core/impl/executors/threading.h"
#include "tensorflow_federated/proto/v0/executor.pb.h"

namespace fcp::confidential_compute {
namespace {

using ::fcp::confidential_compute::ExecutorValue;
using ::fcp::confidential_compute::kComposedTeeLeafUri;
using ::fcp::confidential_compute::kComposedTeeRootUri;

using ValueFuture = std::shared_future<absl::StatusOr<ExecutorValue>>;

class TeeExecutor : public tensorflow_federated::ExecutorBase<ValueFuture> {
 public:
  explicit TeeExecutor(std::shared_ptr<LambdaRunner> lambda_runner,
                       int32_t num_clients,
                       std::optional<int32_t> threadpool_size = std::nullopt)
      : lambda_runner_(std::move(lambda_runner)),
        num_clients_(num_clients),
        thread_pool_(
            // Use a threadpool with CPU * 4 or the user specified
            // maximum.
            threadpool_size.value_or(std::thread::hardware_concurrency() * 4),
            ExecutorName()) {
    VLOG(2) << "thread pool size: "
            << threadpool_size.value_or(std::thread::hardware_concurrency() *
                                        4);
  }
  ~TeeExecutor() override {
    // We must make sure to delete all of our OwnedValueIds, releasing them from
    // the child executor as well, before deleting the child executor.
    ClearTracked();
  }

 private:
  std::shared_ptr<LambdaRunner> lambda_runner_;
  int32_t num_clients_;
  tensorflow_federated::ThreadPool thread_pool_;

  absl::string_view ExecutorName() final { return "TeeExecutor"; }

  absl::StatusOr<ValueFuture> CreateExecutorValue(
      const tensorflow_federated::v0::Value& value_pb) final {
    return tensorflow_federated::ReadyFuture(ExecutorValue(value_pb));
  }

  absl::StatusOr<ValueFuture> CreateCall(
      ValueFuture function, std::optional<ValueFuture> argument) final {
    return ThreadRun(
        [function = std::move(function), argument = std::move(argument),
         this]() -> absl::StatusOr<ExecutorValue> {
          FCP_ASSIGN_OR_RETURN(ExecutorValue fn_value,
                               tensorflow_federated::Wait(function));
          FCP_ASSIGN_OR_RETURN(tensorflow_federated::v0::Value fn_pb,
                               fn_value.value());
          if (!fn_pb.has_computation() ||
              !fn_pb.computation().has_intrinsic()) {
            return absl::InvalidArgumentError("Function has unexpected form");
          }

          absl::string_view uri = fn_pb.computation().intrinsic().uri();
          if (!(uri == kComposedTeeLeafUri || uri == kComposedTeeRootUri)) {
            return absl::InvalidArgumentError(
                absl::StrCat("Unsupported intrinsic ", uri));
          }

          if (!argument.has_value()) {
            return absl::InvalidArgumentError("Missing required argument");
          }

          FCP_ASSIGN_OR_RETURN(ExecutorValue arg_value,
                               tensorflow_federated::Wait(argument.value()));
          FCP_ASSIGN_OR_RETURN(const std::shared_ptr<std::vector<ExecutorValue>>
                                   arg_struct_elements,
                               arg_value.struct_elements());
          if (arg_struct_elements->size() != 2) {
            return absl::InvalidArgumentError("Expected argument of size 2");
          }
          FCP_ASSIGN_OR_RETURN(
              tensorflow_federated::v0::Value arg_for_lambda_runner,
              (*arg_struct_elements)[0].resolve_to_value());
          FCP_ASSIGN_OR_RETURN(
              tensorflow_federated::v0::Value fn_for_lambda_runner,
              (*arg_struct_elements)[1].resolve_to_value());

          FCP_ASSIGN_OR_RETURN(
              tensorflow_federated::v0::Value result,
              lambda_runner_->ExecuteComp(fn_for_lambda_runner,
                                          arg_for_lambda_runner, num_clients_));

          if (!result.has_computation() || !result.computation().has_data()) {
            return absl::InternalError(
                "Expected lambda runner to return a data computation value.");
          }

          return ExecutorValue(result);
        },
        &thread_pool_);
  }

  absl::StatusOr<ValueFuture> CreateStruct(
      std::vector<ValueFuture> elements) final {
    return Map(
        std::move(elements),
        [](std::vector<ExecutorValue> elements)
            -> absl::StatusOr<ExecutorValue> {
          return ExecutorValue(std::make_shared<std::vector<ExecutorValue>>(
              std::move(elements)));
        },
        &thread_pool_);
  }

  absl::StatusOr<ValueFuture> CreateSelection(ValueFuture value,
                                              uint32_t index) final {
    return ThreadRun(
        [value = std::move(value), index]() -> absl::StatusOr<ExecutorValue> {
          FCP_ASSIGN_OR_RETURN(ExecutorValue source_val,
                               tensorflow_federated::Wait(value));
          return ExecutorValue(std::make_shared<ExecutorValue>(source_val),
                               index);
        },
        &thread_pool_);
  }

  absl::Status Materialize(ValueFuture value_fut,
                           tensorflow_federated::v0::Value* value_pb) override {
    FCP_ASSIGN_OR_RETURN(ExecutorValue value,
                         tensorflow_federated::Wait(value_fut));
    FCP_ASSIGN_OR_RETURN(*value_pb, value.resolve_to_value());
    return absl::OkStatus();
  }
};

}  // namespace

std::shared_ptr<tensorflow_federated::Executor> CreateTeeExecutor(
    std::shared_ptr<LambdaRunner> lambda_runner, int32_t num_clients) {
  return std::make_shared<TeeExecutor>(std::move(lambda_runner), num_clients);
}

}  // namespace fcp::confidential_compute
