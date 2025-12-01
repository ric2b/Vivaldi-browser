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
#include "fcp/secagg/shared/compute_session_id.h"

#include <string>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/shared/math.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "openssl/evp.h"

namespace fcp {
namespace secagg {

SessionId ComputeSessionId(const ShareKeysRequest& request) {
  EVP_MD_CTX* ctx;
  FCP_CHECK(ctx = EVP_MD_CTX_create());
  FCP_CHECK(EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr));
  for (const PairOfPublicKeys& keys : request.pairs_of_public_keys()) {
    int noise_pk_size = keys.noise_pk().size();
    std::string noise_pk_size_data = IntToByteString(noise_pk_size);
    int enc_pk_size = keys.enc_pk().size();
    std::string enc_pk_size_data = IntToByteString(noise_pk_size);
    FCP_CHECK(EVP_DigestUpdate(ctx, noise_pk_size_data.c_str(), sizeof(int)));
    FCP_CHECK(EVP_DigestUpdate(ctx, keys.noise_pk().c_str(), noise_pk_size));
    FCP_CHECK(EVP_DigestUpdate(ctx, enc_pk_size_data.c_str(), sizeof(int)));
    FCP_CHECK(EVP_DigestUpdate(ctx, keys.enc_pk().c_str(), enc_pk_size));
  }

  char digest[kSha256Length];
  uint32_t digest_length = 0;
  FCP_CHECK(EVP_DigestFinal_ex(ctx, reinterpret_cast<uint8_t*>(digest),
                               &digest_length));
  FCP_CHECK(digest_length == kSha256Length);
  EVP_MD_CTX_destroy(ctx);
  return {std::string(digest, kSha256Length)};
}

}  // namespace secagg
}  // namespace fcp
