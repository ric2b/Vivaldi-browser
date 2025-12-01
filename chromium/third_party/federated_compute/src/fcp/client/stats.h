/*
 * Copyright 2022 Google LLC
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
#ifndef FCP_CLIENT_STATS_H_
#define FCP_CLIENT_STATS_H_

#include <cstdint>

#include "absl/time/time.h"

namespace fcp {
namespace client {

struct NetworkStats {
  // The estimated number of bytes downloaded from the network ("over the
  // wire").
  int64_t bytes_downloaded = 0;
  // The estimated number of bytes uploaded to the network ("over the
  // wire").
  int64_t bytes_uploaded = 0;
  // The best estimate of the duration of wall clock time spent waiting for
  // network requests to finish (but, for example, excluding any idle time spent
  // waiting between issuing polling requests).
  absl::Duration network_duration = absl::ZeroDuration();

  // Returns the difference between two sets of network stats.
  NetworkStats operator-(const NetworkStats& other) const {
    return {.bytes_downloaded = bytes_downloaded - other.bytes_downloaded,
            .bytes_uploaded = bytes_uploaded - other.bytes_uploaded,
            .network_duration = network_duration - other.network_duration};
  }

  NetworkStats operator+(const NetworkStats& other) const {
    return {.bytes_downloaded = bytes_downloaded + other.bytes_downloaded,
            .bytes_uploaded = bytes_uploaded + other.bytes_uploaded,
            .network_duration = network_duration + other.network_duration};
  }

  NetworkStats& operator+=(const NetworkStats& other) {
    *this = *this + other;
    return *this;
  }
};

inline bool operator==(const NetworkStats& s1, const NetworkStats& s2) {
  return

      s1.bytes_downloaded == s2.bytes_downloaded &&
      s1.bytes_uploaded == s2.bytes_uploaded &&
      s1.network_duration == s2.network_duration;
}

struct ExampleStats {
  // For TensorlowSpec-based plans, this refers to the overall number of
  // elements returned by all ExampleIterator::Next() calls. For
  // ExampleQuerySpec-based plans, this refers to the total number of row counts
  // calculated at example store layer and passed via ExampleQueryResults.
  int example_count = 0;
  int64_t example_size_bytes = 0;
};

inline bool operator==(const ExampleStats& s1, const ExampleStats& s2) {
  return s1.example_count == s2.example_count &&
         s1.example_size_bytes == s2.example_size_bytes;
}

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_STATS_H_
