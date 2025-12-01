/*
 * Copyright 2021 Google LLC
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

#ifndef FCP_TRACING_TRACING_SPAN_ID_H_
#define FCP_TRACING_TRACING_SPAN_ID_H_

#include <atomic>
#include <cstdint>
#include <ostream>

namespace fcp {

// Uniquely identifies tracing span within a process.
struct TracingSpanId {
  std::int64_t value;

  // Generates next unique id
  static TracingSpanId NextUniqueId();
  explicit constexpr TracingSpanId(int id) : value(id) {}

 private:
  static std::atomic<std::int64_t> id_source;
};

inline bool operator==(const TracingSpanId& a, const TracingSpanId& b) {
  return a.value == b.value;
}

inline bool operator!=(const TracingSpanId& a, const TracingSpanId& b) {
  return a.value != b.value;
}

// Overload comparison operators to make it possible to sort by order of ID
// generation.
inline bool operator<(const TracingSpanId& a, const TracingSpanId& b) {
  return a.value < b.value;
}

inline std::ostream& operator<<(std::ostream& s, const TracingSpanId& id) {
  return s << id.value;
}

template <typename H>
H AbslHashValue(H h, const TracingSpanId& id) {
  return H::combine(std::move(h), id.value);
}

}  // namespace fcp

#endif  // FCP_TRACING_TRACING_SPAN_ID_H_
