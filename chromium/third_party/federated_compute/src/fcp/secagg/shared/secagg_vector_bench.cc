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

#include "benchmark//benchmark.h"
#include "fcp/secagg/shared/secagg_vector.h"

namespace fcp {
namespace secagg {
namespace {

constexpr auto kVectorSize = 32 * 1024 * 1024;

static void BM_CreatePowerOfTwo(benchmark::State& state) {
  auto items_processed = 0;
  std::vector<uint64_t> input;
  input.resize(kVectorSize);
  for (auto s : state) {
    uint64_t modulus = 1ULL << static_cast<int>(state.range(1));
    SecAggVector vec(input, modulus, state.range(0));
    benchmark::DoNotOptimize(vec.GetAsUint64Vector());
    items_processed += vec.num_elements();
  }
  state.SetItemsProcessed(items_processed);
}

static void BM_CreateArbitrary(benchmark::State& state) {
  auto items_processed = 0;
  std::vector<uint64_t> input;
  input.resize(kVectorSize);
  for (auto s : state) {
    uint64_t modulus = static_cast<uint64_t>(state.range(1));
    SecAggVector vec(input, modulus, state.range(0));
    benchmark::DoNotOptimize(vec.GetAsUint64Vector());
    items_processed += vec.num_elements();
  }
  state.SetItemsProcessed(items_processed);
}

BENCHMARK(BM_CreatePowerOfTwo)
    ->RangeMultiplier(2)
    ->Ranges({{false, true},
              {1, absl::bit_width(SecAggVector::kMaxModulus - 1ULL)}});

BENCHMARK(BM_CreatePowerOfTwo)->Args({false, 41})->Args({true, 41});

BENCHMARK(BM_CreateArbitrary)
    ->Args({false, 5})
    ->Args({false, 39})
    ->Args({false, 485})
    ->Args({false, 2400})
    ->Args({false, 14901})
    ->Args({false, 51813})
    ->Args({false, 532021})
    ->Args({false, 13916946})
    ->Args({false, 39549497})
    ->Args({false, 548811945})
    ->Args({false, 590549014})
    ->Args({false, 48296031686})
    ->Args({false, 156712951284})
    ->Args({false, 2636861836189})
    ->Args({false, 14673852658160})
    ->Args({false, 92971495438615})
    ->Args({false, 304436005557271})
    ->Args({false, 14046234330484262})
    ->Args({false, 38067457113486645})
    ->Args({false, 175631339105057682})
    ->Args({true, 5})
    ->Args({true, 39})
    ->Args({true, 485})
    ->Args({true, 2400})
    ->Args({true, 14901})
    ->Args({true, 51813})
    ->Args({true, 532021})
    ->Args({true, 13916946})
    ->Args({true, 39549497})
    ->Args({true, 548811945})
    ->Args({true, 590549014})
    ->Args({true, 48296031686})
    ->Args({true, 156712951284})
    ->Args({true, 2636861836189})
    ->Args({true, 14673852658160})
    ->Args({true, 92971495438615})
    ->Args({true, 304436005557271})
    ->Args({true, 14046234330484262})
    ->Args({true, 38067457113486645})
    ->Args({true, 175631339105057682});

}  // namespace
}  // namespace secagg
}  // namespace fcp
