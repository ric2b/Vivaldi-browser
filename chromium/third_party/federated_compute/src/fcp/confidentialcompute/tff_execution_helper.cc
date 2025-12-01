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

#include "fcp/confidentialcompute/tff_execution_helper.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "fcp/base/monitoring.h"
#include "fcp/protos/confidentialcompute/file_info.pb.h"
#include "google/protobuf/util/message_differencer.h"
#include "federated_language/proto/computation.pb.h"
#include "tensorflow_federated/cc/core/impl/executors/cardinalities.h"
#include "tensorflow_federated/cc/core/impl/executors/executor.h"
#include "tensorflow_federated/proto/v0/executor.pb.h"

namespace fcp {
namespace confidential_compute {

namespace {

inline std::shared_ptr<tensorflow_federated::OwnedValueId> ShareValueId(
    tensorflow_federated::OwnedValueId&& id) {
  return std::make_shared<tensorflow_federated::OwnedValueId>(std::move(id));
}

}  // namespace

constexpr absl::string_view kStructureKeyPathDelimiter = "/";

absl::StatusOr<tensorflow_federated::v0::Value> ExtractStructValue(
    const tensorflow_federated::v0::Value& value, absl::string_view key) {
  // Keep a pointer to the location in the structure we are traversing,
  // this will be updated for each key in the full path in the loop below.
  const tensorflow_federated::v0::Value* current_value = &value;
  const std::vector<absl::string_view> key_parts =
      absl::StrSplit(key, kStructureKeyPathDelimiter);
  for (absl::string_view key_part : key_parts) {
    if (current_value->has_federated()) {
      if (!current_value->federated().type().all_equal()) {
        return absl::InvalidArgumentError(
            "Cannot call ExtractStructValue on a federated value with "
            "non-all-equal type");
      }
      current_value = &current_value->federated().value(0);
    }
    if (!current_value->has_struct_()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "ExtractStructValue found unsupported non-Struct value of"
          " type case [",
          current_value->value_case(), "]"));
    }
    // Keys can be indices or field names, so we need to determine whether the
    // key part is parsable integer.
    int element_index;
    if (absl::SimpleAtoi(key_part, &element_index)) {
      if (element_index >= current_value->struct_().element_size()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Ordinal key ", element_index, " is out of range [0,",
                         current_value->struct_().element_size(), ")."));
      }
      current_value = &current_value->struct_().element(element_index).value();
    } else {
      bool found_element = false;
      for (const tensorflow_federated::v0::Value_Struct_Element& element :
           current_value->struct_().element()) {
        if (element.name() == key_part) {
          current_value = &element.value();
          found_element = true;
          continue;
        }
      }
      if (!found_element) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Key ", key_part, " not found. Available fields: [",
            absl::StrJoin(
                std::vector<tensorflow_federated::v0::Value_Struct_Element>(
                    current_value->struct_().element().begin(),
                    current_value->struct_().element().end()),
                ",",
                [](std::string* out,
                   const tensorflow_federated::v0::Value_Struct_Element& e) {
                  absl::StrAppend(out, e.name());
                }),
            "]."));
      }
    }
  }
  return *current_value;
}

absl::StatusOr<ReplaceDatasResult> ReplaceDatas(
    tensorflow_federated::v0::Value value,
    std::function<absl::StatusOr<tensorflow_federated::v0::Value>(std::string)>
        fetch_data_fn,
    std::optional<std::function<absl::StatusOr<tensorflow_federated::v0::Value>(
        std::string, std::string)>>
        fetch_client_data_fn) {
  bool client_upload = false;
  if (google::protobuf::util::MessageDifferencer::Equivalent(
          value, tensorflow_federated::v0::Value())) {
    return ReplaceDatasResult{.replaced_value = value,
                              .contains_client_upload = client_upload};
  }
  tensorflow_federated::v0::Value replaced_value;
  switch (value.value_case()) {
    case tensorflow_federated::v0::Value::kArray:
    case tensorflow_federated::v0::Value::kSequence:
      return ReplaceDatasResult{.replaced_value = value,
                                .contains_client_upload = client_upload};
    case tensorflow_federated::v0::Value::kComputation:
      if (!value.computation().has_data()) {
        return ReplaceDatasResult{.replaced_value = value,
                                  .contains_client_upload = client_upload};
      } else {
        fcp::confidentialcompute::FileInfo file_info;
        value.computation().data().content().UnpackTo(&file_info);
        if (!file_info.client_upload()) {
          FCP_ASSIGN_OR_RETURN(replaced_value, fetch_data_fn(file_info.uri()));
          if (!file_info.key().empty()) {
            FCP_ASSIGN_OR_RETURN(
                replaced_value,
                ExtractStructValue(replaced_value, file_info.key()));
          }
        } else {
          if (!fetch_client_data_fn.has_value()) {
            return absl::InvalidArgumentError(
                "No function provided to fetch client data.");
          }
          FCP_ASSIGN_OR_RETURN(
              replaced_value,
              (*fetch_client_data_fn)(file_info.uri(), file_info.key()));
        }
        return ReplaceDatasResult{
            .replaced_value = replaced_value,
            .contains_client_upload = file_info.client_upload()};
      }
    case tensorflow_federated::v0::Value::kFederated:
      *replaced_value.mutable_federated()->mutable_type() =
          value.federated().type();
      for (const tensorflow_federated::v0::Value& inner_value :
           value.federated().value()) {
        FCP_ASSIGN_OR_RETURN(
            ReplaceDatasResult replacement_federated_value,
            ReplaceDatas(inner_value, fetch_data_fn, fetch_client_data_fn));
        client_upload |= replacement_federated_value.contains_client_upload;
        *replaced_value.mutable_federated()->add_value() =
            std::move(replacement_federated_value.replaced_value);
      }

      // Federated values that have a CLIENTS placement yet do not wrap any
      // client uploads are values introduced by the to_composed_tee_form
      // compiler logic in order to be able to pass pre-aggregate outputs from
      // one client child TeeExecutor to a second client child TeeExecutor. Here
      // we unwrap the federated value and return the first inner value (there
      // should only be one), since the federated value will have already been
      // received by the second TeeExecutor and we will next be trying to
      // provide this value as input to a function that expects the member type.
      if (value.federated().type().placement().value().uri() ==
              tensorflow_federated::kClientsUri &&
          !client_upload) {
        if (replaced_value.federated().value_size() != 1) {
          return absl::InvalidArgumentError(
              "A federated value with CLIENTS placement that isn't wrapping "
              "client uploads should have exactly one inner value");
        }
        return ReplaceDatasResult{
            .replaced_value = replaced_value.federated().value(0),
            .contains_client_upload = client_upload};
      }

      return ReplaceDatasResult{.replaced_value = replaced_value,
                                .contains_client_upload = client_upload};
    case tensorflow_federated::v0::Value::kStruct:
      for (const tensorflow_federated::v0::Value_Struct_Element& inner_value :
           value.struct_().element()) {
        tensorflow_federated::v0::Value_Struct_Element* replaced_element =
            replaced_value.mutable_struct_()->add_element();
        *replaced_element->mutable_name() = inner_value.name();
        FCP_ASSIGN_OR_RETURN(ReplaceDatasResult replacement_struct_value,
                             ReplaceDatas(inner_value.value(), fetch_data_fn,
                                          fetch_client_data_fn));
        *replaced_element->mutable_value() =
            std::move(replacement_struct_value.replaced_value);
      }
      return ReplaceDatasResult{.replaced_value = replaced_value,
                                .contains_client_upload = client_upload};
    default:
      return absl::InvalidArgumentError(
          "ReplaceDatas found unsupported value type");
  }
}

absl::StatusOr<std::shared_ptr<tensorflow_federated::OwnedValueId>> Embed(
    const tensorflow_federated::v0::Value& value,
    std::shared_ptr<tensorflow_federated::Executor> executor) {
  if (value.has_computation() || value.has_federated() || value.has_array()) {
    FCP_ASSIGN_OR_RETURN(tensorflow_federated::OwnedValueId child_value_id,
                         executor->CreateValue(value));
    return ShareValueId(std::move(child_value_id));
  } else if (value.has_struct_()) {
    std::vector<std::shared_ptr<tensorflow_federated::OwnedValueId>> field_ids;
    std::vector<tensorflow_federated::ValueId> unowned_field_ids;
    size_t num_fields = value.struct_().element_size();
    field_ids.reserve(num_fields);
    unowned_field_ids.reserve(num_fields);
    for (const tensorflow_federated::v0::Value_Struct_Element& field_value :
         value.struct_().element()) {
      FCP_ASSIGN_OR_RETURN(
          std::shared_ptr<tensorflow_federated::OwnedValueId>
              child_field_value_id,
          fcp::confidential_compute::Embed(field_value.value(), executor));
      unowned_field_ids.push_back(child_field_value_id->ref());
      field_ids.push_back(std::move(child_field_value_id));
    }
    FCP_ASSIGN_OR_RETURN(tensorflow_federated::OwnedValueId child_struct_id,
                         executor->CreateStruct(unowned_field_ids));
    return ShareValueId(std::move(child_struct_id));
  } else {
    return absl::InvalidArgumentError("Unsupported value type");
  }
}

}  // namespace confidential_compute
}  // namespace fcp
