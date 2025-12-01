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

#ifndef FCP_CONFIDENTIALCOMPUTE_TEE_EXECUTOR_VALUE_H_
#define FCP_CONFIDENTIALCOMPUTE_TEE_EXECUTOR_VALUE_H_

#include <cstdint>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "federated_language/proto/computation.pb.h"
#include "tensorflow_federated/proto/v0/executor.pb.h"

namespace fcp {
namespace confidential_compute {

class ExecutorValue {
 public:
  enum class ValueType { VALUE, STRUCT, SELECTION, UNKNOWN };

  struct Selection {
    std::shared_ptr<ExecutorValue> source;
    uint32_t index = 0;
  };

  absl::StatusOr<const tensorflow_federated::v0::Value> value() const {
    if (type_ != ValueType::VALUE) {
      return absl::InvalidArgumentError("ExecutorValue is not a value");
    }
    return std::get<tensorflow_federated::v0::Value>(value_);
  }

  absl::StatusOr<const std::shared_ptr<std::vector<ExecutorValue>>>
  struct_elements() const {
    if (type_ != ValueType::STRUCT) {
      return absl::InvalidArgumentError("ExecutorValue is not a struct");
    }
    return std::get<std::shared_ptr<std::vector<ExecutorValue>>>(value_);
  }

  absl::StatusOr<const Selection> selection() const {
    if (type_ != ValueType::SELECTION) {
      return absl::InvalidArgumentError("ExecutorValue is not a selection");
    }
    return std::get<Selection>(value_);
  }

  // Convert this ExecutorValue to a Value proto. If this value is a struct or
  // selection, this will recursively resolve all of the elements of the struct
  // or selection to their corresponding Value protos.
  absl::StatusOr<tensorflow_federated::v0::Value> resolve_to_value() const;

  ValueType type() const { return type_; }

  explicit ExecutorValue(tensorflow_federated::v0::Value value)
      : value_(std::move(value)), type_(ValueType::VALUE) {}

  explicit ExecutorValue(
      std::shared_ptr<std::vector<ExecutorValue>> struct_elements)
      : value_(std::move(struct_elements)), type_(ValueType::STRUCT) {}

  ExecutorValue(std::shared_ptr<ExecutorValue> selection_source, uint32_t index)
      : value_(Selection{std::move(selection_source), index}),
        type_(ValueType::SELECTION) {}

 private:
  ExecutorValue() = delete;

  std::variant<tensorflow_federated::v0::Value,
               std::shared_ptr<std::vector<ExecutorValue>>, Selection>
      value_;
  ValueType type_ = ValueType::UNKNOWN;
};

}  // namespace confidential_compute
}  // namespace fcp

#endif  // FCP_CONFIDENTIALCOMPUTE_TEE_EXECUTOR_VALUE_H_
