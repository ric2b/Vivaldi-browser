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

#ifndef FCP_SECAGG_SHARED_AES_CTR_PRNG_FACTORY_H_
#define FCP_SECAGG_SHARED_AES_CTR_PRNG_FACTORY_H_

#include <memory>

#include "fcp/secagg/shared/aes_key.h"
#include "fcp/secagg/shared/aes_prng_factory.h"
#include "fcp/secagg/shared/prng.h"

namespace fcp {
namespace secagg {

// Factory for the OpenSSL-based AesCtrPrng.
class AesCtrPrngFactory : public AesPrngFactory {
 public:
  AesCtrPrngFactory() = default;

  // Creates and returns an instance of AesCtrPrng, given an AES key.
  // For security reasons, the key MUST be suitable for immediate use in AES,
  // i.e. it must not be a shared ECDH secret that has not yet been hashed.
  std::unique_ptr<SecurePrng> MakePrng(const AesKey& key) const override;

  // TODO(team): Remove this when transition to the batch mode of
  // SecurePrng is fully done.
  bool SupportsBatchMode() const override { return true; }
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_AES_CTR_PRNG_FACTORY_H_
