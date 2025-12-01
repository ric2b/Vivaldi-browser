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

#include "fcp/secagg/shared/crypto_rand_prng.h"

#include <cstdint>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/shared/prng.h"
#include "openssl/rand.h"

namespace fcp {
namespace secagg {

template <typename Output>
static Output Rand() {
  Output output;
  uint8_t bytes[sizeof(output)];
  FCP_CHECK(RAND_bytes(bytes, sizeof(output)));
  memcpy(&output, bytes, sizeof(output));
  return output;
}

uint8_t CryptoRandPrng::Rand8() { return Rand<uint8_t>(); }
uint64_t CryptoRandPrng::Rand64() { return Rand<uint64_t>(); }

}  // namespace secagg
}  // namespace fcp
