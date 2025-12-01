/*
 * Copyright 2020 Google LLC
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

#include <cstdint>
#include <vector>

#include "absl/numeric/bits.h"
#include "absl/strings/str_cat.h"
#include "benchmark//benchmark.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/map_of_masks.h"
#include "fcp/secagg/shared/secagg_vector.h"

namespace fcp {
namespace secagg {
namespace {

constexpr auto kVectorSize = 1024 * 1024;
constexpr auto kNumKeys = 128;

inline void BM_MapOfMasks_Impl(benchmark::State& state, uint64_t modulus) {
  state.PauseTiming();
  std::vector<AesKey> prng_keys_to_add;
  uint8_t key[AesKey::kSize];
  memset(key, 'A', AesKey::kSize);
  prng_keys_to_add.reserve(kNumKeys);
  for (int i = 0; i < kNumKeys; i++) {
    prng_keys_to_add.emplace_back(key);
  }
  std::vector<AesKey> prng_keys_to_subtract;
  SessionId session_id = {std::string(32, 'Z')};

  std::vector<InputVectorSpecification> vector_specs;
  vector_specs.emplace_back("unused", kVectorSize, modulus);

  state.ResumeTiming();
  benchmark::DoNotOptimize(MapOfMasks(
      prng_keys_to_add, prng_keys_to_subtract, vector_specs, session_id,
      static_cast<const AesPrngFactory&>(AesCtrPrngFactory())));

  state.SetItemsProcessed(kVectorSize);
}

inline void BM_MapOfMasksV3_Impl(benchmark::State& state, uint64_t modulus) {
  state.PauseTiming();
  std::vector<AesKey> prng_keys_to_add;
  uint8_t key[AesKey::kSize];
  memset(key, 'A', AesKey::kSize);
  prng_keys_to_add.reserve(kNumKeys);
  for (int i = 0; i < kNumKeys; i++) {
    prng_keys_to_add.emplace_back(key);
  }
  std::vector<AesKey> prng_keys_to_subtract;
  SessionId session_id = {std::string(32, 'Z')};

  std::vector<InputVectorSpecification> vector_specs;
  vector_specs.emplace_back("unused", kVectorSize, modulus);

  state.ResumeTiming();
  benchmark::DoNotOptimize(MapOfMasksV3(
      prng_keys_to_add, prng_keys_to_subtract, vector_specs, session_id,
      static_cast<const AesPrngFactory&>(AesCtrPrngFactory())));

  state.SetItemsProcessed(kVectorSize);
}

void BM_MapOfMasks_PowerOfTwo(benchmark::State& state) {
  for (auto s : state) {
    int bitwidth = static_cast<int>(state.range(0));
    BM_MapOfMasks_Impl(state, 1ULL << bitwidth);
  }
}

void BM_MapOfMasks_Arbitrary(benchmark::State& state) {
  for (auto s : state) {
    uint64_t modulus = static_cast<uint64_t>(state.range(0));
    BM_MapOfMasks_Impl(state, modulus);
  }
}

void BM_MapOfMasksV3_PowerOfTwo(benchmark::State& state) {
  for (auto s : state) {
    int bitwidth = static_cast<int>(state.range(0));
    BM_MapOfMasksV3_Impl(state, 1ULL << bitwidth);
  }
}

void BM_MapOfMasksV3_Arbitrary(benchmark::State& state) {
  for (auto s : state) {
    uint64_t modulus = static_cast<uint64_t>(state.range(0));
    BM_MapOfMasksV3_Impl(state, modulus);
  }
}

BENCHMARK(BM_MapOfMasks_PowerOfTwo)
    ->Arg(9)
    ->Arg(25)
    ->Arg(41)
    ->Arg(53)
    ->Arg(absl::bit_width(SecAggVector::kMaxModulus - 1));

BENCHMARK(BM_MapOfMasks_Arbitrary)
    ->Arg(5)
    ->Arg(39)
    ->Arg(485)
    ->Arg(2400)
    ->Arg(14901)
    ->Arg(51813)
    ->Arg(532021)
    ->Arg(13916946)
    ->Arg(39549497)
    ->Arg(548811945)
    ->Arg(590549014)
    ->Arg(48296031686)
    ->Arg(156712951284)
    ->Arg(2636861836189)
    ->Arg(14673852658160)
    ->Arg(92971495438615)
    ->Arg(304436005557271)
    ->Arg(14046234330484262)
    ->Arg(38067457113486645)
    ->Arg(175631339105057682);

BENCHMARK(BM_MapOfMasksV3_PowerOfTwo)
    ->Arg(9)
    ->Arg(25)
    ->Arg(41)
    ->Arg(53)
    ->Arg(absl::bit_width(SecAggVector::kMaxModulus - 1));

BENCHMARK(BM_MapOfMasksV3_Arbitrary)
    ->Arg(5)
    ->Arg(39)
    ->Arg(485)
    ->Arg(2400)
    ->Arg(14901)
    ->Arg(51813)
    ->Arg(532021)
    ->Arg(13916946)
    ->Arg(39549497)
    ->Arg(548811945)
    ->Arg(590549014)
    ->Arg(48296031686)
    ->Arg(156712951284)
    ->Arg(2636861836189)
    ->Arg(14673852658160)
    ->Arg(92971495438615)
    ->Arg(304436005557271)
    ->Arg(14046234330484262)
    ->Arg(38067457113486645)
    ->Arg(175631339105057682);

}  // namespace
}  // namespace secagg
}  // namespace fcp
