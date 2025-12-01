// Copyright 2022 Google LLC
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

#ifndef FCP_CLIENT_CACHE_TEST_HELPERS_H_
#define FCP_CLIENT_CACHE_TEST_HELPERS_H_

#include <optional>

#include "google/protobuf/any.pb.h"
#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/client/cache/resource_cache.h"

namespace fcp {
namespace client {
namespace cache {

// A mock `ResourceCache` implementation that can be used in tests.
class MockResourceCache : public ResourceCache {
 public:
  MOCK_METHOD(absl::Status, Put,
              (absl::string_view cache_id, const absl::Cord& resource,
               const google::protobuf::Any& metadata, absl::Duration max_age),
              (override));
  MOCK_METHOD(absl::StatusOr<ResourceAndMetadata>, Get,
              (absl::string_view cache_id,
               std::optional<absl::Duration> max_age),
              (override));
};

}  // namespace cache
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_CACHE_TEST_HELPERS_H_q
