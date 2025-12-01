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

#ifndef FCP_CLIENT_CACHE_RESOURCE_CACHE_H_
#define FCP_CLIENT_CACHE_RESOURCE_CACHE_H_

#include <optional>

#include "google/protobuf/any.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"

namespace fcp {
namespace client {
namespace cache {

/**
 * A ResourceCache is an interface for a cache that stores resources (entries)
 * for a certain duration. A resource consists of an absl::Cord payload and
 * accompanying metadata, keyed by a string ID.
 */
class ResourceCache {
 public:
  struct ResourceAndMetadata {
    absl::Cord resource;
    google::protobuf::Any metadata;
  };

  virtual ~ResourceCache() = default;

  // Stores resource`under key cache_id. Will be deleted after now + max_age. If
  // cache_id already exists, the corresponding resource and metadata will be
  // overwritten. Metadata will be returned along with the resource when Get()
  // is called. A Resource is *not* guaranteed to be cached until now + max_age,
  // implementations may choose to delete resources earlier if needed. Returns
  // Ok on success On error, returns
  // - INTERNAL - unexpected error.
  // - INVALID_ARGUMENT - if max_age is in the past.
  // - RESOURCE_EXHAUSTED - if the resource is too big too be cached.
  virtual absl::Status Put(absl::string_view cache_id,
                           const absl::Cord& resource,
                           const google::protobuf::Any& metadata,
                           absl::Duration max_age) = 0;

  // Returns resource along with caller-provided stored metadata.
  // If max_age is set, the stored max_age for this cache_id will be updated.
  // Returns Ok on success
  // On error, returns
  // - INTERNAL - unexpected error.
  // - NOT_FOUND - if cache_id not in ResourceCache
  virtual absl::StatusOr<ResourceAndMetadata> Get(
      absl::string_view cache_id, std::optional<absl::Duration> max_age) = 0;
};

}  // namespace cache
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_CACHE_RESOURCE_CACHE_H_
