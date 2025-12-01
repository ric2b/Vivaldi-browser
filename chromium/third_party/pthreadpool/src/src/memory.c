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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* POSIX headers */
#ifdef __ANDROID__
#include <malloc.h>
#endif

/* Windows headers */
#ifdef _WIN32
#include <malloc.h>
#endif

/* Internal library headers */
#include "threadpool-common.h"
#include "threadpool-object.h"

#if defined(__has_builtin)
#if __has_builtin(__builtin_available)
#define PTHREADPOOL_BUILTIN_AVAILABLE
#endif
#endif

extern int posix_memalign(void **memptr, size_t alignment, size_t size);

PTHREADPOOL_INTERNAL struct pthreadpool* pthreadpool_allocate(
    size_t threads_count) {
  assert(threads_count >= 1);

  const size_t threadpool_size =
      sizeof(struct pthreadpool) + threads_count * sizeof(struct thread_info);
  struct pthreadpool* threadpool = NULL;
#if defined(__ANDROID__)
  /*
   * Android didn't get posix_memalign until API level 17 (Android 4.2).
   * Use (otherwise obsolete) memalign function on Android platform.
   */
  threadpool = memalign(PTHREADPOOL_CACHELINE_SIZE, threadpool_size);
#elif defined(_WIN32)
  threadpool = _aligned_malloc(threadpool_size, PTHREADPOOL_CACHELINE_SIZE);
#elif _POSIX_C_SOURCE >= 200112L || defined(__hexagon__)
  if (posix_memalign((void**)&threadpool, PTHREADPOOL_CACHELINE_SIZE,
                     threadpool_size) != 0) {
    return NULL;
  }
#elif defined(PTHREADPOOL_BUILTIN_AVAILABLE)
  if (__builtin_available(macOS 10.15, iOS 13, *)) {
    threadpool = aligned_alloc(PTHREADPOOL_CACHELINE_SIZE, threadpool_size);
  } else {
    threadpool = malloc(threadpool_size);
  }
#else
  threadpool = aligned_alloc(PTHREADPOOL_CACHELINE_SIZE, threadpool_size);
#endif
  if (threadpool == NULL) {
    return NULL;
  }
  memset(threadpool, 0, threadpool_size);
  return threadpool;
}

PTHREADPOOL_INTERNAL void pthreadpool_deallocate(
    struct pthreadpool* threadpool) {
  assert(threadpool != NULL);

#ifdef _WIN32
  _aligned_free(threadpool);
#else
  free(threadpool);
#endif
}
