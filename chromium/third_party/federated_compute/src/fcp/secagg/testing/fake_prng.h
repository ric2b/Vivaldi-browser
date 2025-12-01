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

#ifndef FCP_SECAGG_TESTING_FAKE_PRNG_H_
#define FCP_SECAGG_TESTING_FAKE_PRNG_H_

#include <cstdint>

#include "fcp/secagg/shared/prng.h"

namespace fcp {
namespace secagg {

// Fake Implementation of SecurePrng that just returns constantly incrementing
// values.

class FakePrng : public SecurePrng {
 public:
  // Returns 1, 2, 3, etc.
  FakePrng() = default;

  // Returns the selected value first, and increments by 1 each time from there.
  explicit FakePrng(uint64_t value) : value_(value - 1) {}

  uint8_t Rand8() override { return static_cast<uint8_t>(++value_); }
  uint64_t Rand64() override { return ++value_; }

 private:
  uint64_t value_ = 0;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_TESTING_FAKE_PRNG_H_
