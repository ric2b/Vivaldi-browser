// Copyright (c) 2017 Facebook Inc.
// Copyright (c) 2015-2017 Georgia Institute of Technology
// All rights reserved.
//
// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <cstddef>
#include <cstdint>
#include <thread>  // NOLINT

#include <benchmark/benchmark.h>
#include <pthreadpool.h>

static void SetNumberOfThreads(benchmark::internal::Benchmark* benchmark) {
  const int max_threads = std::thread::hardware_concurrency();
  for (int t = 1; t <= max_threads; t++) {
    benchmark->Arg(t);
  }
}

static void compute_1d(void*, size_t x) {}

static void pthreadpool_parallelize_1d(benchmark::State& state) {
  const uint32_t threads = static_cast<uint32_t>(state.range(0));
  pthreadpool_t threadpool = pthreadpool_create(threads);
  while (state.KeepRunning()) {
    pthreadpool_parallelize_1d(threadpool, compute_1d, nullptr /* context */,
                               threads, /*flags=*/0);
  }
  pthreadpool_destroy(threadpool);
}

BENCHMARK(pthreadpool_parallelize_1d)->UseRealTime()->Apply(SetNumberOfThreads);

static void compute_1d_tile_1d(void*, size_t, size_t) {}

static void pthreadpool_parallelize_1d_tile_1d(benchmark::State& state) {
  const uint32_t threads = static_cast<uint32_t>(state.range(0));
  pthreadpool_t threadpool = pthreadpool_create(threads);
  while (state.KeepRunning()) {
    pthreadpool_parallelize_1d_tile_1d(threadpool, compute_1d_tile_1d,
                                       nullptr /* context */, threads, 1,
                                       /*flags=*/0);
  }
  pthreadpool_destroy(threadpool);
}

BENCHMARK(pthreadpool_parallelize_1d_tile_1d)
    ->UseRealTime()
    ->Apply(SetNumberOfThreads);

static void compute_1d_tile_1d_dynamic(void*, size_t, size_t) {}

static void pthreadpool_parallelize_1d_tile_1d_dynamic(
    benchmark::State& state) {
  const uint32_t threads = static_cast<uint32_t>(state.range(0));
  pthreadpool_t threadpool = pthreadpool_create(threads);
  while (state.KeepRunning()) {
    pthreadpool_parallelize_1d_tile_1d_dynamic(
        threadpool, compute_1d_tile_1d_dynamic, nullptr /* context */, threads,
        1, /*flags=*/0);
  }
  pthreadpool_destroy(threadpool);
}

BENCHMARK(pthreadpool_parallelize_1d_tile_1d_dynamic)
    ->UseRealTime()
    ->Apply(SetNumberOfThreads);

static void compute_2d(void*, size_t, size_t) {}

static void pthreadpool_parallelize_2d(benchmark::State& state) {
  const uint32_t threads = static_cast<uint32_t>(state.range(0));
  pthreadpool_t threadpool = pthreadpool_create(threads);
  while (state.KeepRunning()) {
    pthreadpool_parallelize_2d(threadpool, compute_2d, nullptr /* context */, 1,
                               threads, /*flags=*/0);
  }
  pthreadpool_destroy(threadpool);
}

BENCHMARK(pthreadpool_parallelize_2d)->UseRealTime()->Apply(SetNumberOfThreads);

static void compute_2d_tile_2d(void*, size_t, size_t, size_t, size_t) {}

static void pthreadpool_parallelize_2d_tile_2d(benchmark::State& state) {
  const uint32_t threads = static_cast<uint32_t>(state.range(0));
  pthreadpool_t threadpool = pthreadpool_create(threads);
  while (state.KeepRunning()) {
    pthreadpool_parallelize_2d_tile_2d(threadpool, compute_2d_tile_2d,
                                       nullptr /* context */, 1, threads, 1, 1,
                                       /*flags=*/0);
  }
  pthreadpool_destroy(threadpool);
}

BENCHMARK(pthreadpool_parallelize_2d_tile_2d)
    ->UseRealTime()
    ->Apply(SetNumberOfThreads);

static void compute_2d_tile_2d_dynamic(void*, size_t, size_t, size_t, size_t) {}

static void pthreadpool_parallelize_2d_tile_2d_dynamic(
    benchmark::State& state) {
  const uint32_t threads = static_cast<uint32_t>(state.range(0));
  pthreadpool_t threadpool = pthreadpool_create(threads);
  while (state.KeepRunning()) {
    pthreadpool_parallelize_2d_tile_2d_dynamic(
        threadpool, compute_2d_tile_2d_dynamic, nullptr /* context */, 1,
        threads, 1, 1, /*flags=*/0);
  }
  pthreadpool_destroy(threadpool);
}

BENCHMARK(pthreadpool_parallelize_2d_tile_2d_dynamic)
    ->UseRealTime()
    ->Apply(SetNumberOfThreads);

static void compute_3d_tile_2d_dynamic(void*, size_t, size_t, size_t, size_t,
                                       size_t) {}

static void pthreadpool_parallelize_3d_tile_2d_dynamic(
    benchmark::State& state) {
  const uint32_t threads = static_cast<uint32_t>(state.range(0));
  pthreadpool_t threadpool = pthreadpool_create(threads);
  while (state.KeepRunning()) {
    pthreadpool_parallelize_3d_tile_2d_dynamic(
        threadpool, compute_3d_tile_2d_dynamic, nullptr /* context */, 1, 1,
        threads, 1, 1, /*flags=*/0);
  }
  pthreadpool_destroy(threadpool);
}

BENCHMARK(pthreadpool_parallelize_3d_tile_2d_dynamic)
    ->UseRealTime()
    ->Apply(SetNumberOfThreads);

BENCHMARK_MAIN();
