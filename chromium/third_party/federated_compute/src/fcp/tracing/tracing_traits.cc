// Copyright 2021 Google LLC
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

#include "fcp/tracing/tracing_traits.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"

namespace fcp {

class UnknownTracingTraits : public TracingTraitsBase {
 public:
  ABSL_MUST_USE_RESULT const char* Name() const override { return "?Unknown?"; }
  ABSL_MUST_USE_RESULT TracingSeverity Severity() const override {
    return TracingSeverity::kWarning;
  };
  ABSL_MUST_USE_RESULT std::string TextFormat(
      const flatbuffers::DetachedBuffer& buf) const override {
    return "";
  };
  ABSL_MUST_USE_RESULT std::string JsonStringFormat(
      const uint8_t* flatbuf_bytes) const override {
    return "";
  };
};

absl::flat_hash_map<TracingTag, std::unique_ptr<TracingTraitsBase>>&
GetTracingTraitsRegistry() {
  static auto tracing_traits_registry =
      new absl::flat_hash_map<TracingTag, std::unique_ptr<TracingTraitsBase>>();
  return *tracing_traits_registry;
}

TracingTraitsBase const* TracingTraitsBase::Lookup(TracingTag tag) {
  auto it = GetTracingTraitsRegistry().find(tag);
  if (it == GetTracingTraitsRegistry().end()) {
    static auto unknown_tracing_traits = new UnknownTracingTraits();
    return unknown_tracing_traits;
  }
  return it->second.get();
}

void TracingTraitsBase::Register(TracingTag tag,
                                 std::unique_ptr<TracingTraitsBase> traits) {
  GetTracingTraitsRegistry()[tag] = std::move(traits);
}

}  // namespace fcp
