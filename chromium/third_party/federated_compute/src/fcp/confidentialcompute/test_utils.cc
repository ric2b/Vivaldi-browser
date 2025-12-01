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

#include "fcp/confidentialcompute/test_utils.h"

#include <optional>
#include <string>

#include "google/protobuf/any.pb.h"
#include "fcp/protos/confidentialcompute/file_info.pb.h"
#include "federated_language/proto/computation.pb.h"
#include "tensorflow_federated/proto/v0/executor.pb.h"

namespace fcp {
namespace confidential_compute {

tensorflow_federated::v0::Value CreateFileInfoValue(
    const std::string& uri, std::optional<std::string> key,
    std::optional<federated_language::Type> type) {
  fcp::confidentialcompute::FileInfo file_info;
  file_info.set_uri(uri);
  if (key.has_value()) {
    file_info.set_key(*key);
  }
  tensorflow_federated::v0::Value value;
  value.mutable_computation()->mutable_data()->mutable_content()->PackFrom(
      file_info);
  if (type.has_value()) {
    *value.mutable_computation()->mutable_type() = *type;
  }
  return value;
}

}  // namespace confidential_compute
}  // namespace fcp
