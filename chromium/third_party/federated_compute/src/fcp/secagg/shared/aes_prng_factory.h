/*
 * Copyright 2019 Google LLC
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

#ifndef FCP_SECAGG_SHARED_AES_PRNG_FACTORY_H_
#define FCP_SECAGG_SHARED_AES_PRNG_FACTORY_H_

#include <memory>
#include "fcp/secagg/shared/aes_key.h"
#include "fcp/secagg/shared/prng.h"

namespace fcp {
namespace secagg {

// Factory interface for AES-based deterministic pseudorandom number generators.
class AesPrngFactory {
 public:
  virtual ~AesPrngFactory() = default;
  virtual std::unique_ptr<SecurePrng> MakePrng(const AesKey& key) const = 0;
  // TODO(team): Remove this when transition to the batch mode of
  // SecurePrng is fully done.
  // The batch mode allows to retrive a large batch of preuso-random numbers
  // in a single call.
  virtual bool SupportsBatchMode() const { return false; }
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_AES_PRNG_FACTORY_H_
