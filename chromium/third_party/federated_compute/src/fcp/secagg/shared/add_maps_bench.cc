/*
 * Copyright 2021 Google LLC
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
#include <memory>
#include <vector>

#include "fcp/secagg/shared/map_of_masks.h"
#include "fcp/secagg/shared/secagg_vector.h"

// Open-source version of benchmarking library
#include "benchmark//benchmark.h"

namespace fcp {
namespace secagg {
namespace {

// Open-source version of benchmarking library
using benchmark::internal::Benchmark;

// This function produces varied pairs of {bit_width, size} for the benchmark.
static void CustomArguments(Benchmark* b) {
  constexpr int bit_widths[] = {8, 25, 38, 53,
                                absl::bit_width(SecAggVector::kMaxModulus - 1)};
  for (int bit_width : bit_widths) {
    for (int size = 32; size <= 32 * 1024 * 1024; size *= 32) {
      b->ArgPair(bit_width, size);
    }
  }
}

std::unique_ptr<SecAggVectorMap> MakeMap(int64_t bit_width, int64_t size,
                                         uint64_t start, uint64_t step) {
  std::vector<uint64_t> vec;
  vec.resize(size);

  uint64_t modulus = 1ULL << bit_width;
  uint64_t v = start;
  for (int64_t i = 0; i < size; i++) {
    vec[i] = v;
    v = (v + step) % modulus;
  }

  auto map = std::make_unique<SecAggVectorMap>();
  map->emplace("test", SecAggVector(vec, modulus));
  return map;
}

void BM_AddMaps(benchmark::State& state) {
  auto map_a = MakeMap(state.range(0), state.range(1), 1, 1);
  auto map_b = MakeMap(state.range(0), state.range(1), 2, 3);
  for (auto _ : state) {
    auto map_sum = AddMaps(*map_a, *map_b);
    benchmark::DoNotOptimize(map_sum);
  }
}

BENCHMARK(BM_AddMaps)->Apply(CustomArguments);

}  // namespace
}  // namespace secagg
}  // namespace fcp
