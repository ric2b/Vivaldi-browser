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

#ifndef FCP_SECAGG_SHARED_ECDH_KEYS_H_
#define FCP_SECAGG_SHARED_ECDH_KEYS_H_

#include "fcp/secagg/shared/key.h"

// This file contains definitions for ECDH public key and private key types.

namespace fcp {
namespace secagg {
// A Key that serves as a private key for use with ECDH, with the NIST P-256
// curve. Works the same as Key, but is guaranteed to have either 0 or 32 bytes.
// A 0-byte key should not be used for anything, and represents the absence of
// a key in a collection of keys.
class EcdhPrivateKey : public Key {
 public:
  static constexpr int kSize = 32;

  // The key is blank.
  EcdhPrivateKey() : Key() {}

  // The data MUST have 32 bytes.
  explicit EcdhPrivateKey(const uint8_t* data) : Key(data, kSize) {}
};

// A Key that serves as a public key for use with ECDH, with the NIST P-256
// curve. Works the same as Key, but is guaranteed to have either 0, 33, or 65
// bytes (depending on whether the key is compressed or not). Clients and the
// server should both produce compressed keys, but legacy Java clients send
// keys in uncompressed format.
// A 0-byte key should not be used for anything, and represents the absence of
// a key in a collection of keys.
class EcdhPublicKey : public Key {
 public:
  static constexpr int kSize = 33;
  // TODO(team): Remove uncompressed support when Java SecAgg deprecated.
  static constexpr int kUncompressedSize = 65;
  enum Format { kCompressed, kUncompressed };

  // The key is blank.
  EcdhPublicKey() : Key() {}

  // If the key is compressed, data must have 33 bytes.
  // If the key is uncompressed, data must have 65 bytes and the uncompressed
  // format must be specified.
  explicit EcdhPublicKey(const uint8_t* data, Format format = kCompressed)
      : Key(data, format == kCompressed ? kSize : kUncompressedSize) {}
};
}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_ECDH_KEYS_H_
