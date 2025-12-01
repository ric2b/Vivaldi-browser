// Copyright 2024 Google LLC
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

#ifndef FCP_CONFIDENTIALCOMPUTE_TFF_EXECUTION_HELPER_H_
#define FCP_CONFIDENTIALCOMPUTE_TFF_EXECUTION_HELPER_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "tensorflow_federated/cc/core/impl/executors/executor.h"
#include "tensorflow_federated/proto/v0/executor.pb.h"

namespace fcp {
namespace confidential_compute {

// Extracts a value from a struct at a given encoded key.
//
// A key such as "bar/0/1" indicates that we should get the element with name
// "bar" from the input struct, get the 0th element from the resulting
// intermediate struct, and finally return the 1st element from that struct.
// If a portion of the delimited key can be converted into an int, it is
// assumed to represent an index into the struct as opposed to a struct element
// name.
//
// Note that the input value may be federated (or may contain federated values).
absl::StatusOr<tensorflow_federated::v0::Value> ExtractStructValue(
    const tensorflow_federated::v0::Value& value, absl::string_view key);

struct ReplaceDatasResult {
  tensorflow_federated::v0::Value replaced_value;
  bool contains_client_upload = false;
};

// Iterates through the input value and replaces all Data values with the values
// they point to. Data values are resolved using the provided fetch_data_fn or
// fetch_client_data_fn for client uploaded data.
//
// fetch_data_fn takes a single argument, the URI of the Data value to resolve,
// and returns the resolved value.
//
// fetch_client_data_fn takes two arguments, the URI of the Data value to
// resolve and the key of the tensor within the Data value. It returns the
// tensor data as a resolved value.
//
// The input value may contain a mix of Data values and non-Data values.
absl::StatusOr<ReplaceDatasResult> ReplaceDatas(
    tensorflow_federated::v0::Value value,
    std::function<absl::StatusOr<tensorflow_federated::v0::Value>(std::string)>
        fetch_data_fn,
    std::optional<std::function<absl::StatusOr<tensorflow_federated::v0::Value>(
        std::string, std::string)>>
        fetch_client_data_fn = std::nullopt);

// Embeds the provided value into the provided TFF execution stack.
absl::StatusOr<std::shared_ptr<tensorflow_federated::OwnedValueId>> Embed(
    const tensorflow_federated::v0::Value& value,
    std::shared_ptr<tensorflow_federated::Executor> executor);

}  // namespace confidential_compute
}  // namespace fcp

#endif  // FCP_CONFIDENTIALCOMPUTE_TFF_EXECUTION_HELPER_H_
