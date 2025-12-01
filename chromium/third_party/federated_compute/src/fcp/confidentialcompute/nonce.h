// Copyright 2023 Google LLC
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

// This file contains utilities for generating and tracking session nonces.

#ifndef FCP_CONFIDENTIALCOMPUTE_NONCE_H_
#define FCP_CONFIDENTIALCOMPUTE_NONCE_H_

#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "fcp/protos/confidentialcompute/confidential_transform.pb.h"

namespace fcp {
namespace confidential_compute {

// Class used to track and verify a session-level nonce and blob counter.
//
// This class is not thread safe.
class NonceChecker {
 public:
  NonceChecker();
  // Checks that the BlobMetadata's counter is greater than any blob counters
  // seen so far and that RewrappedAssociatedData.nonce is correct. If the blob
  // is unencrypted, always returns OK and doesn't affect the blob counters seen
  // so far.
  absl::Status CheckBlobNonce(
      const fcp::confidentialcompute::BlobMetadata& blob_metadata);

  std::string GetSessionNonce() { return session_nonce_; }

 private:
  std::string session_nonce_;
  // The next valid blob counter. Values less than this are invalid.
  uint32_t counter_ = 0;
};

struct NonceAndCounter {
  // Unique nonce for a blob.
  std::string blob_nonce;
  // The counter value for the blob, which is encoded in the blob_nonce.
  uint32_t counter;
};

// Class used to generate the series of blob-level nonces for a given session.
//
// This class is not thread safe.
class NonceGenerator {
 public:
  explicit NonceGenerator(std::string session_nonce)
      : session_nonce_(std::move(session_nonce)) {};

  // Returns the next blob-level nonce and its associated counter. If
  // successful, increments `counter_`.
  absl::StatusOr<NonceAndCounter> GetNextBlobNonce();

 private:
  std::string session_nonce_;
  uint32_t counter_ = 0;
};

}  // namespace confidential_compute
}  // namespace fcp

#endif  // FCP_CONFIDENTIALCOMPUTE_NONCE_H_
