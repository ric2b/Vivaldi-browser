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

#ifndef FCP_SECAGG_SHARED_AES_GCM_ENCRYPTION_H_
#define FCP_SECAGG_SHARED_AES_GCM_ENCRYPTION_H_

#include <string>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/shared/aes_key.h"
#include "openssl/evp.h"

namespace fcp {
namespace secagg {

// A class to handle encryption and decryption using AES-256-GCM.
// This class is NOT thread-safe.
class AesGcmEncryption {
 public:
  AesGcmEncryption();

  // Encrypts the plaintext with the given key, using AES-256-GCM. Prepends an
  // IV randomly generated with the given prng to the ciphertext, and appends
  // the AES-GCM tag.
  std::string Encrypt(const AesKey& key, const std::string& plaintext);

  // Decrypts the plaintext with the given key, using AES-256-GCM. Expects the
  // IV to be prepended to the ciphertext, and the tag to be appended. If the
  // tag does not authenticate, returns a DATA_LOSS error status.
  StatusOr<std::string> Decrypt(const AesKey& key,
                                const std::string& ciphertext);
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_AES_GCM_ENCRYPTION_H_
