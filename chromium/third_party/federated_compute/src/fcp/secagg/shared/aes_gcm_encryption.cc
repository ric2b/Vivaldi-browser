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

#include "fcp/secagg/shared/aes_gcm_encryption.h"

#include <cstdint>
#include <string>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/shared/aes_key.h"
#include "fcp/secagg/shared/prng.h"
#include "openssl/cipher.h"
#include "openssl/evp.h"
#include "openssl/rand.h"

namespace fcp {
namespace secagg {

constexpr int kIvSize = 12;
constexpr int kTagSize = 16;

AesGcmEncryption::AesGcmEncryption() = default;

std::string AesGcmEncryption::Encrypt(const AesKey& key,
                                      const std::string& plaintext) {
  FCP_CHECK(key.size() != 0) << "Encrypt called with blank key.";
  FCP_CHECK(key.size() == AesKey::kSize)
      << "Encrypt called with key of " << key.size()
      << " bytes, but 32 bytes are required.";
  std::vector<uint8_t> ciphertext_buffer;
  ciphertext_buffer.resize(kIvSize + plaintext.length() + kTagSize);
  FCP_CHECK(RAND_bytes(ciphertext_buffer.data(), kIvSize));

  // ScopedEVP_AEAD_CTX will automatically call EVP_AEAD_CTX_cleanup when going
  // out of scope.
  bssl::ScopedEVP_AEAD_CTX ctx;
  FCP_CHECK(EVP_AEAD_CTX_init(ctx.get(), EVP_aead_aes_256_gcm(),
                              const_cast<uint8_t*>(key.data()), key.size(),
                              EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr) == 1);
  size_t len;
  FCP_CHECK(EVP_AEAD_CTX_seal(
                ctx.get(), ciphertext_buffer.data() + kIvSize, &len,
                plaintext.size() + kTagSize, ciphertext_buffer.data(), kIvSize,
                reinterpret_cast<const uint8_t*>(plaintext.c_str()),
                plaintext.size(), nullptr, 0) == 1);
  return std::string(ciphertext_buffer.begin(), ciphertext_buffer.end());
}

StatusOr<std::string> AesGcmEncryption::Decrypt(const AesKey& key,
                                                const std::string& ciphertext) {
  FCP_CHECK(key.size() != 0) << "Decrypt called with blank key.";
  FCP_CHECK(key.size() == AesKey::kSize)
      << "Decrypt called with key of " << key.size()
      << " bytes, but 32 bytes are required.";
  if (ciphertext.size() < kIvSize + kTagSize) {
    return FCP_STATUS(DATA_LOSS) << "Ciphertext is too short.";
  }
  size_t len;
  std::vector<uint8_t> plaintext_buffer;
  plaintext_buffer.resize(ciphertext.size() - kIvSize - kTagSize);

  // ScopedEVP_AEAD_CTX will automatically call EVP_AEAD_CTX_cleanup when going
  // out of scope.
  bssl::ScopedEVP_AEAD_CTX ctx;
  FCP_CHECK(EVP_AEAD_CTX_init(ctx.get(), EVP_aead_aes_256_gcm(),
                              const_cast<uint8_t*>(key.data()), key.size(),
                              EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr) == 1);
  if (EVP_AEAD_CTX_open(
          ctx.get(), plaintext_buffer.data(), &len, plaintext_buffer.size(),
          reinterpret_cast<const uint8_t*>(ciphertext.data()), kIvSize,
          reinterpret_cast<const uint8_t*>(ciphertext.data() + kIvSize),
          ciphertext.size() - kIvSize, nullptr, 0) != 1) {
    return FCP_STATUS(DATA_LOSS) << "Verification of ciphertext failed.";
  }
  return std::string(plaintext_buffer.begin(), plaintext_buffer.end());
}

}  // namespace secagg
}  // namespace fcp
