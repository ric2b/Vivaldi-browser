/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FCP_SECAGG_SHARED_AES_KEY_H_
#define FCP_SECAGG_SHARED_AES_KEY_H_

#include "fcp/base/monitoring.h"
#include "fcp/secagg/shared/key.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"

namespace fcp {
namespace secagg {
// A Key specifically intended for use with AES symmetric encryption.
// Keys originating on Java clients are 17 bytes or shorter (typically
// 16 or 17 bytes, but sometimes shorter).
// Keys originating on C++ clients must have 32 bytes.
// A 0-byte key should not be used for anything, and represents the absence of
// a key in a collection of keys.
//
class AesKey : public Key {
 public:
  static constexpr int kSize = 32;  // Expected key size for AES-256

  // The key is blank.
  AesKey() : Key() {}

  // The key is a standard-size 32 byte key.
  explicit AesKey(const uint8_t* data, int key_size = kSize);

  // Create a key by reconstructing it from key shares. Length depends on the
  // key shares, and may not be 32 bytes. Threshold is the threshold used when
  // the secret was shared, i.e. the minimum number of clients to reconstruct.
  static StatusOr<AesKey> CreateFromShares(
      const std::vector<ShamirShare>& shares, int threshold);
};
}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_AES_KEY_H_
