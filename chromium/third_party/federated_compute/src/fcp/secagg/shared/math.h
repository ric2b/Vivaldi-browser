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

// This class contains some simple inline math methods commonly used elsewhere
// within SecAgg. No error checking or bounds checking is performed. The calling
// code is responsible for making sure the operations do not overflow, except as
// noted.

#ifndef FCP_SECAGG_SHARED_MATH_H_
#define FCP_SECAGG_SHARED_MATH_H_

#include <cstdint>
#include <string>

#include "absl/base/internal/endian.h"
#include "absl/numeric/int128.h"
#include "fcp/base/monitoring.h"

namespace fcp {
namespace secagg {

// Integer division rounded up.
static inline uint32_t DivideRoundUp(uint32_t a, uint32_t b) {
  return (a + b - 1) / b;
}

// Addition modulo non-zero integer z.
static inline uint64_t AddMod(uint64_t a, uint64_t b, uint64_t z) {
  return (a + b) % z;
}

// Optimized version of AddMod that assumes that a and b are smaller than mod.
// This version produces a code with branchless CMOVB instruction and is at
// least 2x faster than AddMod on x64.
// TODO(team): Eventually this should replace AddMod.
inline uint64_t AddModOpt(uint64_t a, uint64_t b, uint64_t mod) {
#ifndef NDEBUG
  // Verify assumption that a and b are smaller than mod to start with.
  FCP_CHECK(a < mod && b < mod);
  // Make sure there is no overflow when adding a and b.
  FCP_CHECK(a <= (a + b) && b <= (a + b));
#endif
  uint64_t sum = a + b;
  return sum < mod ? sum : sum - mod;
}

// Subtraction modulo non-zero integer z. Handles underflow correctly if b > a.
static inline uint64_t SubtractMod(uint64_t a, uint64_t b, uint64_t z) {
  return ((a - b) + z) % z;
}

// Optimized version of SubtractMod that assumes that a and b are smaller than
// mod.  This version produces a code with branchless CMOVB instruction and is
// at least 2x faster than SubtractMod on x64.
// TODO(team): Eventually this should replace SubtractMod.
inline uint64_t SubtractModOpt(uint64_t a, uint64_t b, uint64_t mod) {
#ifndef NDEBUG
  // Verify assumption that a and b are smaller than mod to start with.
  FCP_CHECK(a < mod && b < mod);
#endif
  return a >= b ? a - b : mod - b + a;
}

// Multiplication of 32-bit integers modulo a non-zero integer z.
// Guarantees the output is a 32-bit integer and avoids overflow by casting both
// factors to uint64_t first.
static inline uint32_t MultiplyMod(uint32_t a, uint32_t b, uint64_t z) {
  return static_cast<uint32_t>((uint64_t{a} * uint64_t{b}) % z);
}

// Multiplication of 64-bit integers modulo a non-zero integer z.
// Guarantees the output is a 64-bit integer and avoids overflow by casting both
// factors to uint128 first.
static inline uint64_t MultiplyMod64(uint64_t a, uint64_t b, uint64_t z) {
  return absl::Uint128Low64((absl::uint128(a) * absl::uint128(b)) %
                            absl::uint128(z));
}

// Modular inverse of a 64-bit integer modulo a prime z via Fermat's little
// theorem. Assumes that z is prime.
static inline uint64_t InverseModPrime(uint64_t a, uint64_t z) {
  uint64_t inverse = 1;
  uint64_t exponent = z - 2;

  while (exponent > 0) {
    if (exponent & 1) {
      inverse = MultiplyMod64(inverse, a, z);
    }

    exponent >>= 1;
    a = MultiplyMod64(a, a, z);
  }

  return inverse;
}

// Converts ints to big-endian byte string representation. Provides platform-
// independence only in converting known integer values to byte strings for use
// in cryptographic methods, not for general processing of binary data.
static inline std::string IntToByteString(uint32_t input) {
  char bytes[4];
  absl::big_endian::Store32(bytes, input);
  return std::string(bytes, 4);
}

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_MATH_H_
