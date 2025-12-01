// Copyright 2019 Google LLC
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

#ifndef FCP_TRACING_TRACING_TAG_H_
#define FCP_TRACING_TRACING_TAG_H_

#include <stdint.h>

#include <iosfwd>
#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "flatbuffers/flatbuffers.h"

namespace fcp {

// 4-character tag uniquely identifying FlatBuffers tables used in tracing.
union TracingTag {
  char data[4];
  uint32_t value;
  // Constructing from 4-char string literals. Supplied char array
  // has a length of 5, to accommodate terminating 0, which is NOT stored in the
  // tag.
  explicit constexpr TracingTag(char const (&fourCharsLiteral)[5])
      : data{fourCharsLiteral[0], fourCharsLiteral[1], fourCharsLiteral[2],
             fourCharsLiteral[3]} {}

  ABSL_MUST_USE_RESULT std::string str() const {
    return std::string{data[0], data[1], data[2], data[3]};
  }
  TracingTag() = delete;

  static inline const TracingTag* FromFlatbuf(
      const flatbuffers::DetachedBuffer& buf) {
    return reinterpret_cast<const TracingTag*>(&buf.data()[4]);
  }
};

inline std::ostream& operator<<(std::ostream& out, const TracingTag& tag) {
  out << tag.str();
  return out;
}

inline bool operator==(TracingTag const& a, TracingTag const& b) {
  return a.value == b.value;
}

inline bool operator!=(TracingTag const& a, TracingTag const& b) {
  return a.value != b.value;
}

template <typename H>
H AbslHashValue(H h, const fcp::TracingTag& t) {
  return H::combine(std::move(h), t.value);
}
}  // namespace fcp

#endif  // FCP_TRACING_TRACING_TAG_H_
