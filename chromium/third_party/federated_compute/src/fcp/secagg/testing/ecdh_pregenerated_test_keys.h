/*
 * Copyright 2018 Google LLC
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

#ifndef FCP_SECAGG_TESTING_ECDH_PREGENERATED_TEST_KEYS_H_
#define FCP_SECAGG_TESTING_ECDH_PREGENERATED_TEST_KEYS_H_

#include <string>
#include <vector>

#include "fcp/secagg/shared/ecdh_keys.h"

namespace fcp {
namespace secagg {

// This class contains some pregenerated ECDH public/private keypairs. In no
// actual implementation should pregenerated keys such as these be used.
class EcdhPregeneratedTestKeys {
 public:
  // Valid inputs for all functions are integers from 0 to kNumTestEcdhKeys.
  static constexpr int kNumTestEcdhKeys = 8;

  EcdhPregeneratedTestKeys();

  // Returns a public or private key.
  EcdhPrivateKey GetPrivateKey(size_t index);
  EcdhPublicKey GetPublicKey(size_t index);

  // Returns a public or private key in the form of a string.
  std::string GetPrivateKeyString(size_t index);
  std::string GetPublicKeyString(size_t index);

  // Returns an uncompressed public key.
  EcdhPublicKey GetUncompressedPublicKey(size_t index);
  // Returns an uncompressed public key in the form of a string, with X.509
  // header.
  std::string GetUncompressedPublicKeyString(size_t index);

 private:
  std::vector<const char*> private_key_strings_;
  std::vector<const char*> public_key_strings_;
  std::vector<EcdhPrivateKey> private_keys_;
  std::vector<EcdhPublicKey> public_keys_;
  std::vector<EcdhPublicKey> uncompressed_public_keys_;
  std::vector<const char*> uncompressed_public_key_strings_;
};
}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_TESTING_ECDH_PREGENERATED_TEST_KEYS_H_
