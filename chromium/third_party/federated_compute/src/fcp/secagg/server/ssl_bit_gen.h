/*
 * Copyright 2020 Google LLC
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

#ifndef FCP_SECAGG_SERVER_SSL_BIT_GEN_H_
#define FCP_SECAGG_SERVER_SSL_BIT_GEN_H_

#include <cstdint>
#include <limits>

namespace fcp {
namespace secagg {

// A secure BitGen class (analogous to absl::BitGen) for use with absl random
// APIs, which uses RAND_bytes as a source of randomness. This type satisfies
// the UniformRandomBitGenerator (URBG) concept:
// https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator
//
// For generating a large quantity of random bytes (e.g. a cryptographic key),
// it is more appropriate to use RAND_bytes directly.
//
// Thread safety: SslBitGen is thread safe.
//
// SslBitGen construction is free, and instances don't need to be
// reused. In addition, it's probably better to make it clear at the call site
// when a SslBitGen is being used, as opposed to a different URBG. So
// rather than storing the SslBitGen, if possible, prefer to create one
// at the time it is needed:
//
//     int x = absl::Uniform(SslBitGen(), 0, 100);
//
class SslBitGen {
 public:
  using result_type = uint64_t;

  SslBitGen() = default;

  // SslBitGen cannot be copied or moved. This allows uses of it to easily be
  // replaced with a stateful UniformRandomBitGenerator.
  SslBitGen(const SslBitGen&) = delete;
  SslBitGen& operator=(const SslBitGen&) = delete;

  bool operator==(const SslBitGen&) const = delete;
  bool operator!=(const SslBitGen&) const = delete;

  // Returns a random number from a CSPRNG.
  result_type operator()();

  static constexpr result_type min() {
    return std::numeric_limits<result_type>::min();
  }
  static constexpr result_type max() {
    return std::numeric_limits<result_type>::max();
  }
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SSL_BIT_GEN_H_
