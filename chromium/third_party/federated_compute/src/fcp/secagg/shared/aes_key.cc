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

#include "fcp/secagg/shared/aes_key.h"

#include <string>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"

static constexpr int kLegacyKeySize = 17;

namespace fcp {
namespace secagg {

AesKey::AesKey(const uint8_t* data, int key_size) : Key(data, key_size) {
  FCP_CHECK((key_size > 0 && key_size <= 17) || (key_size == 32));
}

StatusOr<AesKey> AesKey::CreateFromShares(
    const std::vector<ShamirShare>& shares, int threshold) {
  ShamirSecretSharing reconstructor;
  // TODO(team): Once Java support is removed, assume 32 byte keys.
  int key_length = 0;
  // For compatibility, we need to know if the key that was shared was 128 or
  // 256 bits long. It can only have been one of those two lengths, so the
  // shares should be either 20 or 36 bytes long respectively.
  for (int i = 0; i < shares.size() && key_length == 0; ++i) {
    if (shares[i].data.size() == 36) {
      key_length = kSize;
    } else if (shares[i].data.size() == 20) {
      key_length = kLegacyKeySize;  // May be 17 bytes or shorter, see below
    } else {
      // Key share must be missing if it's not one of those lengths.
      FCP_CHECK(shares[i].data.empty());
    }
  }
  FCP_CHECK(key_length != 0);
  std::string reconstructed;
  FCP_ASSIGN_OR_RETURN(
      reconstructed, reconstructor.Reconstruct(threshold, shares, key_length));

  if (key_length == kLegacyKeySize) {
    // The key produced on Java side normally has 16 bytes, however when
    // exporting the key from BigInteger to byte array an extra zero byte is
    // added at the front if the high-order bit was '1' to indicate that the
    // BigInteger value was positive (to avoid treating the high order bit
    // as the sign bit). However the byte array may also be shorter than
    // 16 bytes if the BigInteger value was smaller.
    // For compatibility with Java behavior any leading zero byte that isn't
    // followed by a byte with '1' in the high-order bit need to be removed.
    int index = 0;
    while (index < kLegacyKeySize - 1 &&
           static_cast<uint8_t>(reconstructed[index]) == 0 &&
           static_cast<uint8_t>(reconstructed[index + 1]) <= 127) {
      index++;
    }

    if (index > 0) {
      reconstructed.erase(0, index);
      key_length -= index;
    }
  }
  return AesKey(reinterpret_cast<const uint8_t*>(reconstructed.c_str()),
                key_length);
}

}  // namespace secagg
}  // namespace fcp
