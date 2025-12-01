// Copyright 2020 Google LLC
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

#include "fcp/tracing/tracing_context_utils.h"

#include <optional>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "fcp/base/monitoring.h"
#include "flatbuffers/flatbuffers.h"

namespace fcp::tracing_internal {

void SetTracingContextOnMessage(const google::protobuf::Message& context,
                                google::protobuf::Message& message) {
  const google::protobuf::FieldDescriptor* field_descriptor =
      message.GetDescriptor()->FindFieldByName(kContextFieldName);
  if (field_descriptor == nullptr) {
    return;
  }
  FCP_CHECK(field_descriptor->type() == google::protobuf::FieldDescriptor::TYPE_BYTES ||
            field_descriptor->type() == google::protobuf::FieldDescriptor::TYPE_STRING)
      << kContextWrongTypeMessage;
  message.GetReflection()->SetString(&message, field_descriptor,
                                     context.SerializeAsString());
}

}  // namespace fcp::tracing_internal
