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

#ifndef FCP_SECAGG_SHARED_CRYPTO_RAND_PRNG_H_
#define FCP_SECAGG_SHARED_CRYPTO_RAND_PRNG_H_

#include <cstdint>

#include "fcp/secagg/shared/prng.h"

namespace fcp {
namespace secagg {

// A cryptographically strong Pseudorandom Number Generator based on OpenSSL,
// which seeds using /dev/urandom on UNIX-like operating systems, and other
// sources of randomness on Windows.
//
// This class is thread-safe.

class CryptoRandPrng : public SecurePrng {
 public:
  CryptoRandPrng() = default;

  uint8_t Rand8() override;
  uint64_t Rand64() override;
};

}  // namespace secagg
}  // namespace fcp
#endif  // FCP_SECAGG_SHARED_CRYPTO_RAND_PRNG_H_
