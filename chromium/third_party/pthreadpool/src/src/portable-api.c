// Copyright (c) 2017 Facebook Inc.
// Copyright (c) 2015-2017 Georgia Institute of Technology
// All rights reserved.
//
// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

/* Standard C headers */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if PTHREADPOOL_USE_CPUINFO
#include <cpuinfo.h>
#endif

/* Dependencies */
#include <fxdiv.h>

/* Public library header */
#include <pthreadpool.h>

/* Internal library headers */
#include "threadpool-atomics.h"
#include "threadpool-common.h"
#include "threadpool-object.h"
#include "threadpool-utils.h"

#define PTHREADPOOL_DEFAULT_FASTEST_TO_SLOWEST_RATIO 2
#define PTHREADPOOL_MAX_FASTEST_TO_SLOWEST_RATIO 4

static size_t get_fastest_to_slowest_ratio() {
#if PTHREADPOOL_USE_CPUINFO
  // If we are not the fastest core, assume that we are at most 4x slower
  // than the fastest core.
  return cpuinfo_get_current_uarch_index_with_default(0) > 0
             ? PTHREADPOOL_MAX_FASTEST_TO_SLOWEST_RATIO
             : PTHREADPOOL_DEFAULT_FASTEST_TO_SLOWEST_RATIO;
#else
  return PTHREADPOOL_DEFAULT_FASTEST_TO_SLOWEST_RATIO;
#endif  // PTHREADPOOL_USE_CPUINFO
}

static size_t get_chunk(pthreadpool_atomic_size_t* num_tiles,
                        size_t fastest_to_slowest_ratio) {
  /* Check whether there are any tiles left. */
  size_t curr_num_tiles = pthreadpool_load_relaxed_size_t(num_tiles);
  if (*(ptrdiff_t*)&curr_num_tiles <= 0) {
    return 0;
  }

  /* Choose a chunk size based on the global remaining amount of work and the
   * current number of threads. */
  size_t chunk_size = max(curr_num_tiles / fastest_to_slowest_ratio, 1);
  curr_num_tiles =
      pthreadpool_fetch_decrement_n_relaxed_size_t(num_tiles, chunk_size);
  if (*(ptrdiff_t*)&curr_num_tiles <= 0) {
    return 0;
  }
  return min(chunk_size, curr_num_tiles);
}

size_t PTHREADPOOL_IMPL(pthreadpool_get_threads_count)(
    struct pthreadpool* threadpool) {
  if (threadpool == NULL) {
    return 1;
  }

  return threadpool->threads_count.value;
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_get_threads_count)

static void thread_parallelize_1d(struct pthreadpool* threadpool,
                                  struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_1d_t task =
      (pthreadpool_task_1d_t)pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  size_t range_start = pthreadpool_load_relaxed_size_t(&thread->range_start);
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, range_start++);
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      task(argument, index);
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_1d_with_thread(struct pthreadpool* threadpool,
                                              struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_1d_with_thread_t task =
      (pthreadpool_task_1d_with_thread_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  const size_t thread_number = thread->thread_number;
  /* Process thread's own range of items */
  size_t range_start = pthreadpool_load_relaxed_size_t(&thread->range_start);
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, thread_number, range_start++);
  }

  /* There still may be other threads with work */
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      task(argument, thread_number, index);
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_1d_with_uarch(struct pthreadpool* threadpool,
                                             struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_1d_with_id_t task =
      (pthreadpool_task_1d_with_id_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  const uint32_t default_uarch_index =
      threadpool->params.parallelize_1d_with_uarch.default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index >
      threadpool->params.parallelize_1d_with_uarch.max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  /* Process thread's own range of items */
  size_t range_start = pthreadpool_load_relaxed_size_t(&thread->range_start);
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, uarch_index, range_start++);
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      task(argument, uarch_index, index);
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_1d_tile_1d(struct pthreadpool* threadpool,
                                          struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_1d_tile_1d_t task =
      (pthreadpool_task_1d_tile_1d_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const size_t tile = threadpool->params.parallelize_1d_tile_1d.tile;
  size_t tile_start = range_start * tile;

  const size_t range = threadpool->params.parallelize_1d_tile_1d.range;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, tile_start, min(range - tile_start, tile));
    tile_start += tile;
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t tile_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const size_t tile_start = tile_index * tile;
      task(argument, tile_start, min(range - tile_start, tile));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_1d_tile_1d_dynamic(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get a handle on the params.
  struct pthreadpool_1d_tile_1d_dynamic_params* params =
      &threadpool->params.parallelize_1d_tile_1d_dynamic;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_i = params->range;
  const size_t tile_i = params->tile;
  const pthreadpool_task_1d_tile_1d_dynamic_t task =
      (pthreadpool_task_1d_tile_1d_dynamic_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      /* Call the task function. */
      const size_t index_i = offset * tile_i;
      const size_t step_i = min(tile_i * chunk_size, range_i - index_i);
      task(argument, index_i, step_i);
      offset += chunk_size;
    }
  }

  /* Make changes by this thread visible to other threads. */
  pthreadpool_fence_release();
}

static void thread_parallelize_1d_tile_1d_dynamic_with_thread(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get a handle on the params.
  struct pthreadpool_1d_tile_1d_dynamic_params* params =
      &threadpool->params.parallelize_1d_tile_1d_dynamic;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_i = params->range;
  const size_t tile_i = params->tile;
  const pthreadpool_task_1d_tile_1d_dynamic_with_id_t task =
      (pthreadpool_task_1d_tile_1d_dynamic_with_id_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      /* Call the task function. */
      const size_t index_i = offset * tile_i;
      const size_t step_i = min(tile_i * chunk_size, range_i - index_i);
      task(argument, thread_number, index_i, step_i);
      offset += chunk_size;
    }
  }

  /* Make changes by this thread visible to other threads. */
  pthreadpool_fence_release();
}

static void thread_parallelize_1d_tile_1d_dynamic_with_uarch_with_thread(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get a handle on the params.
  struct pthreadpool_1d_tile_1d_dynamic_with_uarch_params* params =
      &threadpool->params.parallelize_1d_tile_1d_dynamic_with_uarch;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_i = params->range;
  const size_t tile_i = params->tile;
  const pthreadpool_task_1d_tile_1d_dynamic_with_id_with_thread_t task =
      (pthreadpool_task_1d_tile_1d_dynamic_with_id_with_thread_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  const uint32_t default_uarch_index = params->default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index > params->max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      /* Call the task function. */
      const size_t index_i = offset * tile_i;
      const size_t step_i = min(tile_i * chunk_size, range_i - index_i);
      task(argument, uarch_index, thread_number, index_i, step_i);
      offset += chunk_size;
    }
  }

  /* Make changes by this thread visible to other threads. */
  pthreadpool_fence_release();
}

static void thread_parallelize_2d(struct pthreadpool* threadpool,
                                  struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_2d_t task =
      (pthreadpool_task_2d_t)pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_2d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(range_start, range_j);
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;

  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, j);
    if (++j == range_j.value) {
      j = 0;
      i += 1;
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(linear_index, range_j);
      task(argument, index_i_j.quotient, index_i_j.remainder);
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_2d_with_thread(struct pthreadpool* threadpool,
                                              struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_2d_with_thread_t task =
      (pthreadpool_task_2d_with_thread_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_2d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(range_start, range_j);
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;

  const size_t thread_number = thread->thread_number;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, thread_number, i, j);
    if (++j == range_j.value) {
      j = 0;
      i += 1;
    }
  }

  /* There still may be other threads with work */
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(linear_index, range_j);
      task(argument, thread_number, index_i_j.quotient, index_i_j.remainder);
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_2d_tile_1d(struct pthreadpool* threadpool,
                                          struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_2d_tile_1d_t task =
      (pthreadpool_task_2d_tile_1d_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_j =
      threadpool->params.parallelize_2d_tile_1d.tile_range_j;
  const struct fxdiv_result_size_t tile_index_i_j =
      fxdiv_divide_size_t(range_start, tile_range_j);
  const size_t tile_j = threadpool->params.parallelize_2d_tile_1d.tile_j;
  size_t i = tile_index_i_j.quotient;
  size_t start_j = tile_index_i_j.remainder * tile_j;

  const size_t range_j = threadpool->params.parallelize_2d_tile_1d.range_j;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, start_j, min(range_j - start_j, tile_j));
    start_j += tile_j;
    if (start_j >= range_j) {
      start_j = 0;
      i += 1;
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_i_j =
          fxdiv_divide_size_t(linear_index, tile_range_j);
      const size_t start_j = tile_index_i_j.remainder * tile_j;
      task(argument, tile_index_i_j.quotient, start_j,
           min(range_j - start_j, tile_j));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_2d_tile_1d_with_uarch(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_2d_tile_1d_with_id_t task =
      (pthreadpool_task_2d_tile_1d_with_id_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  const uint32_t default_uarch_index =
      threadpool->params.parallelize_2d_tile_1d_with_uarch.default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index >
      threadpool->params.parallelize_2d_tile_1d_with_uarch.max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_j =
      threadpool->params.parallelize_2d_tile_1d_with_uarch.tile_range_j;
  const struct fxdiv_result_size_t tile_index_i_j =
      fxdiv_divide_size_t(range_start, tile_range_j);
  const size_t tile_j =
      threadpool->params.parallelize_2d_tile_1d_with_uarch.tile_j;
  size_t i = tile_index_i_j.quotient;
  size_t start_j = tile_index_i_j.remainder * tile_j;

  const size_t range_j =
      threadpool->params.parallelize_2d_tile_1d_with_uarch.range_j;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, uarch_index, i, start_j, min(range_j - start_j, tile_j));
    start_j += tile_j;
    if (start_j >= range_j) {
      start_j = 0;
      i += 1;
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_i_j =
          fxdiv_divide_size_t(linear_index, tile_range_j);
      const size_t start_j = tile_index_i_j.remainder * tile_j;
      task(argument, uarch_index, tile_index_i_j.quotient, start_j,
           min(range_j - start_j, tile_j));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_2d_tile_1d_with_uarch_with_thread(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_2d_tile_1d_with_id_with_thread_t task =
      (pthreadpool_task_2d_tile_1d_with_id_with_thread_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  const uint32_t default_uarch_index =
      threadpool->params.parallelize_2d_tile_1d_with_uarch.default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index >
      threadpool->params.parallelize_2d_tile_1d_with_uarch.max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_j =
      threadpool->params.parallelize_2d_tile_1d_with_uarch.tile_range_j;
  const struct fxdiv_result_size_t tile_index_i_j =
      fxdiv_divide_size_t(range_start, tile_range_j);
  const size_t tile_j =
      threadpool->params.parallelize_2d_tile_1d_with_uarch.tile_j;
  size_t i = tile_index_i_j.quotient;
  size_t start_j = tile_index_i_j.remainder * tile_j;

  const size_t thread_number = thread->thread_number;
  const size_t range_j =
      threadpool->params.parallelize_2d_tile_1d_with_uarch.range_j;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, uarch_index, thread_number, i, start_j,
         min(range_j - start_j, tile_j));
    start_j += tile_j;
    if (start_j >= range_j) {
      start_j = 0;
      i += 1;
    }
  }

  /* There still may be other threads with work */
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_i_j =
          fxdiv_divide_size_t(linear_index, tile_range_j);
      const size_t start_j = tile_index_i_j.remainder * tile_j;
      task(argument, uarch_index, thread_number, tile_index_i_j.quotient,
           start_j, min(range_j - start_j, tile_j));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_2d_tile_1d_dynamic(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get a handle on the params.
  struct pthreadpool_2d_tile_1d_dynamic_params* params =
      &threadpool->params.parallelize_2d_tile_1d_dynamic;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_j = params->range_j;
  const size_t tile_j = params->tile_j;
  const size_t tile_range_j = divide_round_up(range_j, tile_j);
  const pthreadpool_task_2d_tile_1d_dynamic_t task =
      (pthreadpool_task_2d_tile_1d_dynamic_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      // Call the task function.
      size_t index_i = offset / tile_range_j;
      size_t tile_index_j = offset % tile_range_j;
      while (chunk_size > 0) {
        const size_t index_j = tile_index_j * tile_j;
        const size_t tile_step_j = min(chunk_size, tile_range_j - tile_index_j);
        const size_t step_j = min(tile_step_j * tile_j, range_j - index_j);

        task(argument, index_i, index_j, step_j);

        tile_index_j += tile_step_j;
        if (tile_range_j <= tile_index_j) {
          tile_index_j -= tile_range_j;
          index_i += 1;
        }
        chunk_size -= tile_step_j;
        offset += tile_step_j;
      }
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_2d_tile_1d_dynamic_with_thread(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get a handle on the params.
  struct pthreadpool_2d_tile_1d_dynamic_params* params =
      &threadpool->params.parallelize_2d_tile_1d_dynamic;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_j = params->range_j;
  const size_t tile_j = params->tile_j;
  const size_t tile_range_j = divide_round_up(range_j, tile_j);
  const pthreadpool_task_2d_tile_1d_dynamic_with_id_t task =
      (pthreadpool_task_2d_tile_1d_dynamic_with_id_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      // Call the task function.
      size_t index_i = offset / tile_range_j;
      size_t tile_index_j = offset % tile_range_j;
      while (chunk_size > 0) {
        const size_t index_j = tile_index_j * tile_j;
        const size_t tile_step_j = min(chunk_size, tile_range_j - tile_index_j);
        const size_t step_j = min(tile_step_j * tile_j, range_j - index_j);

        task(argument, thread_number, index_i, index_j, step_j);

        tile_index_j += tile_step_j;
        if (tile_range_j <= tile_index_j) {
          tile_index_j -= tile_range_j;
          index_i += 1;
        }
        chunk_size -= tile_step_j;
        offset += tile_step_j;
      }
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_2d_tile_1d_dynamic_with_uarch_with_thread(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get a handle on the params.
  struct pthreadpool_2d_tile_1d_dynamic_with_uarch_params* params =
      &threadpool->params.parallelize_2d_tile_1d_dynamic_with_uarch;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_j = params->range_j;
  const size_t tile_j = params->tile_j;
  const size_t tile_range_j = divide_round_up(range_j, tile_j);
  const pthreadpool_task_2d_tile_1d_dynamic_with_id_with_thread_t task =
      (pthreadpool_task_2d_tile_1d_dynamic_with_id_with_thread_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  const uint32_t default_uarch_index = params->default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index > params->max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      // Call the task function.
      size_t index_i = offset / tile_range_j;
      size_t tile_index_j = offset % tile_range_j;
      while (chunk_size > 0) {
        const size_t index_j = tile_index_j * tile_j;
        const size_t tile_step_j = min(chunk_size, tile_range_j - tile_index_j);
        const size_t step_j = min(tile_step_j * tile_j, range_j - index_j);

        task(argument, uarch_index, thread_number, index_i, index_j, step_j);

        tile_index_j += tile_step_j;
        if (tile_range_j <= tile_index_j) {
          tile_index_j -= tile_range_j;
          index_i += 1;
        }
        chunk_size -= tile_step_j;
        offset += tile_step_j;
      }
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_2d_tile_2d(struct pthreadpool* threadpool,
                                          struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_2d_tile_2d_t task =
      (pthreadpool_task_2d_tile_2d_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_j =
      threadpool->params.parallelize_2d_tile_2d.tile_range_j;
  const struct fxdiv_result_size_t tile_index_i_j =
      fxdiv_divide_size_t(range_start, tile_range_j);
  const size_t tile_i = threadpool->params.parallelize_2d_tile_2d.tile_i;
  const size_t tile_j = threadpool->params.parallelize_2d_tile_2d.tile_j;
  size_t start_i = tile_index_i_j.quotient * tile_i;
  size_t start_j = tile_index_i_j.remainder * tile_j;

  const size_t range_i = threadpool->params.parallelize_2d_tile_2d.range_i;
  const size_t range_j = threadpool->params.parallelize_2d_tile_2d.range_j;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, start_i, start_j, min(range_i - start_i, tile_i),
         min(range_j - start_j, tile_j));
    start_j += tile_j;
    if (start_j >= range_j) {
      start_j = 0;
      start_i += tile_i;
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_i_j =
          fxdiv_divide_size_t(linear_index, tile_range_j);
      const size_t start_i = tile_index_i_j.quotient * tile_i;
      const size_t start_j = tile_index_i_j.remainder * tile_j;
      task(argument, start_i, start_j, min(range_i - start_i, tile_i),
           min(range_j - start_j, tile_j));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_2d_tile_2d_with_uarch(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_2d_tile_2d_with_id_t task =
      (pthreadpool_task_2d_tile_2d_with_id_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  const uint32_t default_uarch_index =
      threadpool->params.parallelize_2d_tile_2d_with_uarch.default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index >
      threadpool->params.parallelize_2d_tile_2d_with_uarch.max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  /* Process thread's own range of items */
  const struct fxdiv_divisor_size_t tile_range_j =
      threadpool->params.parallelize_2d_tile_2d_with_uarch.tile_range_j;
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_result_size_t index =
      fxdiv_divide_size_t(range_start, tile_range_j);
  const size_t range_i =
      threadpool->params.parallelize_2d_tile_2d_with_uarch.range_i;
  const size_t tile_i =
      threadpool->params.parallelize_2d_tile_2d_with_uarch.tile_i;
  const size_t range_j =
      threadpool->params.parallelize_2d_tile_2d_with_uarch.range_j;
  const size_t tile_j =
      threadpool->params.parallelize_2d_tile_2d_with_uarch.tile_j;
  size_t start_i = index.quotient * tile_i;
  size_t start_j = index.remainder * tile_j;

  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, uarch_index, start_i, start_j,
         min(range_i - start_i, tile_i), min(range_j - start_j, tile_j));
    start_j += tile_j;
    if (start_j >= range_j) {
      start_j = 0;
      start_i += tile_i;
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_i_j =
          fxdiv_divide_size_t(linear_index, tile_range_j);
      const size_t start_i = tile_index_i_j.quotient * tile_i;
      const size_t start_j = tile_index_i_j.remainder * tile_j;
      task(argument, uarch_index, start_i, start_j,
           min(range_i - start_i, tile_i), min(range_j - start_j, tile_j));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_2d_tile_2d_dynamic(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get a handle on the params.
  struct pthreadpool_2d_tile_2d_dynamic_params* params =
      &threadpool->params.parallelize_2d_tile_2d_dynamic;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_i = params->range_i;
  const size_t range_j = params->range_j;
  const size_t tile_i = params->tile_i;
  const size_t tile_j = params->tile_j;
  const size_t tile_range_i = divide_round_up(range_i, tile_i);
  const size_t tile_range_j = divide_round_up(range_j, tile_j);
  const pthreadpool_task_2d_tile_2d_dynamic_t task =
      (pthreadpool_task_2d_tile_2d_dynamic_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      /* Iterate over the chunk and call the task function. */
      size_t tile_index_i = offset / tile_range_j;
      if (tile_range_j == 1) {
        /* If there is only a single tile in the `j`th (last) dimension, then we
         * group by the `j`th (second-last) dimeension. */
        const size_t index_i = tile_index_i * tile_i;
        const size_t tile_step_i = min(tile_range_i - tile_index_i, chunk_size);
        const size_t step_i = min(tile_step_i * tile_i, range_i - index_i);

        task(argument, index_i, /*index_j=*/0, step_i, range_j);

        offset += tile_step_i;
      } else {
        size_t tile_index_j = offset % tile_range_j;
        while (chunk_size > 0) {
          const size_t index_i = tile_index_i * tile_i;
          const size_t index_j = tile_index_j * tile_j;
          const size_t step_i = min(tile_i, range_i - index_i);
          const size_t tile_step_j =
              min(tile_range_j - tile_index_j, chunk_size);
          const size_t step_j = min(tile_step_j * tile_j, range_j - index_j);

          task(argument, index_i, index_j, step_i, step_j);

          tile_index_j += tile_step_j;
          if (tile_range_j <= tile_index_j) {
            tile_index_j -= tile_range_j;
            tile_index_i += 1;
          }
          chunk_size -= tile_step_j;
          offset += tile_step_j;
        }
      }
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_2d_tile_2d_dynamic_with_uarch(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Identify the uarch for the current core.
  const uint32_t default_uarch_index =
      threadpool->params.parallelize_2d_tile_2d_dynamic_with_uarch
          .default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index > threadpool->params.parallelize_2d_tile_2d_dynamic_with_uarch
                        .max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  // Get a handle on the params.
  struct pthreadpool_2d_tile_2d_dynamic_with_uarch_params* params =
      &threadpool->params.parallelize_2d_tile_2d_dynamic_with_uarch;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_i = params->range_i;
  const size_t range_j = params->range_j;
  const size_t tile_i = params->tile_i;
  const size_t tile_j = params->tile_j;
  const size_t tile_range_i = divide_round_up(range_i, tile_i);
  const size_t tile_range_j = divide_round_up(range_j, tile_j);
  const pthreadpool_task_2d_tile_2d_dynamic_with_id_t task =
      (pthreadpool_task_2d_tile_2d_dynamic_with_id_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      /* Iterate over the chunk and call the task function. */
      size_t tile_index_i = offset / tile_range_j;
      if (tile_range_j == 1) {
        /* If there is only a single tile in the `j`th (last) dimension, then we
         * group by the `j`th (second-last) dimeension. */
        const size_t index_i = tile_index_i * tile_i;
        const size_t tile_step_i = min(tile_range_i - tile_index_i, chunk_size);
        const size_t step_i = min(tile_step_i * tile_i, range_i - index_i);

        task(argument, uarch_index, index_i, /*index_j=*/0, step_i, range_j);

        offset += tile_step_i;
      } else {
        size_t tile_index_j = offset % tile_range_j;
        while (chunk_size > 0) {
          const size_t index_i = tile_index_i * tile_i;
          const size_t index_j = tile_index_j * tile_j;
          const size_t step_i = min(tile_i, range_i - index_i);
          const size_t tile_step_j =
              min(tile_range_j - tile_index_j, chunk_size);
          const size_t step_j = min(tile_step_j * tile_j, range_j - index_j);

          task(argument, uarch_index, index_i, index_j, step_i, step_j);

          tile_index_j += tile_step_j;
          if (tile_range_j <= tile_index_j) {
            tile_index_j -= tile_range_j;
            tile_index_i += 1;
          }
          chunk_size -= tile_step_j;
          offset += tile_step_j;
        }
      }
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_2d_tile_2d_dynamic_with_thread(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get a handle on the params.
  struct pthreadpool_2d_tile_2d_dynamic_params* params =
      &threadpool->params.parallelize_2d_tile_2d_dynamic;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_i = params->range_i;
  const size_t range_j = params->range_j;
  const size_t tile_i = params->tile_i;
  const size_t tile_j = params->tile_j;
  const size_t tile_range_i = divide_round_up(range_i, tile_i);
  const size_t tile_range_j = divide_round_up(range_j, tile_j);
  const pthreadpool_task_2d_tile_2d_dynamic_with_id_t task =
      (pthreadpool_task_2d_tile_2d_dynamic_with_id_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      /* Iterate over the chunk and call the task function. */
      size_t tile_index_i = offset / tile_range_j;
      if (tile_range_j == 1) {
        /* If there is only a single tile in the `j`th (last) dimension, then we
         * group by the `j`th (second-last) dimeension. */
        const size_t index_i = tile_index_i * tile_i;
        const size_t tile_step_i = min(tile_range_i - tile_index_i, chunk_size);
        const size_t step_i = min(tile_step_i * tile_i, range_i - index_i);

        task(argument, thread_number, index_i, /*index_j=*/0, step_i, range_j);

        offset += tile_step_i;
      } else {
        size_t tile_index_j = offset % tile_range_j;
        while (chunk_size > 0) {
          const size_t index_i = tile_index_i * tile_i;
          const size_t index_j = tile_index_j * tile_j;
          const size_t step_i = min(tile_i, range_i - index_i);
          const size_t tile_step_j =
              min(tile_range_j - tile_index_j, chunk_size);
          const size_t step_j = min(tile_step_j * tile_j, range_j - index_j);

          task(argument, thread_number, index_i, index_j, step_i, step_j);

          tile_index_j += tile_step_j;
          if (tile_range_j <= tile_index_j) {
            tile_index_j -= tile_range_j;
            tile_index_i += 1;
          }
          chunk_size -= tile_step_j;
          offset += tile_step_j;
        }
      }
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_3d(struct pthreadpool* threadpool,
                                  struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_3d_t task =
      (pthreadpool_task_3d_t)pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t range_k =
      threadpool->params.parallelize_3d.range_k;
  const struct fxdiv_result_size_t index_ij_k =
      fxdiv_divide_size_t(range_start, range_k);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_3d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(index_ij_k.quotient, range_j);
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t k = index_ij_k.remainder;

  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, j, k);
    if (++k == range_k.value) {
      k = 0;
      if (++j == range_j.value) {
        j = 0;
        i += 1;
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t index_ij_k =
          fxdiv_divide_size_t(linear_index, range_k);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(index_ij_k.quotient, range_j);
      task(argument, index_i_j.quotient, index_i_j.remainder,
           index_ij_k.remainder);
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_3d_tile_1d(struct pthreadpool* threadpool,
                                          struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_3d_tile_1d_t task =
      (pthreadpool_task_3d_tile_1d_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_k =
      threadpool->params.parallelize_3d_tile_1d.tile_range_k;
  const struct fxdiv_result_size_t tile_index_ij_k =
      fxdiv_divide_size_t(range_start, tile_range_k);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_3d_tile_1d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(tile_index_ij_k.quotient, range_j);
  const size_t tile_k = threadpool->params.parallelize_3d_tile_1d.tile_k;
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t start_k = tile_index_ij_k.remainder * tile_k;

  const size_t range_k = threadpool->params.parallelize_3d_tile_1d.range_k;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, j, start_k, min(range_k - start_k, tile_k));
    start_k += tile_k;
    if (start_k >= range_k) {
      start_k = 0;
      if (++j == range_j.value) {
        j = 0;
        i += 1;
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_ij_k =
          fxdiv_divide_size_t(linear_index, tile_range_k);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(tile_index_ij_k.quotient, range_j);
      const size_t start_k = tile_index_ij_k.remainder * tile_k;
      task(argument, index_i_j.quotient, index_i_j.remainder, start_k,
           min(range_k - start_k, tile_k));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_3d_tile_1d_with_thread(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_3d_tile_1d_with_thread_t task =
      (pthreadpool_task_3d_tile_1d_with_thread_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_k =
      threadpool->params.parallelize_3d_tile_1d.tile_range_k;
  const struct fxdiv_result_size_t tile_index_ij_k =
      fxdiv_divide_size_t(range_start, tile_range_k);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_3d_tile_1d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(tile_index_ij_k.quotient, range_j);
  const size_t tile_k = threadpool->params.parallelize_3d_tile_1d.tile_k;
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t start_k = tile_index_ij_k.remainder * tile_k;

  const size_t thread_number = thread->thread_number;
  const size_t range_k = threadpool->params.parallelize_3d_tile_1d.range_k;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, thread_number, i, j, start_k,
         min(range_k - start_k, tile_k));
    start_k += tile_k;
    if (start_k >= range_k) {
      start_k = 0;
      if (++j == range_j.value) {
        j = 0;
        i += 1;
      }
    }
  }

  /* There still may be other threads with work */
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_ij_k =
          fxdiv_divide_size_t(linear_index, tile_range_k);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(tile_index_ij_k.quotient, range_j);
      const size_t start_k = tile_index_ij_k.remainder * tile_k;
      task(argument, thread_number, index_i_j.quotient, index_i_j.remainder,
           start_k, min(range_k - start_k, tile_k));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_3d_tile_1d_with_uarch(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_3d_tile_1d_with_id_t task =
      (pthreadpool_task_3d_tile_1d_with_id_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  const uint32_t default_uarch_index =
      threadpool->params.parallelize_3d_tile_1d_with_uarch.default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index >
      threadpool->params.parallelize_3d_tile_1d_with_uarch.max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_k =
      threadpool->params.parallelize_3d_tile_1d_with_uarch.tile_range_k;
  const struct fxdiv_result_size_t tile_index_ij_k =
      fxdiv_divide_size_t(range_start, tile_range_k);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_3d_tile_1d_with_uarch.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(tile_index_ij_k.quotient, range_j);
  const size_t tile_k =
      threadpool->params.parallelize_3d_tile_1d_with_uarch.tile_k;
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t start_k = tile_index_ij_k.remainder * tile_k;

  const size_t range_k =
      threadpool->params.parallelize_3d_tile_1d_with_uarch.range_k;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, uarch_index, i, j, start_k, min(range_k - start_k, tile_k));
    start_k += tile_k;
    if (start_k >= range_k) {
      start_k = 0;
      if (++j == range_j.value) {
        j = 0;
        i += 1;
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_ij_k =
          fxdiv_divide_size_t(linear_index, tile_range_k);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(tile_index_ij_k.quotient, range_j);
      const size_t start_k = tile_index_ij_k.remainder * tile_k;
      task(argument, uarch_index, index_i_j.quotient, index_i_j.remainder,
           start_k, min(range_k - start_k, tile_k));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_3d_tile_1d_with_uarch_with_thread(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_3d_tile_1d_with_id_with_thread_t task =
      (pthreadpool_task_3d_tile_1d_with_id_with_thread_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  const uint32_t default_uarch_index =
      threadpool->params.parallelize_3d_tile_1d_with_uarch.default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index >
      threadpool->params.parallelize_3d_tile_1d_with_uarch.max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_k =
      threadpool->params.parallelize_3d_tile_1d_with_uarch.tile_range_k;
  const struct fxdiv_result_size_t tile_index_ij_k =
      fxdiv_divide_size_t(range_start, tile_range_k);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_3d_tile_1d_with_uarch.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(tile_index_ij_k.quotient, range_j);
  const size_t tile_k =
      threadpool->params.parallelize_3d_tile_1d_with_uarch.tile_k;
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t start_k = tile_index_ij_k.remainder * tile_k;

  const size_t thread_number = thread->thread_number;
  const size_t range_k =
      threadpool->params.parallelize_3d_tile_1d_with_uarch.range_k;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, uarch_index, thread_number, i, j, start_k,
         min(range_k - start_k, tile_k));
    start_k += tile_k;
    if (start_k >= range_k) {
      start_k = 0;
      if (++j == range_j.value) {
        j = 0;
        i += 1;
      }
    }
  }

  /* There still may be other threads with work */
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_ij_k =
          fxdiv_divide_size_t(linear_index, tile_range_k);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(tile_index_ij_k.quotient, range_j);
      const size_t start_k = tile_index_ij_k.remainder * tile_k;
      task(argument, uarch_index, thread_number, index_i_j.quotient,
           index_i_j.remainder, start_k, min(range_k - start_k, tile_k));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_3d_tile_1d_dynamic_with_thread(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get a handle on the params.
  struct pthreadpool_3d_tile_1d_dynamic_params* params =
      &threadpool->params.parallelize_3d_tile_1d_dynamic;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_j = params->range_j;
  const size_t range_k = params->range_k;
  const size_t tile_k = params->tile_k;
  const size_t tile_range_k = divide_round_up(range_k, tile_k);
  const pthreadpool_task_3d_tile_1d_dynamic_with_id_t task =
      (pthreadpool_task_3d_tile_1d_dynamic_with_id_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      /* Iterate over the chunk and call the task function. */
      size_t index_i = offset / (range_j * tile_range_k);
      size_t index_j = (offset / tile_range_k) % range_j;
      size_t tile_index_k = offset % tile_range_k;
      while (chunk_size > 0) {
        const size_t index_k = tile_index_k * tile_k;
        const size_t tile_step_k = min(tile_range_k - tile_index_k, chunk_size);
        const size_t step_k = min(tile_step_k * tile_k, range_k - index_k);

        task(argument, thread_number, index_i, index_j, index_k, step_k);

        tile_index_k += tile_step_k;
        if (tile_range_k <= tile_index_k) {
          tile_index_k -= tile_range_k;
          if (range_j <= ++index_j) {
            index_j = 0;
            index_i += 1;
          }
        }
        chunk_size -= tile_step_k;
        offset += tile_step_k;
      }
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_3d_tile_1d_dynamic_with_uarch_with_thread(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get a handle on the params.
  struct pthreadpool_3d_tile_1d_dynamic_with_uarch_params* params =
      &threadpool->params.parallelize_3d_tile_1d_dynamic_with_uarch;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_j = params->range_j;
  const size_t range_k = params->range_k;
  const size_t tile_k = params->tile_k;
  const size_t tile_range_k = divide_round_up(range_k, tile_k);
  const pthreadpool_task_3d_tile_1d_dynamic_with_id_with_thread_t task =
      (pthreadpool_task_3d_tile_1d_dynamic_with_id_with_thread_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Get the uarch_index.
  const uint32_t default_uarch_index =
      threadpool->params.parallelize_3d_tile_1d_with_uarch.default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index >
      threadpool->params.parallelize_3d_tile_1d_with_uarch.max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      /* Iterate over the chunk and call the task function. */
      size_t index_i = offset / (range_j * tile_range_k);
      size_t index_j = (offset / tile_range_k) % range_j;
      size_t tile_index_k = offset % tile_range_k;
      while (chunk_size > 0) {
        const size_t index_k = tile_index_k * tile_k;
        const size_t tile_step_k = min(tile_range_k - tile_index_k, chunk_size);
        const size_t step_k = min(tile_step_k * tile_k, range_k - index_k);

        task(argument, uarch_index, thread_number, index_i, index_j, index_k,
             step_k);

        tile_index_k += tile_step_k;
        if (tile_range_k <= tile_index_k) {
          tile_index_k -= tile_range_k;
          if (range_j <= ++index_j) {
            index_j = 0;
            index_i += 1;
          }
        }
        chunk_size -= tile_step_k;
        offset += tile_step_k;
      }
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_3d_tile_2d(struct pthreadpool* threadpool,
                                          struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_3d_tile_2d_t task =
      (pthreadpool_task_3d_tile_2d_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_k =
      threadpool->params.parallelize_3d_tile_2d.tile_range_k;
  const struct fxdiv_result_size_t tile_index_ij_k =
      fxdiv_divide_size_t(range_start, tile_range_k);
  const struct fxdiv_divisor_size_t tile_range_j =
      threadpool->params.parallelize_3d_tile_2d.tile_range_j;
  const struct fxdiv_result_size_t tile_index_i_j =
      fxdiv_divide_size_t(tile_index_ij_k.quotient, tile_range_j);
  const size_t tile_j = threadpool->params.parallelize_3d_tile_2d.tile_j;
  const size_t tile_k = threadpool->params.parallelize_3d_tile_2d.tile_k;
  size_t i = tile_index_i_j.quotient;
  size_t start_j = tile_index_i_j.remainder * tile_j;
  size_t start_k = tile_index_ij_k.remainder * tile_k;

  const size_t range_k = threadpool->params.parallelize_3d_tile_2d.range_k;
  const size_t range_j = threadpool->params.parallelize_3d_tile_2d.range_j;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, start_j, start_k, min(range_j - start_j, tile_j),
         min(range_k - start_k, tile_k));
    start_k += tile_k;
    if (start_k >= range_k) {
      start_k = 0;
      start_j += tile_j;
      if (start_j >= range_j) {
        start_j = 0;
        i += 1;
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_ij_k =
          fxdiv_divide_size_t(linear_index, tile_range_k);
      const struct fxdiv_result_size_t tile_index_i_j =
          fxdiv_divide_size_t(tile_index_ij_k.quotient, tile_range_j);
      const size_t start_j = tile_index_i_j.remainder * tile_j;
      const size_t start_k = tile_index_ij_k.remainder * tile_k;
      task(argument, tile_index_i_j.quotient, start_j, start_k,
           min(range_j - start_j, tile_j), min(range_k - start_k, tile_k));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_3d_tile_2d_with_uarch(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_3d_tile_2d_with_id_t task =
      (pthreadpool_task_3d_tile_2d_with_id_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  const uint32_t default_uarch_index =
      threadpool->params.parallelize_3d_tile_2d_with_uarch.default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index >
      threadpool->params.parallelize_3d_tile_2d_with_uarch.max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_k =
      threadpool->params.parallelize_3d_tile_2d_with_uarch.tile_range_k;
  const struct fxdiv_result_size_t tile_index_ij_k =
      fxdiv_divide_size_t(range_start, tile_range_k);
  const struct fxdiv_divisor_size_t tile_range_j =
      threadpool->params.parallelize_3d_tile_2d_with_uarch.tile_range_j;
  const struct fxdiv_result_size_t tile_index_i_j =
      fxdiv_divide_size_t(tile_index_ij_k.quotient, tile_range_j);
  const size_t tile_j =
      threadpool->params.parallelize_3d_tile_2d_with_uarch.tile_j;
  const size_t tile_k =
      threadpool->params.parallelize_3d_tile_2d_with_uarch.tile_k;
  size_t i = tile_index_i_j.quotient;
  size_t start_j = tile_index_i_j.remainder * tile_j;
  size_t start_k = tile_index_ij_k.remainder * tile_k;

  const size_t range_k =
      threadpool->params.parallelize_3d_tile_2d_with_uarch.range_k;
  const size_t range_j =
      threadpool->params.parallelize_3d_tile_2d_with_uarch.range_j;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, uarch_index, i, start_j, start_k,
         min(range_j - start_j, tile_j), min(range_k - start_k, tile_k));
    start_k += tile_k;
    if (start_k >= range_k) {
      start_k = 0;
      start_j += tile_j;
      if (start_j >= range_j) {
        start_j = 0;
        i += 1;
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_ij_k =
          fxdiv_divide_size_t(linear_index, tile_range_k);
      const struct fxdiv_result_size_t tile_index_i_j =
          fxdiv_divide_size_t(tile_index_ij_k.quotient, tile_range_j);
      const size_t start_j = tile_index_i_j.remainder * tile_j;
      const size_t start_k = tile_index_ij_k.remainder * tile_k;
      task(argument, uarch_index, tile_index_i_j.quotient, start_j, start_k,
           min(range_j - start_j, tile_j), min(range_k - start_k, tile_k));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_3d_tile_2d_dynamic(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get a handle on the params.
  struct pthreadpool_3d_tile_2d_dynamic_params* params =
      &threadpool->params.parallelize_3d_tile_2d_dynamic;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_j = params->range_j;
  const size_t range_k = params->range_k;
  const size_t tile_j = params->tile_j;
  const size_t tile_k = params->tile_k;
  const size_t tile_range_j = divide_round_up(range_j, tile_j);
  const size_t tile_range_k = divide_round_up(range_k, tile_k);
  const pthreadpool_task_3d_tile_2d_dynamic_t task =
      (pthreadpool_task_3d_tile_2d_dynamic_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      /* Iterate over the chunk and call the task function. */
      size_t index_i = offset / (tile_range_j * tile_range_k);
      size_t tile_index_j = (offset / tile_range_k) % tile_range_j;
      if (tile_range_k == 1) {
        /* If there is only a single tile in the `k`th (last) dimension, then we
         * group by the `j`th (second-last) dimeension. */
        while (chunk_size > 0) {
          const size_t index_j = tile_index_j * tile_j;
          const size_t tile_step_j =
              min(tile_range_j - tile_index_j, chunk_size);
          const size_t step_j = min(tile_step_j * tile_j, range_j - index_j);

          task(argument, index_i, index_j, /*index_k=*/0, step_j, range_k);

          tile_index_j += tile_step_j;
          if (tile_range_j <= tile_index_j) {
            tile_index_j -= tile_range_j;
            index_i += 1;
          }
          chunk_size -= tile_step_j;
          offset += tile_step_j;
        }
      } else {
        size_t tile_index_k = offset % tile_range_k;
        while (chunk_size > 0) {
          const size_t index_j = tile_index_j * tile_j;
          const size_t index_k = tile_index_k * tile_k;
          const size_t step_j = min(tile_j, range_j - index_j);
          const size_t tile_step_k =
              min(tile_range_k - tile_index_k, chunk_size);
          const size_t step_k = min(tile_step_k * tile_k, range_k - index_k);

          task(argument, index_i, index_j, index_k, step_j, step_k);

          tile_index_k += tile_step_k;
          if (tile_range_k <= tile_index_k) {
            tile_index_k -= tile_range_k;
            if (tile_range_j <= ++tile_index_j) {
              tile_index_j = 0;
              index_i += 1;
            }
          }
          chunk_size -= tile_step_k;
          offset += tile_step_k;
        }
      }
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_3d_tile_2d_dynamic_with_uarch(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get the uarch.
  const uint32_t default_uarch_index =
      threadpool->params.parallelize_3d_tile_2d_dynamic_with_uarch
          .default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index > threadpool->params.parallelize_3d_tile_2d_dynamic_with_uarch
                        .max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  // Get a handle on the params.
  struct pthreadpool_3d_tile_2d_dynamic_with_uarch_params* params =
      &threadpool->params.parallelize_3d_tile_2d_dynamic_with_uarch;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_j = params->range_j;
  const size_t range_k = params->range_k;
  const size_t tile_j = params->tile_j;
  const size_t tile_k = params->tile_k;
  const size_t tile_range_j = divide_round_up(range_j, tile_j);
  const size_t tile_range_k = divide_round_up(range_k, tile_k);
  const pthreadpool_task_3d_tile_2d_dynamic_with_id_t task =
      (pthreadpool_task_3d_tile_2d_dynamic_with_id_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      /* Iterate over the chunk and call the task function. */
      size_t index_i = offset / (tile_range_j * tile_range_k);
      size_t tile_index_j = (offset / tile_range_k) % tile_range_j;
      if (tile_range_k == 1) {
        /* If there is only a single tile in the `k`th (last) dimension, then we
         * group by the `j`th (second-last) dimeension. */
        while (chunk_size > 0) {
          const size_t index_j = tile_index_j * tile_j;
          const size_t tile_step_j =
              min(tile_range_j - tile_index_j, chunk_size);
          const size_t step_j = min(tile_step_j * tile_j, range_j - index_j);

          task(argument, uarch_index, index_i, index_j, /*index_k=*/0, step_j,
               range_k);

          tile_index_j += tile_step_j;
          if (tile_range_j <= tile_index_j) {
            tile_index_j -= tile_range_j;
            index_i += 1;
          }
          chunk_size -= tile_step_j;
          offset += tile_step_j;
        }
      } else {
        size_t tile_index_k = offset % tile_range_k;
        while (chunk_size > 0) {
          const size_t index_j = tile_index_j * tile_j;
          const size_t index_k = tile_index_k * tile_k;
          const size_t step_j = min(tile_j, range_j - index_j);
          const size_t tile_step_k =
              min(tile_range_k - tile_index_k, chunk_size);
          const size_t step_k = min(tile_step_k * tile_k, range_k - index_k);

          task(argument, uarch_index, index_i, index_j, index_k, step_j,
               step_k);

          tile_index_k += tile_step_k;
          if (tile_range_k <= tile_index_k) {
            tile_index_k -= tile_range_k;
            if (tile_range_j <= ++tile_index_j) {
              tile_index_j = 0;
              index_i += 1;
            }
          }
          chunk_size -= tile_step_k;
          offset += tile_step_k;
        }
      }
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_3d_tile_2d_dynamic_with_thread(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get a handle on the params.
  struct pthreadpool_3d_tile_2d_dynamic_params* params =
      &threadpool->params.parallelize_3d_tile_2d_dynamic;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_j = params->range_j;
  const size_t range_k = params->range_k;
  const size_t tile_j = params->tile_j;
  const size_t tile_k = params->tile_k;
  const size_t tile_range_j = divide_round_up(range_j, tile_j);
  const size_t tile_range_k = divide_round_up(range_k, tile_k);
  const pthreadpool_task_3d_tile_2d_dynamic_with_id_t task =
      (pthreadpool_task_3d_tile_2d_dynamic_with_id_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      /* Iterate over the chunk and call the task function. */
      size_t index_i = offset / (tile_range_j * tile_range_k);
      size_t tile_index_j = (offset / tile_range_k) % tile_range_j;
      if (tile_range_k == 1) {
        /* If there is only a single tile in the `k`th (last) dimension, then we
         * group by the `j`th (second-last) dimeension. */
        while (chunk_size > 0) {
          const size_t index_j = tile_index_j * tile_j;
          const size_t tile_step_j =
              min(tile_range_j - tile_index_j, chunk_size);
          const size_t step_j = min(tile_step_j * tile_j, range_j - index_j);

          task(argument, thread_number, index_i, index_j, /*index_k=*/0, step_j,
               range_k);

          tile_index_j += tile_step_j;
          if (tile_range_j <= tile_index_j) {
            tile_index_j -= tile_range_j;
            index_i += 1;
          }
          chunk_size -= tile_step_j;
          offset += tile_step_j;
        }
      } else {
        size_t tile_index_k = offset % tile_range_k;
        while (chunk_size > 0) {
          const size_t index_j = tile_index_j * tile_j;
          const size_t index_k = tile_index_k * tile_k;
          const size_t step_j = min(tile_j, range_j - index_j);
          const size_t tile_step_k =
              min(tile_range_k - tile_index_k, chunk_size);
          const size_t step_k = min(tile_step_k * tile_k, range_k - index_k);

          task(argument, thread_number, index_i, index_j, index_k, step_j,
               step_k);

          tile_index_k += tile_step_k;
          if (tile_range_k <= tile_index_k) {
            tile_index_k -= tile_range_k;
            if (tile_range_j <= ++tile_index_j) {
              tile_index_j = 0;
              index_i += 1;
            }
          }
          chunk_size -= tile_step_k;
          offset += tile_step_k;
        }
      }
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_4d(struct pthreadpool* threadpool,
                                  struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_4d_t task =
      (pthreadpool_task_4d_t)pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t range_kl =
      threadpool->params.parallelize_4d.range_kl;
  const struct fxdiv_result_size_t index_ij_kl =
      fxdiv_divide_size_t(range_start, range_kl);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_4d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(index_ij_kl.quotient, range_j);
  const struct fxdiv_divisor_size_t range_l =
      threadpool->params.parallelize_4d.range_l;
  const struct fxdiv_result_size_t index_k_l =
      fxdiv_divide_size_t(index_ij_kl.remainder, range_l);
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t k = index_k_l.quotient;
  size_t l = index_k_l.remainder;

  const size_t range_k = threadpool->params.parallelize_4d.range_k;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, j, k, l);
    if (++l == range_l.value) {
      l = 0;
      if (++k == range_k) {
        k = 0;
        if (++j == range_j.value) {
          j = 0;
          i += 1;
        }
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t index_ij_kl =
          fxdiv_divide_size_t(linear_index, range_kl);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(index_ij_kl.quotient, range_j);
      const struct fxdiv_result_size_t index_k_l =
          fxdiv_divide_size_t(index_ij_kl.remainder, range_l);
      task(argument, index_i_j.quotient, index_i_j.remainder,
           index_k_l.quotient, index_k_l.remainder);
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_4d_tile_1d(struct pthreadpool* threadpool,
                                          struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_4d_tile_1d_t task =
      (pthreadpool_task_4d_tile_1d_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_kl =
      threadpool->params.parallelize_4d_tile_1d.tile_range_kl;
  const struct fxdiv_result_size_t tile_index_ij_kl =
      fxdiv_divide_size_t(range_start, tile_range_kl);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_4d_tile_1d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(tile_index_ij_kl.quotient, range_j);
  const struct fxdiv_divisor_size_t tile_range_l =
      threadpool->params.parallelize_4d_tile_1d.tile_range_l;
  const struct fxdiv_result_size_t tile_index_k_l =
      fxdiv_divide_size_t(tile_index_ij_kl.remainder, tile_range_l);
  const size_t tile_l = threadpool->params.parallelize_4d_tile_1d.tile_l;
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t k = tile_index_k_l.quotient;
  size_t start_l = tile_index_k_l.remainder * tile_l;

  const size_t range_k = threadpool->params.parallelize_4d_tile_1d.range_k;
  const size_t range_l = threadpool->params.parallelize_4d_tile_1d.range_l;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, j, k, start_l, min(range_l - start_l, tile_l));
    start_l += tile_l;
    if (start_l >= range_l) {
      start_l = 0;
      if (++k == range_k) {
        k = 0;
        if (++j == range_j.value) {
          j = 0;
          i += 1;
        }
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_ij_kl =
          fxdiv_divide_size_t(linear_index, tile_range_kl);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(tile_index_ij_kl.quotient, range_j);
      const struct fxdiv_result_size_t tile_index_k_l =
          fxdiv_divide_size_t(tile_index_ij_kl.remainder, tile_range_l);
      const size_t start_l = tile_index_k_l.remainder * tile_l;
      task(argument, index_i_j.quotient, index_i_j.remainder,
           tile_index_k_l.quotient, start_l, min(range_l - start_l, tile_l));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_4d_tile_2d(struct pthreadpool* threadpool,
                                          struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_4d_tile_2d_t task =
      (pthreadpool_task_4d_tile_2d_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_kl =
      threadpool->params.parallelize_4d_tile_2d.tile_range_kl;
  const struct fxdiv_result_size_t tile_index_ij_kl =
      fxdiv_divide_size_t(range_start, tile_range_kl);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_4d_tile_2d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(tile_index_ij_kl.quotient, range_j);
  const struct fxdiv_divisor_size_t tile_range_l =
      threadpool->params.parallelize_4d_tile_2d.tile_range_l;
  const struct fxdiv_result_size_t tile_index_k_l =
      fxdiv_divide_size_t(tile_index_ij_kl.remainder, tile_range_l);
  const size_t tile_k = threadpool->params.parallelize_4d_tile_2d.tile_k;
  const size_t tile_l = threadpool->params.parallelize_4d_tile_2d.tile_l;
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t start_k = tile_index_k_l.quotient * tile_k;
  size_t start_l = tile_index_k_l.remainder * tile_l;

  const size_t range_l = threadpool->params.parallelize_4d_tile_2d.range_l;
  const size_t range_k = threadpool->params.parallelize_4d_tile_2d.range_k;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, j, start_k, start_l, min(range_k - start_k, tile_k),
         min(range_l - start_l, tile_l));
    start_l += tile_l;
    if (start_l >= range_l) {
      start_l = 0;
      start_k += tile_k;
      if (start_k >= range_k) {
        start_k = 0;
        if (++j == range_j.value) {
          j = 0;
          i += 1;
        }
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_ij_kl =
          fxdiv_divide_size_t(linear_index, tile_range_kl);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(tile_index_ij_kl.quotient, range_j);
      const struct fxdiv_result_size_t tile_index_k_l =
          fxdiv_divide_size_t(tile_index_ij_kl.remainder, tile_range_l);
      const size_t start_k = tile_index_k_l.quotient * tile_k;
      const size_t start_l = tile_index_k_l.remainder * tile_l;
      task(argument, index_i_j.quotient, index_i_j.remainder, start_k, start_l,
           min(range_k - start_k, tile_k), min(range_l - start_l, tile_l));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_4d_tile_2d_with_uarch(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_4d_tile_2d_with_id_t task =
      (pthreadpool_task_4d_tile_2d_with_id_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  const uint32_t default_uarch_index =
      threadpool->params.parallelize_4d_tile_2d_with_uarch.default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index >
      threadpool->params.parallelize_4d_tile_2d_with_uarch.max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_kl =
      threadpool->params.parallelize_4d_tile_2d_with_uarch.tile_range_kl;
  const struct fxdiv_result_size_t tile_index_ij_kl =
      fxdiv_divide_size_t(range_start, tile_range_kl);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_4d_tile_2d_with_uarch.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(tile_index_ij_kl.quotient, range_j);
  const struct fxdiv_divisor_size_t tile_range_l =
      threadpool->params.parallelize_4d_tile_2d_with_uarch.tile_range_l;
  const struct fxdiv_result_size_t tile_index_k_l =
      fxdiv_divide_size_t(tile_index_ij_kl.remainder, tile_range_l);
  const size_t tile_k =
      threadpool->params.parallelize_4d_tile_2d_with_uarch.tile_k;
  const size_t tile_l =
      threadpool->params.parallelize_4d_tile_2d_with_uarch.tile_l;
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t start_k = tile_index_k_l.quotient * tile_k;
  size_t start_l = tile_index_k_l.remainder * tile_l;

  const size_t range_l =
      threadpool->params.parallelize_4d_tile_2d_with_uarch.range_l;
  const size_t range_k =
      threadpool->params.parallelize_4d_tile_2d_with_uarch.range_k;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, uarch_index, i, j, start_k, start_l,
         min(range_k - start_k, tile_k), min(range_l - start_l, tile_l));
    start_l += tile_l;
    if (start_l >= range_l) {
      start_l = 0;
      start_k += tile_k;
      if (start_k >= range_k) {
        start_k = 0;
        if (++j == range_j.value) {
          j = 0;
          i += 1;
        }
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_ij_kl =
          fxdiv_divide_size_t(linear_index, tile_range_kl);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(tile_index_ij_kl.quotient, range_j);
      const struct fxdiv_result_size_t tile_index_k_l =
          fxdiv_divide_size_t(tile_index_ij_kl.remainder, tile_range_l);
      const size_t start_k = tile_index_k_l.quotient * tile_k;
      const size_t start_l = tile_index_k_l.remainder * tile_l;
      task(argument, uarch_index, index_i_j.quotient, index_i_j.remainder,
           start_k, start_l, min(range_k - start_k, tile_k),
           min(range_l - start_l, tile_l));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_4d_tile_2d_dynamic(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get a handle on the params.
  struct pthreadpool_4d_tile_2d_dynamic_params* params =
      &threadpool->params.parallelize_4d_tile_2d_dynamic;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_j = params->range_j;
  const size_t range_k = params->range_k;
  const size_t range_l = params->range_l;
  const size_t tile_k = params->tile_k;
  const size_t tile_l = params->tile_l;
  const size_t tile_range_k = divide_round_up(range_k, tile_k);
  const size_t tile_range_l = divide_round_up(range_l, tile_l);
  const pthreadpool_task_4d_tile_2d_dynamic_t task =
      (pthreadpool_task_4d_tile_2d_dynamic_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      /* Iterate over the chunk and call the task function. */
      size_t index_i = offset / (range_j * tile_range_k * tile_range_l);
      size_t index_j = (offset / (tile_range_k * tile_range_l)) % range_j;
      size_t tile_index_k = (offset / tile_range_l) % tile_range_k;
      if (tile_range_l == 1) {
        /* If there is only a single tile in the `k`th (last) dimension, then we
         * group by the `j`th (second-last) dimeension. */
        while (chunk_size > 0) {
          const size_t index_k = tile_index_k * tile_k;
          const size_t tile_step_k =
              min(tile_range_k - tile_index_k, chunk_size);
          const size_t step_k = min(tile_step_k * tile_k, range_k - index_k);

          task(argument, index_i, index_j, index_k, /*index_k=*/0, step_k,
               range_l);

          tile_index_k += tile_step_k;
          if (tile_range_k <= tile_index_k) {
            tile_index_k -= tile_range_k;
            index_j += 1;
            if (range_j <= index_j) {
              index_j = 0;
              index_i += 1;
            }
          }
          chunk_size -= tile_step_k;
          offset += tile_step_k;
        }
      } else {
        size_t tile_index_l = offset % tile_range_l;
        while (chunk_size > 0) {
          const size_t index_k = tile_index_k * tile_k;
          const size_t index_l = tile_index_l * tile_l;
          const size_t step_k = min(tile_k, range_k - index_k);
          const size_t tile_step_l =
              min(tile_range_l - tile_index_l, chunk_size);
          const size_t step_l = min(tile_step_l * tile_l, range_l - index_l);

          task(argument, index_i, index_j, index_k, index_l, step_k, step_l);

          tile_index_l += tile_step_l;
          if (tile_range_l <= tile_index_l) {
            tile_index_l -= tile_range_l;
            if (tile_range_k <= ++tile_index_k) {
              tile_index_k = 0;
              index_j += 1;
              if (range_j <= index_j) {
                index_j = 0;
                index_i += 1;
              }
            }
          }
          chunk_size -= tile_step_l;
          offset += tile_step_l;
        }
      }
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_4d_tile_2d_dynamic_with_uarch(
    struct pthreadpool* threadpool, struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  // Get the uarch.
  const uint32_t default_uarch_index =
      threadpool->params.parallelize_4d_tile_2d_dynamic_with_uarch
          .default_uarch_index;
  uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
  uarch_index =
      cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
  if (uarch_index > threadpool->params.parallelize_4d_tile_2d_dynamic_with_uarch
                        .max_uarch_index) {
    uarch_index = default_uarch_index;
  }
#endif

  // Get a handle on the params.
  struct pthreadpool_4d_tile_2d_dynamic_with_uarch_params* params =
      &threadpool->params.parallelize_4d_tile_2d_dynamic_with_uarch;
  const size_t num_threads = threadpool->threads_count.value;
  const size_t range_j = params->range_j;
  const size_t range_k = params->range_k;
  const size_t range_l = params->range_l;
  const size_t tile_k = params->tile_k;
  const size_t tile_l = params->tile_l;
  const size_t tile_range_k = divide_round_up(range_k, tile_k);
  const size_t tile_range_l = divide_round_up(range_l, tile_l);
  const pthreadpool_task_4d_tile_2d_dynamic_with_id_t task =
      (pthreadpool_task_4d_tile_2d_dynamic_with_id_t)
          pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);
  const size_t thread_number = thread->thread_number;
  const size_t fastest_to_slowest_ratio = get_fastest_to_slowest_ratio();

  // Do tiles in our own range first (tid = 0), then the other ranges when we're
  // done.
  for (size_t tid = 0; tid < num_threads; tid++) {
    struct thread_info* thread =
        &threadpool->threads[(num_threads + thread_number - tid) % num_threads];

    size_t offset =
        (tid == 0) ? pthreadpool_load_relaxed_size_t(&thread->range_start) : 0;

    /* Loop as long as there is work to be done. */
    while (true) {
      /* Choose a chunk size based on the remaining amount of work and the
       * current number of threads. */
      size_t chunk_size =
          get_chunk(&thread->range_length, fastest_to_slowest_ratio);
      if (!chunk_size) {
        break;
      }

      /* If this is "our" range, take chunks of tiles from the front, otherwise
       * take them from the back. */
      if (tid != 0) {
        offset = pthreadpool_decrement_n_fetch_relaxed_size_t(
            &thread->range_end, chunk_size);
      }

      /* Iterate over the chunk and call the task function. */
      size_t index_i = offset / (range_j * tile_range_k * tile_range_l);
      size_t index_j = (offset / (tile_range_k * tile_range_l)) % range_j;
      size_t tile_index_k = (offset / tile_range_l) % tile_range_k;
      if (tile_range_l == 1) {
        /* If there is only a single tile in the `k`th (last) dimension, then we
         * group by the `j`th (second-last) dimeension. */
        while (chunk_size > 0) {
          const size_t index_k = tile_index_k * tile_k;
          const size_t tile_step_k =
              min(tile_range_k - tile_index_k, chunk_size);
          const size_t step_k = min(tile_step_k * tile_k, range_k - index_k);

          task(argument, uarch_index, index_i, index_j, index_k, /*index_k=*/0,
               step_k, range_l);

          tile_index_k += tile_step_k;
          if (tile_range_k <= tile_index_k) {
            tile_index_k -= tile_range_k;
            index_j += 1;
            if (range_j <= index_j) {
              index_j = 0;
              index_i += 1;
            }
          }
          chunk_size -= tile_step_k;
          offset += tile_step_k;
        }
      } else {
        size_t tile_index_l = offset % tile_range_l;
        while (chunk_size > 0) {
          const size_t index_k = tile_index_k * tile_k;
          const size_t index_l = tile_index_l * tile_l;
          const size_t step_k = min(tile_k, range_k - index_k);
          const size_t tile_step_l =
              min(tile_range_l - tile_index_l, chunk_size);
          const size_t step_l = min(tile_step_l * tile_l, range_l - index_l);

          task(argument, uarch_index, index_i, index_j, index_k, index_l,
               step_k, step_l);

          tile_index_l += tile_step_l;
          if (tile_range_l <= tile_index_l) {
            tile_index_l -= tile_range_l;
            if (tile_range_k <= ++tile_index_k) {
              tile_index_k = 0;
              index_j += 1;
              if (range_j <= index_j) {
                index_j = 0;
                index_i += 1;
              }
            }
          }
          chunk_size -= tile_step_l;
          offset += tile_step_l;
        }
      }
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_5d(struct pthreadpool* threadpool,
                                  struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_5d_t task =
      (pthreadpool_task_5d_t)pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t range_lm =
      threadpool->params.parallelize_5d.range_lm;
  const struct fxdiv_result_size_t index_ijk_lm =
      fxdiv_divide_size_t(range_start, range_lm);
  const struct fxdiv_divisor_size_t range_k =
      threadpool->params.parallelize_5d.range_k;
  const struct fxdiv_result_size_t index_ij_k =
      fxdiv_divide_size_t(index_ijk_lm.quotient, range_k);
  const struct fxdiv_divisor_size_t range_m =
      threadpool->params.parallelize_5d.range_m;
  const struct fxdiv_result_size_t index_l_m =
      fxdiv_divide_size_t(index_ijk_lm.remainder, range_m);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_5d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(index_ij_k.quotient, range_j);
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t k = index_ij_k.remainder;
  size_t l = index_l_m.quotient;
  size_t m = index_l_m.remainder;

  const size_t range_l = threadpool->params.parallelize_5d.range_l;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, j, k, l, m);
    if (++m == range_m.value) {
      m = 0;
      if (++l == range_l) {
        l = 0;
        if (++k == range_k.value) {
          k = 0;
          if (++j == range_j.value) {
            j = 0;
            i += 1;
          }
        }
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t index_ijk_lm =
          fxdiv_divide_size_t(linear_index, range_lm);
      const struct fxdiv_result_size_t index_ij_k =
          fxdiv_divide_size_t(index_ijk_lm.quotient, range_k);
      const struct fxdiv_result_size_t index_l_m =
          fxdiv_divide_size_t(index_ijk_lm.remainder, range_m);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(index_ij_k.quotient, range_j);
      task(argument, index_i_j.quotient, index_i_j.remainder,
           index_ij_k.remainder, index_l_m.quotient, index_l_m.remainder);
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_5d_tile_1d(struct pthreadpool* threadpool,
                                          struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_5d_tile_1d_t task =
      (pthreadpool_task_5d_tile_1d_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_m =
      threadpool->params.parallelize_5d_tile_1d.tile_range_m;
  const struct fxdiv_result_size_t tile_index_ijkl_m =
      fxdiv_divide_size_t(range_start, tile_range_m);
  const struct fxdiv_divisor_size_t range_kl =
      threadpool->params.parallelize_5d_tile_1d.range_kl;
  const struct fxdiv_result_size_t index_ij_kl =
      fxdiv_divide_size_t(tile_index_ijkl_m.quotient, range_kl);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_5d_tile_1d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(index_ij_kl.quotient, range_j);
  const struct fxdiv_divisor_size_t range_l =
      threadpool->params.parallelize_5d_tile_1d.range_l;
  const struct fxdiv_result_size_t index_k_l =
      fxdiv_divide_size_t(index_ij_kl.remainder, range_l);
  const size_t tile_m = threadpool->params.parallelize_5d_tile_1d.tile_m;
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t k = index_k_l.quotient;
  size_t l = index_k_l.remainder;
  size_t start_m = tile_index_ijkl_m.remainder * tile_m;

  const size_t range_m = threadpool->params.parallelize_5d_tile_1d.range_m;
  const size_t range_k = threadpool->params.parallelize_5d_tile_1d.range_k;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, j, k, l, start_m, min(range_m - start_m, tile_m));
    start_m += tile_m;
    if (start_m >= range_m) {
      start_m = 0;
      if (++l == range_l.value) {
        l = 0;
        if (++k == range_k) {
          k = 0;
          if (++j == range_j.value) {
            j = 0;
            i += 1;
          }
        }
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_ijkl_m =
          fxdiv_divide_size_t(linear_index, tile_range_m);
      const struct fxdiv_result_size_t index_ij_kl =
          fxdiv_divide_size_t(tile_index_ijkl_m.quotient, range_kl);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(index_ij_kl.quotient, range_j);
      const struct fxdiv_result_size_t index_k_l =
          fxdiv_divide_size_t(index_ij_kl.remainder, range_l);
      size_t start_m = tile_index_ijkl_m.remainder * tile_m;
      task(argument, index_i_j.quotient, index_i_j.remainder,
           index_k_l.quotient, index_k_l.remainder, start_m,
           min(range_m - start_m, tile_m));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_5d_tile_2d(struct pthreadpool* threadpool,
                                          struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_5d_tile_2d_t task =
      (pthreadpool_task_5d_tile_2d_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_lm =
      threadpool->params.parallelize_5d_tile_2d.tile_range_lm;
  const struct fxdiv_result_size_t tile_index_ijk_lm =
      fxdiv_divide_size_t(range_start, tile_range_lm);
  const struct fxdiv_divisor_size_t range_k =
      threadpool->params.parallelize_5d_tile_2d.range_k;
  const struct fxdiv_result_size_t index_ij_k =
      fxdiv_divide_size_t(tile_index_ijk_lm.quotient, range_k);
  const struct fxdiv_divisor_size_t tile_range_m =
      threadpool->params.parallelize_5d_tile_2d.tile_range_m;
  const struct fxdiv_result_size_t tile_index_l_m =
      fxdiv_divide_size_t(tile_index_ijk_lm.remainder, tile_range_m);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_5d_tile_2d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(index_ij_k.quotient, range_j);
  const size_t tile_l = threadpool->params.parallelize_5d_tile_2d.tile_l;
  const size_t tile_m = threadpool->params.parallelize_5d_tile_2d.tile_m;
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t k = index_ij_k.remainder;
  size_t start_l = tile_index_l_m.quotient * tile_l;
  size_t start_m = tile_index_l_m.remainder * tile_m;

  const size_t range_m = threadpool->params.parallelize_5d_tile_2d.range_m;
  const size_t range_l = threadpool->params.parallelize_5d_tile_2d.range_l;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, j, k, start_l, start_m, min(range_l - start_l, tile_l),
         min(range_m - start_m, tile_m));
    start_m += tile_m;
    if (start_m >= range_m) {
      start_m = 0;
      start_l += tile_l;
      if (start_l >= range_l) {
        start_l = 0;
        if (++k == range_k.value) {
          k = 0;
          if (++j == range_j.value) {
            j = 0;
            i += 1;
          }
        }
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_ijk_lm =
          fxdiv_divide_size_t(linear_index, tile_range_lm);
      const struct fxdiv_result_size_t index_ij_k =
          fxdiv_divide_size_t(tile_index_ijk_lm.quotient, range_k);
      const struct fxdiv_result_size_t tile_index_l_m =
          fxdiv_divide_size_t(tile_index_ijk_lm.remainder, tile_range_m);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(index_ij_k.quotient, range_j);
      const size_t start_l = tile_index_l_m.quotient * tile_l;
      const size_t start_m = tile_index_l_m.remainder * tile_m;
      task(argument, index_i_j.quotient, index_i_j.remainder,
           index_ij_k.remainder, start_l, start_m,
           min(range_l - start_l, tile_l), min(range_m - start_m, tile_m));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_6d(struct pthreadpool* threadpool,
                                  struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_6d_t task =
      (pthreadpool_task_6d_t)pthreadpool_load_relaxed_void_p(&threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t range_lmn =
      threadpool->params.parallelize_6d.range_lmn;
  const struct fxdiv_result_size_t index_ijk_lmn =
      fxdiv_divide_size_t(range_start, range_lmn);
  const struct fxdiv_divisor_size_t range_k =
      threadpool->params.parallelize_6d.range_k;
  const struct fxdiv_result_size_t index_ij_k =
      fxdiv_divide_size_t(index_ijk_lmn.quotient, range_k);
  const struct fxdiv_divisor_size_t range_n =
      threadpool->params.parallelize_6d.range_n;
  const struct fxdiv_result_size_t index_lm_n =
      fxdiv_divide_size_t(index_ijk_lmn.remainder, range_n);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_6d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(index_ij_k.quotient, range_j);
  const struct fxdiv_divisor_size_t range_m =
      threadpool->params.parallelize_6d.range_m;
  const struct fxdiv_result_size_t index_l_m =
      fxdiv_divide_size_t(index_lm_n.quotient, range_m);
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t k = index_ij_k.remainder;
  size_t l = index_l_m.quotient;
  size_t m = index_l_m.remainder;
  size_t n = index_lm_n.remainder;

  const size_t range_l = threadpool->params.parallelize_6d.range_l;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, j, k, l, m, n);
    if (++n == range_n.value) {
      n = 0;
      if (++m == range_m.value) {
        m = 0;
        if (++l == range_l) {
          l = 0;
          if (++k == range_k.value) {
            k = 0;
            if (++j == range_j.value) {
              j = 0;
              i += 1;
            }
          }
        }
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t index_ijk_lmn =
          fxdiv_divide_size_t(linear_index, range_lmn);
      const struct fxdiv_result_size_t index_ij_k =
          fxdiv_divide_size_t(index_ijk_lmn.quotient, range_k);
      const struct fxdiv_result_size_t index_lm_n =
          fxdiv_divide_size_t(index_ijk_lmn.remainder, range_n);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(index_ij_k.quotient, range_j);
      const struct fxdiv_result_size_t index_l_m =
          fxdiv_divide_size_t(index_lm_n.quotient, range_m);
      task(argument, index_i_j.quotient, index_i_j.remainder,
           index_ij_k.remainder, index_l_m.quotient, index_l_m.remainder,
           index_lm_n.remainder);
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_6d_tile_1d(struct pthreadpool* threadpool,
                                          struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_6d_tile_1d_t task =
      (pthreadpool_task_6d_tile_1d_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_lmn =
      threadpool->params.parallelize_6d_tile_1d.tile_range_lmn;
  const struct fxdiv_result_size_t tile_index_ijk_lmn =
      fxdiv_divide_size_t(range_start, tile_range_lmn);
  const struct fxdiv_divisor_size_t range_k =
      threadpool->params.parallelize_6d_tile_1d.range_k;
  const struct fxdiv_result_size_t index_ij_k =
      fxdiv_divide_size_t(tile_index_ijk_lmn.quotient, range_k);
  const struct fxdiv_divisor_size_t tile_range_n =
      threadpool->params.parallelize_6d_tile_1d.tile_range_n;
  const struct fxdiv_result_size_t tile_index_lm_n =
      fxdiv_divide_size_t(tile_index_ijk_lmn.remainder, tile_range_n);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_6d_tile_1d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(index_ij_k.quotient, range_j);
  const struct fxdiv_divisor_size_t range_m =
      threadpool->params.parallelize_6d_tile_1d.range_m;
  const struct fxdiv_result_size_t index_l_m =
      fxdiv_divide_size_t(tile_index_lm_n.quotient, range_m);
  const size_t tile_n = threadpool->params.parallelize_6d_tile_1d.tile_n;
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t k = index_ij_k.remainder;
  size_t l = index_l_m.quotient;
  size_t m = index_l_m.remainder;
  size_t start_n = tile_index_lm_n.remainder * tile_n;

  const size_t range_n = threadpool->params.parallelize_6d_tile_1d.range_n;
  const size_t range_l = threadpool->params.parallelize_6d_tile_1d.range_l;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, j, k, l, m, start_n, min(range_n - start_n, tile_n));
    start_n += tile_n;
    if (start_n >= range_n) {
      start_n = 0;
      if (++m == range_m.value) {
        m = 0;
        if (++l == range_l) {
          l = 0;
          if (++k == range_k.value) {
            k = 0;
            if (++j == range_j.value) {
              j = 0;
              i += 1;
            }
          }
        }
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_ijk_lmn =
          fxdiv_divide_size_t(linear_index, tile_range_lmn);
      const struct fxdiv_result_size_t index_ij_k =
          fxdiv_divide_size_t(tile_index_ijk_lmn.quotient, range_k);
      const struct fxdiv_result_size_t tile_index_lm_n =
          fxdiv_divide_size_t(tile_index_ijk_lmn.remainder, tile_range_n);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(index_ij_k.quotient, range_j);
      const struct fxdiv_result_size_t index_l_m =
          fxdiv_divide_size_t(tile_index_lm_n.quotient, range_m);
      const size_t start_n = tile_index_lm_n.remainder * tile_n;
      task(argument, index_i_j.quotient, index_i_j.remainder,
           index_ij_k.remainder, index_l_m.quotient, index_l_m.remainder,
           start_n, min(range_n - start_n, tile_n));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

static void thread_parallelize_6d_tile_2d(struct pthreadpool* threadpool,
                                          struct thread_info* thread) {
  assert(threadpool != NULL);
  assert(thread != NULL);

  const pthreadpool_task_6d_tile_2d_t task =
      (pthreadpool_task_6d_tile_2d_t)pthreadpool_load_relaxed_void_p(
          &threadpool->task);
  void* const argument = pthreadpool_load_relaxed_void_p(&threadpool->argument);

  /* Process thread's own range of items */
  const size_t range_start =
      pthreadpool_load_relaxed_size_t(&thread->range_start);
  const struct fxdiv_divisor_size_t tile_range_mn =
      threadpool->params.parallelize_6d_tile_2d.tile_range_mn;
  const struct fxdiv_result_size_t tile_index_ijkl_mn =
      fxdiv_divide_size_t(range_start, tile_range_mn);
  const struct fxdiv_divisor_size_t range_kl =
      threadpool->params.parallelize_6d_tile_2d.range_kl;
  const struct fxdiv_result_size_t index_ij_kl =
      fxdiv_divide_size_t(tile_index_ijkl_mn.quotient, range_kl);
  const struct fxdiv_divisor_size_t tile_range_n =
      threadpool->params.parallelize_6d_tile_2d.tile_range_n;
  const struct fxdiv_result_size_t tile_index_m_n =
      fxdiv_divide_size_t(tile_index_ijkl_mn.remainder, tile_range_n);
  const struct fxdiv_divisor_size_t range_j =
      threadpool->params.parallelize_6d_tile_2d.range_j;
  const struct fxdiv_result_size_t index_i_j =
      fxdiv_divide_size_t(index_ij_kl.quotient, range_j);
  const struct fxdiv_divisor_size_t range_l =
      threadpool->params.parallelize_6d_tile_2d.range_l;
  const struct fxdiv_result_size_t index_k_l =
      fxdiv_divide_size_t(index_ij_kl.remainder, range_l);
  const size_t tile_m = threadpool->params.parallelize_6d_tile_2d.tile_m;
  const size_t tile_n = threadpool->params.parallelize_6d_tile_2d.tile_n;
  size_t i = index_i_j.quotient;
  size_t j = index_i_j.remainder;
  size_t k = index_k_l.quotient;
  size_t l = index_k_l.remainder;
  size_t start_m = tile_index_m_n.quotient * tile_m;
  size_t start_n = tile_index_m_n.remainder * tile_n;

  const size_t range_n = threadpool->params.parallelize_6d_tile_2d.range_n;
  const size_t range_m = threadpool->params.parallelize_6d_tile_2d.range_m;
  const size_t range_k = threadpool->params.parallelize_6d_tile_2d.range_k;
  while (pthreadpool_try_decrement_relaxed_size_t(&thread->range_length)) {
    task(argument, i, j, k, l, start_m, start_n, min(range_m - start_m, tile_m),
         min(range_n - start_n, tile_n));
    start_n += tile_n;
    if (start_n >= range_n) {
      start_n = 0;
      start_m += tile_m;
      if (start_m >= range_m) {
        start_m = 0;
        if (++l == range_l.value) {
          l = 0;
          if (++k == range_k) {
            k = 0;
            if (++j == range_j.value) {
              j = 0;
              i += 1;
            }
          }
        }
      }
    }
  }

  /* There still may be other threads with work */
  const size_t thread_number = thread->thread_number;
  const size_t threads_count = threadpool->threads_count.value;
  for (size_t tid = modulo_decrement(thread_number, threads_count);
       tid != thread_number; tid = modulo_decrement(tid, threads_count)) {
    struct thread_info* other_thread = &threadpool->threads[tid];
    while (
        pthreadpool_try_decrement_relaxed_size_t(&other_thread->range_length)) {
      const size_t linear_index =
          pthreadpool_decrement_fetch_relaxed_size_t(&other_thread->range_end);
      const struct fxdiv_result_size_t tile_index_ijkl_mn =
          fxdiv_divide_size_t(linear_index, tile_range_mn);
      const struct fxdiv_result_size_t index_ij_kl =
          fxdiv_divide_size_t(tile_index_ijkl_mn.quotient, range_kl);
      const struct fxdiv_result_size_t tile_index_m_n =
          fxdiv_divide_size_t(tile_index_ijkl_mn.remainder, tile_range_n);
      const struct fxdiv_result_size_t index_i_j =
          fxdiv_divide_size_t(index_ij_kl.quotient, range_j);
      const struct fxdiv_result_size_t index_k_l =
          fxdiv_divide_size_t(index_ij_kl.remainder, range_l);
      const size_t start_m = tile_index_m_n.quotient * tile_m;
      const size_t start_n = tile_index_m_n.remainder * tile_n;
      task(argument, index_i_j.quotient, index_i_j.remainder,
           index_k_l.quotient, index_k_l.remainder, start_m, start_n,
           min(range_m - start_m, tile_m), min(range_n - start_n, tile_n));
    }
  }

  /* Make changes by this thread visible to other threads */
  pthreadpool_fence_release();
}

void PTHREADPOOL_IMPL(pthreadpool_parallelize_1d)(
    struct pthreadpool* threadpool, pthreadpool_task_1d_t function,
    void* context, size_t range, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 || range <= 1) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range; i++) {
      function(context, i);
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    thread_function_t parallelize_1d = &thread_parallelize_1d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (range < range_threshold) {
      parallelize_1d = &pthreadpool_thread_parallelize_1d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_1d, NULL, 0,
                            (void*)function, context, range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_1d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_1d_with_thread)(
    struct pthreadpool* threadpool, pthreadpool_task_1d_with_thread_t function,
    void* context, size_t range, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 || range <= 1) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range; i++) {
      function(context, 0, i);
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    thread_function_t parallelize_1d_with_thread =
        &thread_parallelize_1d_with_thread;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (range < range_threshold) {
      parallelize_1d_with_thread =
          &pthreadpool_thread_parallelize_1d_with_thread_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_1d_with_thread, NULL, 0,
                            (void*)function, context, range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_1d_with_thread)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_1d_with_uarch)(
    pthreadpool_t threadpool, pthreadpool_task_1d_with_id_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 || range <= 1) {
    /* No thread pool used: execute task sequentially on the calling thread */

    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range; i++) {
      function(context, uarch_index, i);
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const struct pthreadpool_1d_with_uarch_params params = {
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
    };
    thread_function_t parallelize_1d_with_uarch =
        &thread_parallelize_1d_with_uarch;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (range < range_threshold) {
      parallelize_1d_with_uarch =
          &pthreadpool_thread_parallelize_1d_with_uarch_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_1d_with_uarch, &params,
                            sizeof(params), function, context, range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_1d_with_uarch)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_1d_tile_1d)(
    pthreadpool_t threadpool, pthreadpool_task_1d_tile_1d_t function,
    void* context, size_t range, size_t tile, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 || range <= tile) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range; i += tile) {
      function(context, i, min(range - i, tile));
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range = divide_round_up(range, tile);
    const struct pthreadpool_1d_tile_1d_params params = {
        .range = range,
        .tile = tile,
    };
    thread_function_t parallelize_1d_tile_1d = &thread_parallelize_1d_tile_1d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (range < range_threshold) {
      parallelize_1d_tile_1d =
          &pthreadpool_thread_parallelize_1d_tile_1d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_1d_tile_1d, &params,
                            sizeof(params), function, context, tile_range,
                            flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_1d_tile_1d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_1d_tile_1d_dynamic)(
    pthreadpool_t threadpool, pthreadpool_task_1d_tile_1d_dynamic_t function,
    void* context, size_t range, size_t tile, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 || range <= tile) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    function(context, 0, range);
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range = divide_round_up(range, tile);
    const struct pthreadpool_1d_tile_1d_dynamic_params params = {
        .range = range,
        .tile = tile,
    };
    pthreadpool_parallelize(threadpool, thread_parallelize_1d_tile_1d_dynamic,
                            &params, sizeof(params), function, context,
                            tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_1d_tile_1d_dynamic)

void PTHREADPOOL_IMPL(
    pthreadpool_parallelize_1d_tile_1d_dynamic_with_thread)(
    pthreadpool_t threadpool,
    pthreadpool_task_1d_tile_1d_dynamic_with_id_t function, void* context,
    size_t range, size_t tile, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 || range <= tile) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    function(context, /*thread_id=*/0, /*index=*/0, range);
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range = divide_round_up(range, tile);
    const struct pthreadpool_1d_tile_1d_dynamic_params params = {
        .range = range,
        .tile = tile,
    };
    pthreadpool_parallelize(
        threadpool, thread_parallelize_1d_tile_1d_dynamic_with_thread, &params,
        sizeof(params), function, context, tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_1d_tile_1d_dynamic_with_thread)

void PTHREADPOOL_IMPL(
    pthreadpool_parallelize_1d_tile_1d_dynamic_with_uarch_with_thread)(
    pthreadpool_t threadpool,
    pthreadpool_task_1d_tile_1d_dynamic_with_id_with_thread_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range, size_t tile, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 || range <= tile) {
    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    function(context, uarch_index, /*thread_id=*/0, /*index=*/0, range);
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range = divide_round_up(range, tile);
    const struct pthreadpool_1d_tile_1d_dynamic_with_uarch_params params = {
        .range = range,
        .tile = tile,
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
    };
    pthreadpool_parallelize(
        threadpool,
        thread_parallelize_1d_tile_1d_dynamic_with_uarch_with_thread, &params,
        sizeof(params), function, context, tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(
    pthreadpool_parallelize_1d_tile_1d_dynamic_with_uarch_with_thread)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_2d)(
    pthreadpool_t threadpool, pthreadpool_task_2d_t function, void* context,
    size_t range_i, size_t range_j, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      (range_i | range_j) <= 1) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        function(context, i, j);
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t range = range_i * range_j;
    const struct pthreadpool_2d_params params = {
        .range_j = fxdiv_init_size_t(range_j),
    };
    thread_function_t parallelize_2d = &thread_parallelize_2d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (range < range_threshold) {
      parallelize_2d = &pthreadpool_thread_parallelize_2d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_2d, &params, sizeof(params),
                            function, context, range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_2d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_2d_with_thread)(
    pthreadpool_t threadpool, pthreadpool_task_2d_with_thread_t function,
    void* context, size_t range_i, size_t range_j, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      (range_i | range_j) <= 1) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        function(context, 0, i, j);
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t range = range_i * range_j;
    const struct pthreadpool_2d_params params = {
        .range_j = fxdiv_init_size_t(range_j),
    };
    thread_function_t parallelize_2d_with_thread =
        &thread_parallelize_2d_with_thread;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (range < range_threshold) {
      parallelize_2d_with_thread =
          &pthreadpool_thread_parallelize_2d_with_thread_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_2d_with_thread, &params,
                            sizeof(params), function, context, range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_2d_with_thread)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_2d_tile_1d)(
    pthreadpool_t threadpool, pthreadpool_task_2d_tile_1d_t function,
    void* context, size_t range_i, size_t range_j, size_t tile_j,
    uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      (range_i <= 1 && range_j <= tile_j)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j += tile_j) {
        function(context, i, j, min(range_j - j, tile_j));
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range = range_i * tile_range_j;
    const struct pthreadpool_2d_tile_1d_params params = {
        .range_j = range_j,
        .tile_j = tile_j,
        .tile_range_j = fxdiv_init_size_t(tile_range_j),
    };
    thread_function_t parallelize_2d_tile_1d = &thread_parallelize_2d_tile_1d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_2d_tile_1d =
          &pthreadpool_thread_parallelize_2d_tile_1d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_2d_tile_1d, &params,
                            sizeof(params), function, context, tile_range,
                            flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_2d_tile_1d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_2d_tile_1d_with_uarch)(
    pthreadpool_t threadpool, pthreadpool_task_2d_tile_1d_with_id_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t tile_j, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      (range_i <= 1 && range_j <= tile_j)) {
    /* No thread pool used: execute task sequentially on the calling thread */

    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j += tile_j) {
        function(context, uarch_index, i, j, min(range_j - j, tile_j));
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range = range_i * tile_range_j;
    const struct pthreadpool_2d_tile_1d_with_uarch_params params = {
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
        .range_j = range_j,
        .tile_j = tile_j,
        .tile_range_j = fxdiv_init_size_t(tile_range_j),
    };
    thread_function_t parallelize_2d_tile_1d_with_uarch =
        &thread_parallelize_2d_tile_1d_with_uarch;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_2d_tile_1d_with_uarch =
          &pthreadpool_thread_parallelize_2d_tile_1d_with_uarch_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_2d_tile_1d_with_uarch,
                            &params, sizeof(params), function, context,
                            tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_2d_tile_1d_with_uarch)

void PTHREADPOOL_IMPL(
    pthreadpool_parallelize_2d_tile_1d_with_uarch_with_thread)(
    pthreadpool_t threadpool,
    pthreadpool_task_2d_tile_1d_with_id_with_thread_t function, void* context,
    uint32_t default_uarch_index, uint32_t max_uarch_index, size_t range_i,
    size_t range_j, size_t tile_j, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      (range_i <= 1 && range_j <= tile_j)) {
    /* No thread pool used: execute task sequentially on the calling thread */

    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j += tile_j) {
        function(context, uarch_index, 0, i, j, min(range_j - j, tile_j));
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range = range_i * tile_range_j;
    const struct pthreadpool_2d_tile_1d_with_uarch_params params = {
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
        .range_j = range_j,
        .tile_j = tile_j,
        .tile_range_j = fxdiv_init_size_t(tile_range_j),
    };
    thread_function_t parallelize_2d_tile_1d_with_uarch_with_thread =
        &thread_parallelize_2d_tile_1d_with_uarch_with_thread;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_2d_tile_1d_with_uarch_with_thread =
          &pthreadpool_thread_parallelize_2d_tile_1d_with_uarch_with_thread_fastpath;
    }
#endif
    pthreadpool_parallelize(
        threadpool, parallelize_2d_tile_1d_with_uarch_with_thread, &params,
        sizeof(params), function, context, tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(
    pthreadpool_parallelize_2d_tile_1d_with_uarch_with_thread)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_2d_tile_1d_dynamic)(
    pthreadpool_t threadpool, pthreadpool_task_2d_tile_1d_dynamic_t function,
    void* context, size_t range_i, size_t range_j, size_t tile_j,
    uint32_t flags) {
  if (threadpool == NULL || threadpool->threads_count.value <= 1 ||
      (range_i <= 1 && range_j <= tile_j)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t index_i = 0; index_i < range_i; index_i++) {
      function(context, index_i, /*index_j=*/0, range_j);
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range = range_i * tile_range_j;
    const struct pthreadpool_2d_tile_1d_dynamic_params params = {
        .range_i = range_i,
        .range_j = range_j,
        .tile_j = tile_j,
    };
    pthreadpool_parallelize(threadpool, thread_parallelize_2d_tile_1d_dynamic,
                            &params, sizeof(params), function, context,
                            tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_2d_tile_1d_dynamic)

void PTHREADPOOL_IMPL(
    pthreadpool_parallelize_2d_tile_1d_dynamic_with_thread)(
    pthreadpool_t threadpool,
    pthreadpool_task_2d_tile_1d_dynamic_with_id_t function, void* context,
    size_t range_i, size_t range_j, size_t tile_j, uint32_t flags) {
  if (threadpool == NULL || threadpool->threads_count.value <= 1 ||
      (range_i <= 1 && range_j <= tile_j)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t index_i = 0; index_i < range_i; index_i++) {
      function(context, /*thread_id=*/0, index_i, /*index_j=*/0, range_j);
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range = range_i * tile_range_j;
    const struct pthreadpool_2d_tile_1d_dynamic_params params = {
        .range_i = range_i,
        .range_j = range_j,
        .tile_j = tile_j,
    };
    pthreadpool_parallelize(
        threadpool, thread_parallelize_2d_tile_1d_dynamic_with_thread, &params,
        sizeof(params), function, context, tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_2d_tile_1d_dynamic_with_thread)

void PTHREADPOOL_IMPL(
    pthreadpool_parallelize_2d_tile_1d_dynamic_with_uarch_with_thread)(
    pthreadpool_t threadpool,
    pthreadpool_task_2d_tile_1d_dynamic_with_id_with_thread_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t tile_j, uint32_t flags) {
  if (threadpool == NULL || threadpool->threads_count.value <= 1 ||
      (range_i <= 1 && range_j <= tile_j)) {
    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t index_i = 0; index_i < range_i; index_i++) {
      function(context, uarch_index, /*thread_id=*/0, index_i, /*index_j=*/0,
               range_j);
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range = range_i * tile_range_j;
    const struct pthreadpool_2d_tile_1d_dynamic_with_uarch_params params = {
        .range_i = range_i,
        .range_j = range_j,
        .tile_j = tile_j,
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
    };
    pthreadpool_parallelize(
        threadpool,
        thread_parallelize_2d_tile_1d_dynamic_with_uarch_with_thread, &params,
        sizeof(params), function, context, tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(
    pthreadpool_parallelize_2d_tile_1d_dynamic_with_uarch_with_thread)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_2d_tile_2d)(
    pthreadpool_t threadpool, pthreadpool_task_2d_tile_2d_t function,
    void* context, size_t range_i, size_t range_j, size_t tile_i, size_t tile_j,
    uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      (range_i <= tile_i && range_j <= tile_j)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i += tile_i) {
      for (size_t j = 0; j < range_j; j += tile_j) {
        function(context, i, j, min(range_i - i, tile_i),
                 min(range_j - j, tile_j));
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_i = divide_round_up(range_i, tile_i);
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range = tile_range_i * tile_range_j;
    const struct pthreadpool_2d_tile_2d_params params = {
        .range_i = range_i,
        .tile_i = tile_i,
        .range_j = range_j,
        .tile_j = tile_j,
        .tile_range_j = fxdiv_init_size_t(tile_range_j),
    };
    thread_function_t parallelize_2d_tile_2d = &thread_parallelize_2d_tile_2d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_2d_tile_2d =
          &pthreadpool_thread_parallelize_2d_tile_2d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_2d_tile_2d, &params,
                            sizeof(params), function, context, tile_range,
                            flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_2d_tile_2d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_2d_tile_2d_dynamic)(
    pthreadpool_t threadpool, pthreadpool_task_2d_tile_2d_dynamic_t function,
    void* context, size_t range_i, size_t range_j, size_t tile_i, size_t tile_j,
    uint32_t flags) {
  if (threadpool == NULL || threadpool->threads_count.value <= 1 ||
      (range_i <= tile_i && range_j <= tile_j)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    if (range_j <= tile_j) {
      function(context, /*index_i=*/0, /*index_j=*/0, range_i, range_j);
    } else {
      for (size_t index_i = 0; index_i < range_i; index_i += tile_i) {
        function(context, index_i, /*index_j=*/0,
                 min(tile_i, range_i - index_i), range_j);
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_i = divide_round_up(range_i, tile_i);
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range = tile_range_i * tile_range_j;
    const struct pthreadpool_2d_tile_2d_dynamic_params params = {
        .range_i = range_i,
        .range_j = range_j,
        .tile_i = tile_i,
        .tile_j = tile_j,
    };
    pthreadpool_parallelize(threadpool, thread_parallelize_2d_tile_2d_dynamic,
                            &params, sizeof(params), function, context,
                            tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_2d_tile_2d_dynamic)

void PTHREADPOOL_IMPL(
    pthreadpool_parallelize_2d_tile_2d_dynamic_with_uarch)(
    pthreadpool_t threadpool,
    pthreadpool_task_2d_tile_2d_dynamic_with_id_t function, void* context,
    uint32_t default_uarch_index, uint32_t max_uarch_index, size_t range_i,
    size_t range_j, size_t tile_i, size_t tile_j, uint32_t flags) {
  if (threadpool == NULL || threadpool->threads_count.value <= 1 ||
      (range_i <= tile_i && range_j <= tile_j)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    if (range_j <= tile_j) {
      function(context, uarch_index, /*index_i=*/0, /*index_j=*/0, range_i,
               range_j);
    } else {
      for (size_t index_i = 0; index_i < range_i; index_i += tile_i) {
        function(context, uarch_index, index_i, /*index_j=*/0,
                 min(tile_i, range_i - index_i), range_j);
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_i = divide_round_up(range_i, tile_i);
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range = tile_range_i * tile_range_j;
    const struct pthreadpool_2d_tile_2d_dynamic_with_uarch_params params = {
        .range_i = range_i,
        .range_j = range_j,
        .tile_i = tile_i,
        .tile_j = tile_j,
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
    };
    pthreadpool_parallelize(
        threadpool, thread_parallelize_2d_tile_2d_dynamic_with_uarch, &params,
        sizeof(params), function, context, tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_2d_tile_2d_dynamic_with_uarch)

void PTHREADPOOL_IMPL(
    pthreadpool_parallelize_2d_tile_2d_dynamic_with_thread)(
    pthreadpool_t threadpool,
    pthreadpool_task_2d_tile_2d_dynamic_with_id_t function, void* context,
    size_t range_i, size_t range_j, size_t tile_i, size_t tile_j,
    uint32_t flags) {
  if (threadpool == NULL || threadpool->threads_count.value <= 1 ||
      (range_i <= tile_i && range_j <= tile_j)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    if (range_j <= tile_j) {
      function(context, /*thread_id=*/0, /*index_i=*/0, /*index_j=*/0, range_i,
               range_j);
    } else {
      for (size_t index_i = 0; index_i < range_i; index_i += tile_i) {
        function(context, /*thread_id=*/0, index_i, /*index_j=*/0,
                 min(tile_i, range_i - index_i), range_j);
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_i = divide_round_up(range_i, tile_i);
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range = tile_range_i * tile_range_j;
    const struct pthreadpool_2d_tile_2d_dynamic_params params = {
        .range_i = range_i,
        .range_j = range_j,
        .tile_i = tile_i,
        .tile_j = tile_j,
    };
    pthreadpool_parallelize(
        threadpool, thread_parallelize_2d_tile_2d_dynamic_with_thread, &params,
        sizeof(params), function, context, tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_2d_tile_2d_dynamic_with_thread)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_2d_tile_2d_with_uarch)(
    pthreadpool_t threadpool, pthreadpool_task_2d_tile_2d_with_id_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t tile_i, size_t tile_j,
    uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      (range_i <= tile_i && range_j <= tile_j)) {
    /* No thread pool used: execute task sequentially on the calling thread */

    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i += tile_i) {
      for (size_t j = 0; j < range_j; j += tile_j) {
        function(context, uarch_index, i, j, min(range_i - i, tile_i),
                 min(range_j - j, tile_j));
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_i = divide_round_up(range_i, tile_i);
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range = tile_range_i * tile_range_j;
    const struct pthreadpool_2d_tile_2d_with_uarch_params params = {
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
        .range_i = range_i,
        .tile_i = tile_i,
        .range_j = range_j,
        .tile_j = tile_j,
        .tile_range_j = fxdiv_init_size_t(tile_range_j),
    };
    thread_function_t parallelize_2d_tile_2d_with_uarch =
        &thread_parallelize_2d_tile_2d_with_uarch;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_2d_tile_2d_with_uarch =
          &pthreadpool_thread_parallelize_2d_tile_2d_with_uarch_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_2d_tile_2d_with_uarch,
                            &params, sizeof(params), function, context,
                            tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_2d_tile_2d_with_uarch)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_3d)(
    pthreadpool_t threadpool, pthreadpool_task_3d_t function, void* context,
    size_t range_i, size_t range_j, size_t range_k, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      (range_i | range_j | range_k) <= 1) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k++) {
          function(context, i, j, k);
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t range = range_i * range_j * range_k;
    const struct pthreadpool_3d_params params = {
        .range_j = fxdiv_init_size_t(range_j),
        .range_k = fxdiv_init_size_t(range_k),
    };
    thread_function_t parallelize_3d = &thread_parallelize_3d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (range < range_threshold) {
      parallelize_3d = &pthreadpool_thread_parallelize_3d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_3d, &params, sizeof(params),
                            function, context, range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_3d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_3d_tile_1d)(
    pthreadpool_t threadpool, pthreadpool_task_3d_tile_1d_t function,
    void* context, size_t range_i, size_t range_j, size_t range_k,
    size_t tile_k, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      ((range_i | range_j) <= 1 && range_k <= tile_k)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k += tile_k) {
          function(context, i, j, k, min(range_k - k, tile_k));
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_k = divide_round_up(range_k, tile_k);
    const size_t tile_range = range_i * range_j * tile_range_k;
    const struct pthreadpool_3d_tile_1d_params params = {
        .range_k = range_k,
        .tile_k = tile_k,
        .range_j = fxdiv_init_size_t(range_j),
        .tile_range_k = fxdiv_init_size_t(tile_range_k),
    };
    thread_function_t parallelize_3d_tile_1d = &thread_parallelize_3d_tile_1d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_3d_tile_1d =
          &pthreadpool_thread_parallelize_3d_tile_1d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_3d_tile_1d, &params,
                            sizeof(params), function, context, tile_range,
                            flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_3d_tile_1d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_3d_tile_1d_with_thread)(
    pthreadpool_t threadpool,
    pthreadpool_task_3d_tile_1d_with_thread_t function, void* context,
    size_t range_i, size_t range_j, size_t range_k, size_t tile_k,
    uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      ((range_i | range_j) <= 1 && range_k <= tile_k)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k += tile_k) {
          function(context, 0, i, j, k, min(range_k - k, tile_k));
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_k = divide_round_up(range_k, tile_k);
    const size_t tile_range = range_i * range_j * tile_range_k;
    const struct pthreadpool_3d_tile_1d_params params = {
        .range_k = range_k,
        .tile_k = tile_k,
        .range_j = fxdiv_init_size_t(range_j),
        .tile_range_k = fxdiv_init_size_t(tile_range_k),
    };
    thread_function_t parallelize_3d_tile_1d_with_thread =
        &thread_parallelize_3d_tile_1d_with_thread;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_3d_tile_1d_with_thread =
          &pthreadpool_thread_parallelize_3d_tile_1d_with_thread_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_3d_tile_1d_with_thread,
                            &params, sizeof(params), function, context,
                            tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_3d_tile_1d_with_thread)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_3d_tile_1d_with_uarch)(
    pthreadpool_t threadpool, pthreadpool_task_3d_tile_1d_with_id_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t range_k, size_t tile_k,
    uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      ((range_i | range_j) <= 1 && range_k <= tile_k)) {
    /* No thread pool used: execute task sequentially on the calling thread */

    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k += tile_k) {
          function(context, uarch_index, i, j, k, min(range_k - k, tile_k));
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_k = divide_round_up(range_k, tile_k);
    const size_t tile_range = range_i * range_j * tile_range_k;
    const struct pthreadpool_3d_tile_1d_with_uarch_params params = {
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
        .range_k = range_k,
        .tile_k = tile_k,
        .range_j = fxdiv_init_size_t(range_j),
        .tile_range_k = fxdiv_init_size_t(tile_range_k),
    };
    thread_function_t parallelize_3d_tile_1d_with_uarch =
        &thread_parallelize_3d_tile_1d_with_uarch;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_3d_tile_1d_with_uarch =
          &pthreadpool_thread_parallelize_3d_tile_1d_with_uarch_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_3d_tile_1d_with_uarch,
                            &params, sizeof(params), function, context,
                            tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_3d_tile_1d_with_uarch)

void PTHREADPOOL_IMPL(
    pthreadpool_parallelize_3d_tile_1d_with_uarch_with_thread)(
    pthreadpool_t threadpool,
    pthreadpool_task_3d_tile_1d_with_id_with_thread_t function, void* context,
    uint32_t default_uarch_index, uint32_t max_uarch_index, size_t range_i,
    size_t range_j, size_t range_k, size_t tile_k, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      ((range_i | range_j) <= 1 && range_k <= tile_k)) {
    /* No thread pool used: execute task sequentially on the calling thread */

    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k += tile_k) {
          function(context, uarch_index, 0, i, j, k, min(range_k - k, tile_k));
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_k = divide_round_up(range_k, tile_k);
    const size_t tile_range = range_i * range_j * tile_range_k;
    const struct pthreadpool_3d_tile_1d_with_uarch_params params = {
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
        .range_k = range_k,
        .tile_k = tile_k,
        .range_j = fxdiv_init_size_t(range_j),
        .tile_range_k = fxdiv_init_size_t(tile_range_k),
    };
    thread_function_t parallelize_3d_tile_1d_with_uarch_with_thread =
        &thread_parallelize_3d_tile_1d_with_uarch_with_thread;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_3d_tile_1d_with_uarch_with_thread =
          &pthreadpool_thread_parallelize_3d_tile_1d_with_uarch_with_thread_fastpath;
    }
#endif
    pthreadpool_parallelize(
        threadpool, parallelize_3d_tile_1d_with_uarch_with_thread, &params,
        sizeof(params), function, context, tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_3d_tile_1d_with_uarch_with_thread)

void PTHREADPOOL_IMPL(
    pthreadpool_parallelize_3d_tile_1d_dynamic_with_thread)(
    pthreadpool_t threadpool,
    pthreadpool_task_3d_tile_1d_dynamic_with_id_t function, void* context,
    size_t range_i, size_t range_j, size_t range_k, size_t tile_k,
    uint32_t flags) {
  if (threadpool == NULL || threadpool->threads_count.value <= 1 ||
      (range_i <= 1 && range_j <= 1 && range_k <= tile_k)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t index_i = 0; index_i < range_i; index_i++) {
      for (size_t index_j = 0; index_j < range_j; index_j++) {
        function(context, /*thread_id=*/0, index_i, index_j, /*index_k=*/0,
                 range_k);
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_k = divide_round_up(range_k, tile_k);
    const size_t tile_range = range_i * range_j * tile_range_k;
    const struct pthreadpool_3d_tile_1d_dynamic_params params = {
        .range_i = range_i,
        .range_j = range_j,
        .range_k = range_k,
        .tile_k = tile_k,
    };
    pthreadpool_parallelize(
        threadpool, thread_parallelize_3d_tile_1d_dynamic_with_thread, &params,
        sizeof(params), function, context, tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_3d_tile_1d_dynamic_with_thread)

void PTHREADPOOL_IMPL(
    pthreadpool_parallelize_3d_tile_1d_dynamic_with_uarch_with_thread)(
    pthreadpool_t threadpool,
    pthreadpool_task_3d_tile_1d_dynamic_with_id_with_thread_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t range_k, size_t tile_k,
    uint32_t flags) {
  if (threadpool == NULL || threadpool->threads_count.value <= 1 ||
      (range_i <= 1 && range_j <= 1 && range_k <= tile_k)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t index_i = 0; index_i < range_i; index_i++) {
      for (size_t index_j = 0; index_j < range_j; index_j++) {
        function(context, uarch_index, /*thread_id=*/0, index_i, index_j,
                 /*index_k=*/0, range_k);
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_k = divide_round_up(range_k, tile_k);
    const size_t tile_range = range_i * range_j * tile_range_k;
    const struct pthreadpool_3d_tile_1d_dynamic_with_uarch_params params = {
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
        .range_i = range_i,
        .range_j = range_j,
        .range_k = range_k,
        .tile_k = tile_k,
    };
    pthreadpool_parallelize(
        threadpool,
        thread_parallelize_3d_tile_1d_dynamic_with_uarch_with_thread, &params,
        sizeof(params), function, context, tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(
    pthreadpool_parallelize_3d_tile_1d_dynamic_with_uarch_with_thread)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_3d_tile_2d)(
    pthreadpool_t threadpool, pthreadpool_task_3d_tile_2d_t function,
    void* context, size_t range_i, size_t range_j, size_t range_k,
    size_t tile_j, size_t tile_k, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      (range_i <= 1 && range_j <= tile_j && range_k <= tile_k)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j += tile_j) {
        for (size_t k = 0; k < range_k; k += tile_k) {
          function(context, i, j, k, min(range_j - j, tile_j),
                   min(range_k - k, tile_k));
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range_k = divide_round_up(range_k, tile_k);
    const size_t tile_range = range_i * tile_range_j * tile_range_k;
    const struct pthreadpool_3d_tile_2d_params params = {
        .range_j = range_j,
        .tile_j = tile_j,
        .range_k = range_k,
        .tile_k = tile_k,
        .tile_range_j = fxdiv_init_size_t(tile_range_j),
        .tile_range_k = fxdiv_init_size_t(tile_range_k),
    };
    thread_function_t parallelize_3d_tile_2d = &thread_parallelize_3d_tile_2d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_3d_tile_2d =
          &pthreadpool_thread_parallelize_3d_tile_2d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_3d_tile_2d, &params,
                            sizeof(params), function, context, tile_range,
                            flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_3d_tile_2d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_3d_tile_2d_dynamic)(
    pthreadpool_t threadpool, pthreadpool_task_3d_tile_2d_dynamic_t function,
    void* context, size_t range_i, size_t range_j, size_t range_k,
    size_t tile_j, size_t tile_k, uint32_t flags) {
  if (threadpool == NULL || threadpool->threads_count.value <= 1 ||
      (range_i <= 1 && range_j <= tile_j && range_k <= tile_k)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    if (range_k <= tile_k) {
      for (size_t index_i = 0; index_i < range_i; index_i++) {
        function(context, index_i, /*index_j=*/0, /*index_k=*/0, range_j,
                 range_k);
      }
    } else {
      for (size_t index_i = 0; index_i < range_i; index_i++) {
        for (size_t index_j = 0; index_j < range_j; index_j += tile_j) {
          function(context, index_i, index_j, /*index_k=*/0,
                   min(tile_j, range_j - index_j), range_k);
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range_k = divide_round_up(range_k, tile_k);
    const size_t tile_range = range_i * tile_range_j * tile_range_k;
    const struct pthreadpool_3d_tile_2d_dynamic_params params = {
        .range_i = range_i,
        .range_j = range_j,
        .range_k = range_k,
        .tile_j = tile_j,
        .tile_k = tile_k,
    };
    pthreadpool_parallelize(threadpool, thread_parallelize_3d_tile_2d_dynamic,
                            &params, sizeof(params), function, context,
                            tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_3d_tile_2d_dynamic)

void PTHREADPOOL_IMPL(
    pthreadpool_parallelize_3d_tile_2d_dynamic_with_uarch)(
    pthreadpool_t threadpool,
    pthreadpool_task_3d_tile_2d_dynamic_with_id_t function, void* context,
    uint32_t default_uarch_index, uint32_t max_uarch_index, size_t range_i,
    size_t range_j, size_t range_k, size_t tile_j, size_t tile_k,
    uint32_t flags) {
  if (threadpool == NULL || threadpool->threads_count.value <= 1 ||
      (range_i <= 1 && range_j <= tile_j && range_k <= tile_k)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    if (range_k <= tile_k) {
      for (size_t index_i = 0; index_i < range_i; index_i++) {
        function(context, uarch_index, index_i, /*index_j=*/0, /*index_k=*/0,
                 range_j, range_k);
      }
    } else {
      for (size_t index_i = 0; index_i < range_i; index_i++) {
        for (size_t index_j = 0; index_j < range_j; index_j += tile_j) {
          function(context, uarch_index, index_i, index_j, /*index_k=*/0,
                   min(tile_j, range_j - index_j), range_k);
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range_k = divide_round_up(range_k, tile_k);
    const size_t tile_range = range_i * tile_range_j * tile_range_k;
    const struct pthreadpool_3d_tile_2d_dynamic_with_uarch_params params = {
        .range_i = range_i,
        .range_j = range_j,
        .range_k = range_k,
        .tile_j = tile_j,
        .tile_k = tile_k,
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
    };
    pthreadpool_parallelize(
        threadpool, thread_parallelize_3d_tile_2d_dynamic_with_uarch, &params,
        sizeof(params), function, context, tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_3d_tile_2d_dynamic_with_uarch)

void PTHREADPOOL_IMPL(
    pthreadpool_parallelize_3d_tile_2d_dynamic_with_thread)(
    pthreadpool_t threadpool,
    pthreadpool_task_3d_tile_2d_dynamic_with_id_t function, void* context,
    size_t range_i, size_t range_j, size_t range_k, size_t tile_j,
    size_t tile_k, uint32_t flags) {
  if (threadpool == NULL || threadpool->threads_count.value <= 1 ||
      (range_i <= 1 && range_j <= tile_j && range_k <= tile_k)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    if (range_k <= tile_k) {
      for (size_t index_i = 0; index_i < range_i; index_i++) {
        function(context, /*thread_id=*/0, index_i, /*index_j=*/0,
                 /*index_k=*/0, range_j, range_k);
      }
    } else {
      for (size_t index_i = 0; index_i < range_i; index_i++) {
        for (size_t index_j = 0; index_j < range_j; index_j += tile_j) {
          function(context, /*thread_id=*/0, index_i, index_j, /*index_k=*/0,
                   min(tile_j, range_j - index_j), range_k);
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range_k = divide_round_up(range_k, tile_k);
    const size_t tile_range = range_i * tile_range_j * tile_range_k;
    const struct pthreadpool_3d_tile_2d_dynamic_params params = {
        .range_i = range_i,
        .range_j = range_j,
        .range_k = range_k,
        .tile_j = tile_j,
        .tile_k = tile_k,
    };
    pthreadpool_parallelize(
        threadpool, thread_parallelize_3d_tile_2d_dynamic_with_thread, &params,
        sizeof(params), function, context, tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_3d_tile_2d_dynamic_with_thread)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_3d_tile_2d_with_uarch)(
    pthreadpool_t threadpool, pthreadpool_task_3d_tile_2d_with_id_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t range_k, size_t tile_j,
    size_t tile_k, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      (range_i <= 1 && range_j <= tile_j && range_k <= tile_k)) {
    /* No thread pool used: execute task sequentially on the calling thread */

    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j += tile_j) {
        for (size_t k = 0; k < range_k; k += tile_k) {
          function(context, uarch_index, i, j, k, min(range_j - j, tile_j),
                   min(range_k - k, tile_k));
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_j = divide_round_up(range_j, tile_j);
    const size_t tile_range_k = divide_round_up(range_k, tile_k);
    const size_t tile_range = range_i * tile_range_j * tile_range_k;
    const struct pthreadpool_3d_tile_2d_with_uarch_params params = {
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
        .range_j = range_j,
        .tile_j = tile_j,
        .range_k = range_k,
        .tile_k = tile_k,
        .tile_range_j = fxdiv_init_size_t(tile_range_j),
        .tile_range_k = fxdiv_init_size_t(tile_range_k),
    };
    thread_function_t parallelize_3d_tile_2d_with_uarch =
        &thread_parallelize_3d_tile_2d_with_uarch;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_3d_tile_2d_with_uarch =
          &pthreadpool_thread_parallelize_3d_tile_2d_with_uarch_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_3d_tile_2d_with_uarch,
                            &params, sizeof(params), function, context,
                            tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_3d_tile_2d_with_uarch)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_4d)(
    pthreadpool_t threadpool, pthreadpool_task_4d_t function, void* context,
    size_t range_i, size_t range_j, size_t range_k, size_t range_l,
    uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      (range_i | range_j | range_k | range_l) <= 1) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k++) {
          for (size_t l = 0; l < range_l; l++) {
            function(context, i, j, k, l);
          }
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t range_kl = range_k * range_l;
    const size_t range = range_i * range_j * range_kl;
    const struct pthreadpool_4d_params params = {
        .range_k = range_k,
        .range_j = fxdiv_init_size_t(range_j),
        .range_kl = fxdiv_init_size_t(range_kl),
        .range_l = fxdiv_init_size_t(range_l),
    };
    thread_function_t parallelize_4d = &thread_parallelize_4d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (range < range_threshold) {
      parallelize_4d = &pthreadpool_thread_parallelize_4d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_4d, &params, sizeof(params),
                            function, context, range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_4d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_4d_tile_1d)(
    pthreadpool_t threadpool, pthreadpool_task_4d_tile_1d_t function,
    void* context, size_t range_i, size_t range_j, size_t range_k,
    size_t range_l, size_t tile_l, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      ((range_i | range_j | range_k) <= 1 && range_l <= tile_l)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k++) {
          for (size_t l = 0; l < range_l; l += tile_l) {
            function(context, i, j, k, l, min(range_l - l, tile_l));
          }
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_l = divide_round_up(range_l, tile_l);
    const size_t tile_range_kl = range_k * tile_range_l;
    const size_t tile_range = range_i * range_j * tile_range_kl;
    const struct pthreadpool_4d_tile_1d_params params = {
        .range_k = range_k,
        .range_l = range_l,
        .tile_l = tile_l,
        .range_j = fxdiv_init_size_t(range_j),
        .tile_range_kl = fxdiv_init_size_t(tile_range_kl),
        .tile_range_l = fxdiv_init_size_t(tile_range_l),
    };
    thread_function_t parallelize_4d_tile_1d = &thread_parallelize_4d_tile_1d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_4d_tile_1d =
          &pthreadpool_thread_parallelize_4d_tile_1d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_4d_tile_1d, &params,
                            sizeof(params), function, context, tile_range,
                            flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_4d_tile_1d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_4d_tile_2d)(
    pthreadpool_t threadpool, pthreadpool_task_4d_tile_2d_t function,
    void* context, size_t range_i, size_t range_j, size_t range_k,
    size_t range_l, size_t tile_k, size_t tile_l, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      ((range_i | range_j) <= 1 && range_k <= tile_k && range_l <= tile_l)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k += tile_k) {
          for (size_t l = 0; l < range_l; l += tile_l) {
            function(context, i, j, k, l, min(range_k - k, tile_k),
                     min(range_l - l, tile_l));
          }
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_l = divide_round_up(range_l, tile_l);
    const size_t tile_range_kl =
        divide_round_up(range_k, tile_k) * tile_range_l;
    const size_t tile_range = range_i * range_j * tile_range_kl;
    const struct pthreadpool_4d_tile_2d_params params = {
        .range_k = range_k,
        .tile_k = tile_k,
        .range_l = range_l,
        .tile_l = tile_l,
        .range_j = fxdiv_init_size_t(range_j),
        .tile_range_kl = fxdiv_init_size_t(tile_range_kl),
        .tile_range_l = fxdiv_init_size_t(tile_range_l),
    };
    thread_function_t parallelize_4d_tile_2d = &thread_parallelize_4d_tile_2d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_4d_tile_2d =
          &pthreadpool_thread_parallelize_4d_tile_2d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_4d_tile_2d, &params,
                            sizeof(params), function, context, tile_range,
                            flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_4d_tile_2d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_4d_tile_2d_with_uarch)(
    pthreadpool_t threadpool, pthreadpool_task_4d_tile_2d_with_id_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t range_k, size_t range_l,
    size_t tile_k, size_t tile_l, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      ((range_i | range_j) <= 1 && range_k <= tile_k && range_l <= tile_l)) {
    /* No thread pool used: execute task sequentially on the calling thread */

    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k += tile_k) {
          for (size_t l = 0; l < range_l; l += tile_l) {
            function(context, uarch_index, i, j, k, l, min(range_k - k, tile_k),
                     min(range_l - l, tile_l));
          }
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_l = divide_round_up(range_l, tile_l);
    const size_t tile_range_kl =
        divide_round_up(range_k, tile_k) * tile_range_l;
    const size_t tile_range = range_i * range_j * tile_range_kl;
    const struct pthreadpool_4d_tile_2d_with_uarch_params params = {
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
        .range_k = range_k,
        .tile_k = tile_k,
        .range_l = range_l,
        .tile_l = tile_l,
        .range_j = fxdiv_init_size_t(range_j),
        .tile_range_kl = fxdiv_init_size_t(tile_range_kl),
        .tile_range_l = fxdiv_init_size_t(tile_range_l),
    };
    thread_function_t parallelize_4d_tile_2d_with_uarch =
        &thread_parallelize_4d_tile_2d_with_uarch;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_4d_tile_2d_with_uarch =
          &pthreadpool_thread_parallelize_4d_tile_2d_with_uarch_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_4d_tile_2d_with_uarch,
                            &params, sizeof(params), function, context,
                            tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_4d_tile_2d_with_uarch)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_4d_tile_2d_dynamic)(
    pthreadpool_t threadpool, pthreadpool_task_4d_tile_2d_dynamic_t function,
    void* context, size_t range_i, size_t range_j, size_t range_k,
    size_t range_l, size_t tile_k, size_t tile_l, uint32_t flags) {
  if (threadpool == NULL || threadpool->threads_count.value <= 1 ||
      (range_i <= 1 && range_j <= 1 && range_k <= tile_k &&
       range_l <= tile_l)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    if (range_l <= tile_l) {
      for (size_t index_i = 0; index_i < range_i; index_i++) {
        for (size_t index_j = 0; index_j < range_j; index_j++) {
          function(context, index_i, index_j, /*index_k=*/0, /*index_l=*/0,
                   range_k, range_l);
        }
      }
    } else {
      for (size_t index_i = 0; index_i < range_i; index_i++) {
        for (size_t index_j = 0; index_j < range_j; index_j++) {
          for (size_t index_k = 0; index_k < range_k; index_k += tile_k) {
            function(context, index_i, index_j, index_k, /*index_l=*/0,
                     min(tile_k, range_k - index_k), range_l);
          }
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_k = divide_round_up(range_k, tile_k);
    const size_t tile_range_l = divide_round_up(range_l, tile_l);
    const size_t tile_range = range_i * range_j * tile_range_k * tile_range_l;
    const struct pthreadpool_4d_tile_2d_dynamic_params params = {
        .range_i = range_i,
        .range_j = range_j,
        .range_k = range_k,
        .range_l = range_l,
        .tile_k = tile_k,
        .tile_l = tile_l,
    };
    pthreadpool_parallelize(threadpool, thread_parallelize_4d_tile_2d_dynamic,
                            &params, sizeof(params), function, context,
                            tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_4d_tile_2d_dynamic)

void PTHREADPOOL_IMPL(
    pthreadpool_parallelize_4d_tile_2d_dynamic_with_uarch)(
    pthreadpool_t threadpool,
    pthreadpool_task_4d_tile_2d_dynamic_with_id_t function, void* context,
    uint32_t default_uarch_index, uint32_t max_uarch_index, size_t range_i,
    size_t range_j, size_t range_k, size_t range_l, size_t tile_k,
    size_t tile_l, uint32_t flags) {
  if (threadpool == NULL || threadpool->threads_count.value <= 1 ||
      (range_i <= 1 && range_j <= 1 && range_k <= tile_k &&
       range_l <= tile_l)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    uint32_t uarch_index = default_uarch_index;
#if PTHREADPOOL_USE_CPUINFO
    uarch_index =
        cpuinfo_get_current_uarch_index_with_default(default_uarch_index);
    if (uarch_index > max_uarch_index) {
      uarch_index = default_uarch_index;
    }
#endif

    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    if (range_l <= tile_l) {
      for (size_t index_i = 0; index_i < range_i; index_i++) {
        for (size_t index_j = 0; index_j < range_j; index_j++) {
          function(context, uarch_index, index_i, index_j, /*index_k=*/0,
                   /*index_l=*/0, range_k, range_l);
        }
      }
    } else {
      for (size_t index_i = 0; index_i < range_i; index_i++) {
        for (size_t index_j = 0; index_j < range_j; index_j++) {
          for (size_t index_k = 0; index_k < range_k; index_k += tile_k) {
            function(context, uarch_index, index_i, index_j, index_k,
                     /*index_l=*/0, min(tile_k, range_k - index_k), range_l);
          }
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_k = divide_round_up(range_k, tile_k);
    const size_t tile_range_l = divide_round_up(range_l, tile_l);
    const size_t tile_range = range_i * range_j * tile_range_k * tile_range_l;
    const struct pthreadpool_4d_tile_2d_dynamic_with_uarch_params params = {
        .range_i = range_i,
        .range_j = range_j,
        .range_k = range_k,
        .range_l = range_l,
        .tile_k = tile_k,
        .tile_l = tile_l,
        .default_uarch_index = default_uarch_index,
        .max_uarch_index = max_uarch_index,
    };
    pthreadpool_parallelize(
        threadpool, thread_parallelize_4d_tile_2d_dynamic_with_uarch, &params,
        sizeof(params), function, context, tile_range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_4d_tile_2d_dynamic_with_uarch)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_5d)(
    pthreadpool_t threadpool, pthreadpool_task_5d_t function, void* context,
    size_t range_i, size_t range_j, size_t range_k, size_t range_l,
    size_t range_m, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      (range_i | range_j | range_k | range_l | range_m) <= 1) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k++) {
          for (size_t l = 0; l < range_l; l++) {
            for (size_t m = 0; m < range_m; m++) {
              function(context, i, j, k, l, m);
            }
          }
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t range_lm = range_l * range_m;
    const size_t range = range_i * range_j * range_k * range_lm;
    const struct pthreadpool_5d_params params = {
        .range_l = range_l,
        .range_j = fxdiv_init_size_t(range_j),
        .range_k = fxdiv_init_size_t(range_k),
        .range_lm = fxdiv_init_size_t(range_lm),
        .range_m = fxdiv_init_size_t(range_m),
    };
    thread_function_t parallelize_5d = &thread_parallelize_5d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (range < range_threshold) {
      parallelize_5d = &pthreadpool_thread_parallelize_5d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_5d, &params, sizeof(params),
                            function, context, range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_5d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_5d_tile_1d)(
    pthreadpool_t threadpool, pthreadpool_task_5d_tile_1d_t function,
    void* context, size_t range_i, size_t range_j, size_t range_k,
    size_t range_l, size_t range_m, size_t tile_m, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      ((range_i | range_j | range_k | range_l) <= 1 && range_m <= tile_m)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k++) {
          for (size_t l = 0; l < range_l; l++) {
            for (size_t m = 0; m < range_m; m += tile_m) {
              function(context, i, j, k, l, m, min(range_m - m, tile_m));
            }
          }
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_m = divide_round_up(range_m, tile_m);
    const size_t range_kl = range_k * range_l;
    const size_t tile_range = range_i * range_j * range_kl * tile_range_m;
    const struct pthreadpool_5d_tile_1d_params params = {
        .range_k = range_k,
        .range_m = range_m,
        .tile_m = tile_m,
        .range_j = fxdiv_init_size_t(range_j),
        .range_kl = fxdiv_init_size_t(range_kl),
        .range_l = fxdiv_init_size_t(range_l),
        .tile_range_m = fxdiv_init_size_t(tile_range_m),
    };
    thread_function_t parallelize_5d_tile_1d = &thread_parallelize_5d_tile_1d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_5d_tile_1d =
          &pthreadpool_thread_parallelize_5d_tile_1d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_5d_tile_1d, &params,
                            sizeof(params), function, context, tile_range,
                            flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_5d_tile_1d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_5d_tile_2d)(
    pthreadpool_t threadpool, pthreadpool_task_5d_tile_2d_t function,
    void* context, size_t range_i, size_t range_j, size_t range_k,
    size_t range_l, size_t range_m, size_t tile_l, size_t tile_m,
    uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      ((range_i | range_j | range_k) <= 1 && range_l <= tile_l &&
       range_m <= tile_m)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k++) {
          for (size_t l = 0; l < range_l; l += tile_l) {
            for (size_t m = 0; m < range_m; m += tile_m) {
              function(context, i, j, k, l, m, min(range_l - l, tile_l),
                       min(range_m - m, tile_m));
            }
          }
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_m = divide_round_up(range_m, tile_m);
    const size_t tile_range_lm =
        divide_round_up(range_l, tile_l) * tile_range_m;
    const size_t tile_range = range_i * range_j * range_k * tile_range_lm;
    const struct pthreadpool_5d_tile_2d_params params = {
        .range_l = range_l,
        .tile_l = tile_l,
        .range_m = range_m,
        .tile_m = tile_m,
        .range_j = fxdiv_init_size_t(range_j),
        .range_k = fxdiv_init_size_t(range_k),
        .tile_range_lm = fxdiv_init_size_t(tile_range_lm),
        .tile_range_m = fxdiv_init_size_t(tile_range_m),
    };
    thread_function_t parallelize_5d_tile_2d = &thread_parallelize_5d_tile_2d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_5d_tile_2d =
          &pthreadpool_thread_parallelize_5d_tile_2d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_5d_tile_2d, &params,
                            sizeof(params), function, context, tile_range,
                            flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_5d_tile_2d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_6d)(
    pthreadpool_t threadpool, pthreadpool_task_6d_t function, void* context,
    size_t range_i, size_t range_j, size_t range_k, size_t range_l,
    size_t range_m, size_t range_n, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      (range_i | range_j | range_k | range_l | range_m | range_n) <= 1) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k++) {
          for (size_t l = 0; l < range_l; l++) {
            for (size_t m = 0; m < range_m; m++) {
              for (size_t n = 0; n < range_n; n++) {
                function(context, i, j, k, l, m, n);
              }
            }
          }
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t range_lmn = range_l * range_m * range_n;
    const size_t range = range_i * range_j * range_k * range_lmn;
    const struct pthreadpool_6d_params params = {
        .range_l = range_l,
        .range_j = fxdiv_init_size_t(range_j),
        .range_k = fxdiv_init_size_t(range_k),
        .range_lmn = fxdiv_init_size_t(range_lmn),
        .range_m = fxdiv_init_size_t(range_m),
        .range_n = fxdiv_init_size_t(range_n),
    };
    thread_function_t parallelize_6d = &thread_parallelize_6d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (range < range_threshold) {
      parallelize_6d = &pthreadpool_thread_parallelize_6d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_6d, &params, sizeof(params),
                            function, context, range, flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_6d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_6d_tile_1d)(
    pthreadpool_t threadpool, pthreadpool_task_6d_tile_1d_t function,
    void* context, size_t range_i, size_t range_j, size_t range_k,
    size_t range_l, size_t range_m, size_t range_n, size_t tile_n,
    uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      ((range_i | range_j | range_k | range_l | range_m) <= 1 &&
       range_n <= tile_n)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k++) {
          for (size_t l = 0; l < range_l; l++) {
            for (size_t m = 0; m < range_m; m++) {
              for (size_t n = 0; n < range_n; n += tile_n) {
                function(context, i, j, k, l, m, n, min(range_n - n, tile_n));
              }
            }
          }
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t tile_range_n = divide_round_up(range_n, tile_n);
    const size_t tile_range_lmn = range_l * range_m * tile_range_n;
    const size_t tile_range = range_i * range_j * range_k * tile_range_lmn;
    const struct pthreadpool_6d_tile_1d_params params = {
        .range_l = range_l,
        .range_n = range_n,
        .tile_n = tile_n,
        .range_j = fxdiv_init_size_t(range_j),
        .range_k = fxdiv_init_size_t(range_k),
        .tile_range_lmn = fxdiv_init_size_t(tile_range_lmn),
        .range_m = fxdiv_init_size_t(range_m),
        .tile_range_n = fxdiv_init_size_t(tile_range_n),
    };
    thread_function_t parallelize_6d_tile_1d = &thread_parallelize_6d_tile_1d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_6d_tile_1d =
          &pthreadpool_thread_parallelize_6d_tile_1d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_6d_tile_1d, &params,
                            sizeof(params), function, context, tile_range,
                            flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_6d_tile_1d)

void PTHREADPOOL_IMPL(pthreadpool_parallelize_6d_tile_2d)(
    pthreadpool_t threadpool, pthreadpool_task_6d_tile_2d_t function,
    void* context, size_t range_i, size_t range_j, size_t range_k,
    size_t range_l, size_t range_m, size_t range_n, size_t tile_m,
    size_t tile_n, uint32_t flags) {
  size_t threads_count;
  if (threadpool == NULL ||
      (threads_count = threadpool->threads_count.value) <= 1 ||
      ((range_i | range_j | range_k | range_l) <= 1 && range_m <= tile_m &&
       range_n <= tile_n)) {
    /* No thread pool used: execute task sequentially on the calling thread */
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }
    for (size_t i = 0; i < range_i; i++) {
      for (size_t j = 0; j < range_j; j++) {
        for (size_t k = 0; k < range_k; k++) {
          for (size_t l = 0; l < range_l; l++) {
            for (size_t m = 0; m < range_m; m += tile_m) {
              for (size_t n = 0; n < range_n; n += tile_n) {
                function(context, i, j, k, l, m, n, min(range_m - m, tile_m),
                         min(range_n - n, tile_n));
              }
            }
          }
        }
      }
    }
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  } else {
    const size_t range_kl = range_k * range_l;
    const size_t tile_range_n = divide_round_up(range_n, tile_n);
    const size_t tile_range_mn =
        divide_round_up(range_m, tile_m) * tile_range_n;
    const size_t tile_range = range_i * range_j * range_kl * tile_range_mn;
    const struct pthreadpool_6d_tile_2d_params params = {
        .range_k = range_k,
        .range_m = range_m,
        .tile_m = tile_m,
        .range_n = range_n,
        .tile_n = tile_n,
        .range_j = fxdiv_init_size_t(range_j),
        .range_kl = fxdiv_init_size_t(range_kl),
        .range_l = fxdiv_init_size_t(range_l),
        .tile_range_mn = fxdiv_init_size_t(tile_range_mn),
        .tile_range_n = fxdiv_init_size_t(tile_range_n),
    };
    thread_function_t parallelize_6d_tile_2d = &thread_parallelize_6d_tile_2d;
#if PTHREADPOOL_USE_FASTPATH
    const size_t range_threshold = -threads_count;
    if (tile_range < range_threshold) {
      parallelize_6d_tile_2d =
          &pthreadpool_thread_parallelize_6d_tile_2d_fastpath;
    }
#endif
    pthreadpool_parallelize(threadpool, parallelize_6d_tile_2d, &params,
                            sizeof(params), function, context, tile_range,
                            flags);
  }
}

PTHREADPOOL_WEAK_ALIAS(pthreadpool_parallelize_6d_tile_2d)
