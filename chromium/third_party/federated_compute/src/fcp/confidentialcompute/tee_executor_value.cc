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

#include "fcp/confidentialcompute/tee_executor_value.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "fcp/base/monitoring.h"
#include "fcp/confidentialcompute/constants.h"
#include "fcp/protos/confidentialcompute/file_info.pb.h"
#include "federated_language/proto/computation.pb.h"
#include "tensorflow_federated/proto/v0/executor.pb.h"

namespace fcp {
namespace confidential_compute {

namespace {

// A selection into a data computation value at index x is resolved by setting
// the key field in the FileInfo to "x" (or appending "/x" to an existing key
// field).
absl::StatusOr<tensorflow_federated::v0::Value> ResolveDataSelection(
    const tensorflow_federated::v0::Value& selection_source,
    uint32_t selection_index) {
  federated_language::Data data_value = selection_source.computation().data();
  fcp::confidentialcompute::FileInfo file_info;
  data_value.content().UnpackTo(&file_info);
  if (file_info.key().empty()) {
    file_info.set_key(absl::StrCat(selection_index));
  } else {
    file_info.set_key(
        absl::StrCat(file_info.key(), kFileInfoKeySeparator, selection_index));
  }
  federated_language::Data selected_data_value;
  selected_data_value.mutable_content()->PackFrom(file_info);
  tensorflow_federated::v0::Value value_pb;
  *value_pb.mutable_computation()->mutable_data() = selected_data_value;

  if (!selection_source.computation().has_type()) {
    return absl::InvalidArgumentError(
        "Selection source missing type information");
  }
  if (!selection_source.computation().type().has_struct_()) {
    return absl::InvalidArgumentError(
        "Selection source value type must be a struct.");
  } else if (selection_source.computation().type().struct_().element_size() <=
             selection_index) {
    return absl::InvalidArgumentError(
        "Selection index is out of range for struct type");
  }
  *value_pb.mutable_computation()->mutable_type() =
      selection_source.computation()
          .type()
          .struct_()
          .element(selection_index)
          .value();
  return value_pb;
}

}  // namespace

// Consider moving this implementation to the executors and using
// ParallelTasks with the executor thread_pools if this function proves to be
// a bottleneck (due to serialization of large structures).
absl::StatusOr<tensorflow_federated::v0::Value>
ExecutorValue::resolve_to_value() const {
  switch (type_) {
    case ValueType::VALUE:
      return value();
    case ValueType::STRUCT: {
      tensorflow_federated::v0::Value value_pb;
      // Create a struct field first so that we don't return an empty Value
      // proto if there are no elements in the struct.
      value_pb.mutable_struct_();
      FCP_ASSIGN_OR_RETURN(
          const std::shared_ptr<std::vector<ExecutorValue>> struct_elements,
          struct_elements());
      for (const ExecutorValue& element : *struct_elements) {
        FCP_ASSIGN_OR_RETURN(
            *value_pb.mutable_struct_()->add_element()->mutable_value(),
            element.resolve_to_value());
      }
      return value_pb;
    }
    case ValueType::SELECTION: {
      // Unlike core TFF, selections can be applied to data computation values
      // that use FileInfo as the embedded data content proto. Similar to core
      // TFF, selections can also be applied to struct values.
      FCP_ASSIGN_OR_RETURN(const Selection selection, selection());
      FCP_ASSIGN_OR_RETURN(tensorflow_federated::v0::Value selection_source,
                           selection.source->resolve_to_value());

      if (selection_source.has_struct_()) {
        if (selection.index >= selection_source.struct_().element_size()) {
          return absl::InvalidArgumentError(
              "Selection index is out of range for struct.");
        }
        return selection_source.struct_().element(selection.index).value();
      }

      if (!selection_source.has_computation() ||
          !selection_source.computation().has_data()) {
        return absl::InvalidArgumentError(
            "Selection source value must be a struct or data computation.");
      }

      return ResolveDataSelection(selection_source, selection.index);
    }
    case ValueType::UNKNOWN:
      return absl::InvalidArgumentError("ExecutorValue is of unknown type");
  }
}

}  // namespace confidential_compute
}  // namespace fcp
