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

#ifndef FCP_TRACING_TRACING_CONTEXT_UTILS_H_
#define FCP_TRACING_TRACING_CONTEXT_UTILS_H_

#include <string>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "fcp/base/monitoring.h"
#include "fcp/tracing/tracing_traits.h"
#include "flatbuffers/flatbuffers.h"

namespace fcp::tracing_internal {

constexpr char kContextFieldName[] = "tracing_context";
constexpr char kContextWrongTypeMessage[] =
    "Type of tracing_context field should be bytes or string";

// Given a proto message, checks to see if it has a tracing_context field and
// if so, serializes the provided `context` message and sets the field contents
// to the serialized context.
// If no tracing_context field is present on `message` or it is of a type other
// than string or bytes, this will be a no-op.
void SetTracingContextOnMessage(const google::protobuf::Message& context,
                                google::protobuf::Message& message);

// Given a proto `message` which may have a field tracing_context which contains
// a serialized tracing context proto of type ContextT, uses reflection to
// extract and parse the serialized proto to return a ContextT.
// If the proto `message` does not have a tracing_context field, or it is empty,
// returns the default value of ContextT.
// If the tracing_context field is of a type other than string or bytes, this
// will fail.
template <class ContextT>
ContextT GetContextFromMessage(const google::protobuf::Message& message) {
  const google::protobuf::FieldDescriptor* field_descriptor =
      message.GetDescriptor()->FindFieldByName(kContextFieldName);
  ContextT context;
  if (field_descriptor == nullptr) {
    return context;
  }
  FCP_CHECK(field_descriptor->type() == google::protobuf::FieldDescriptor::TYPE_BYTES ||
            field_descriptor->type() == google::protobuf::FieldDescriptor::TYPE_STRING)
      << kContextWrongTypeMessage;
  std::string serialized_context =
      message.GetReflection()->GetString(message, field_descriptor);
  if (serialized_context.empty()) {
    return context;
  }
  FCP_CHECK(context.ParseFromString(serialized_context));

  return context;
}

}  // namespace fcp::tracing_internal

#endif  // FCP_TRACING_TRACING_CONTEXT_UTILS_H_
