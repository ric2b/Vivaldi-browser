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

#ifndef FCP_SECAGG_SHARED_PRNG_H_
#define FCP_SECAGG_SHARED_PRNG_H_

#include <cstdint>
#include <vector>

namespace fcp {
namespace secagg {

// An interface for a secure pseudo-random number generator.
class SecurePrng {
 public:
  virtual uint8_t Rand8() = 0;
  virtual uint64_t Rand64() = 0;
  virtual ~SecurePrng() = default;
};

// Extension of SecurePrng interface that supports batch mode - getting multiple
// pseudo-random numbers in a single call.
class SecureBatchPrng : public SecurePrng {
 public:
  // Get the maximum size of a buffer that can be filled by RandBuffer() in a
  // single call.
  virtual size_t GetMaxBufferSize() const = 0;

  // Fills the provided buffer with pseudorandom bytes. Returns the number of
  // bytes that has been generated, which can be smaller than the requested
  // buffer_size if it exceeds the maximum buffer size.
  virtual int RandBuffer(uint8_t* buffer, int buffer_size) = 0;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_PRNG_H_
