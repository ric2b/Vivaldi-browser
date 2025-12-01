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
#ifndef FCP_SECAGG_SHARED_MAP_OF_MASKS_H_
#define FCP_SECAGG_SHARED_MAP_OF_MASKS_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "fcp/secagg/shared/aes_key.h"
#include "fcp/secagg/shared/aes_prng_factory.h"
#include "fcp/secagg/shared/async_abort.h"
#include "fcp/secagg/shared/compute_session_id.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_vector.h"

// This file contains two unbound functions for generating and adding maps of
// mask vectors.

namespace fcp {
namespace secagg {

// Generates and returns a map of masks for all the vectors that need to be
// masked, given all the keys that need to be used to mask (or unmask) those
// vectors.
//
// prng_factory is an instance of a subclass of AesPrngFactory.
// For clients communicating with the (C++) version of SecAggServer in this
// package, or the SecAggServer itself, this must be an instance of
// AesCtrPrngFactory.
//
// Returns a nullptr value if the operation was aborted, as detected via the
// optional async_abort parameter.
std::unique_ptr<SecAggVectorMap> MapOfMasks(
    const std::vector<AesKey>& prng_keys_to_add,
    const std::vector<AesKey>& prng_keys_to_subtract,
    const std::vector<InputVectorSpecification>& input_vector_specs,
    const SessionId& session_id, const AesPrngFactory& prng_factory,
    AsyncAbort* async_abort = nullptr);

// Optimized version of MapOfMasks that uses optimized AddModOpt and
// SubtractModOpt modulus operations.
std::unique_ptr<SecAggVectorMap> MapOfMasksV3(
    const std::vector<AesKey>& prng_keys_to_add,
    const std::vector<AesKey>& prng_keys_to_subtract,
    const std::vector<InputVectorSpecification>& input_vector_specs,
    const SessionId& session_id, const AesPrngFactory& prng_factory,
    AsyncAbort* async_abort = nullptr);

// Optimized version of MapOfMasks that uses optimized AddMapOpt and
// SubtractMapOpt modululs operations and produces map of unpacked vectors.
std::unique_ptr<SecAggUnpackedVectorMap> UnpackedMapOfMasks(
    const std::vector<AesKey>& prng_keys_to_add,
    const std::vector<AesKey>& prng_keys_to_subtract,
    const std::vector<InputVectorSpecification>& input_vector_specs,
    const SessionId& session_id, const AesPrngFactory& prng_factory,
    AsyncAbort* async_abort = nullptr);

// Adds two vectors together and returns a new sum vector.
SecAggVector AddVectors(const SecAggVector& a, const SecAggVector& b);

// Takes two maps of masks/masked vectors, and adds them together, returning the
// sum.
std::unique_ptr<SecAggVectorMap> AddMaps(const SecAggVectorMap& a,
                                         const SecAggVectorMap& b);

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_MAP_OF_MASKS_H_
