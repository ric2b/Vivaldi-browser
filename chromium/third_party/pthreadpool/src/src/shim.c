// Copyright (c) 2017 Facebook Inc.
// Copyright (c) 2015-2017 Georgia Institute of Technology
// All rights reserved.
//
// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

/* Standard C headers */
#include <stddef.h>
#include <stdint.h>

/* Public library header */
#include <pthreadpool.h>

/* Internal library headers */
#include "threadpool-common.h"
#include "threadpool-utils.h"

struct pthreadpool {};

static const struct pthreadpool static_pthreadpool = {};

struct pthreadpool* pthreadpool_create(size_t threads_count) {
  if (threads_count <= 1) {
    return (struct pthreadpool*)&static_pthreadpool;
  }

  return NULL;
}

struct pthreadpool* pthreadpool_create_v2(struct pthreadpool_executor* executor,
                                          void* executor_context,
                                          size_t max_num_threads) {
  return NULL;
}

size_t pthreadpool_get_threads_count(struct pthreadpool* threadpool) {
  return 1;
}

size_t pthreadpool_set_threads_count(struct pthreadpool* threadpool,
                                     size_t num_threads) {
  return 1;
}

void pthreadpool_release_executor_threads(struct pthreadpool* threadpool) {}

void pthreadpool_parallelize_1d(struct pthreadpool* threadpool,
                                pthreadpool_task_1d_t function, void* context,
                                size_t range, uint32_t flags) {
  for (size_t i = 0; i < range; i++) {
    function(context, i);
  }
}

void pthreadpool_parallelize_1d_with_thread(
    struct pthreadpool* threadpool, pthreadpool_task_1d_with_thread_t function,
    void* context, size_t range, uint32_t flags) {
  for (size_t i = 0; i < range; i++) {
    function(context, 0, i);
  }
}

void pthreadpool_parallelize_1d_with_uarch(
    pthreadpool_t threadpool, pthreadpool_task_1d_with_id_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range, uint32_t flags) {
  for (size_t i = 0; i < range; i++) {
    function(context, default_uarch_index, i);
  }
}

void pthreadpool_parallelize_1d_tile_1d(pthreadpool_t threadpool,
                                        pthreadpool_task_1d_tile_1d_t function,
                                        void* context, size_t range,
                                        size_t tile, uint32_t flags) {
  for (size_t i = 0; i < range; i += tile) {
    function(context, i, min(range - i, tile));
  }
}

void pthreadpool_parallelize_1d_tile_1d_dynamic(
    pthreadpool_t threadpool, pthreadpool_task_1d_tile_1d_dynamic_t function,
    void* context, size_t range, size_t tile, uint32_t flags) {
  function(context, 0, range);
}

void pthreadpool_parallelize_1d_tile_1d_dynamic_with_thread(
    pthreadpool_t threadpool,
    pthreadpool_task_1d_tile_1d_dynamic_with_id_t function, void* context,
    size_t range, size_t tile, uint32_t flags) {
  function(context, /*thread_id=*/0, 0, range);
}

void pthreadpool_parallelize_1d_tile_1d_dynamic_with_uarch_with_thread(
    pthreadpool_t threadpool,
    pthreadpool_task_1d_tile_1d_dynamic_with_id_with_thread_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range, size_t tile, uint32_t flags) {
  function(context, default_uarch_index, /*thread_id=*/0, 0, range);
}

void pthreadpool_parallelize_2d(struct pthreadpool* threadpool,
                                pthreadpool_task_2d_t function, void* context,
                                size_t range_i, size_t range_j,
                                uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j++) {
      function(context, i, j);
    }
  }
}

void pthreadpool_parallelize_2d_with_thread(
    struct pthreadpool* threadpool, pthreadpool_task_2d_with_thread_t function,
    void* context, size_t range_i, size_t range_j, uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j++) {
      function(context, 0, i, j);
    }
  }
}

void pthreadpool_parallelize_2d_tile_1d(pthreadpool_t threadpool,
                                        pthreadpool_task_2d_tile_1d_t function,
                                        void* context, size_t range_i,
                                        size_t range_j, size_t tile_j,
                                        uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j += tile_j) {
      function(context, i, j, min(range_j - j, tile_j));
    }
  }
}

void pthreadpool_parallelize_2d_tile_1d_with_uarch(
    pthreadpool_t threadpool, pthreadpool_task_2d_tile_1d_with_id_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t tile_j, uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j += tile_j) {
      function(context, default_uarch_index, i, j, min(range_j - j, tile_j));
    }
  }
}

void pthreadpool_parallelize_2d_tile_1d_with_uarch_with_thread(
    pthreadpool_t threadpool,
    pthreadpool_task_2d_tile_1d_with_id_with_thread_t function, void* context,
    uint32_t default_uarch_index, uint32_t max_uarch_index, size_t range_i,
    size_t range_j, size_t tile_j, uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j += tile_j) {
      function(context, default_uarch_index, 0, i, j, min(range_j - j, tile_j));
    }
  }
}

void pthreadpool_parallelize_2d_tile_1d_dynamic(
    pthreadpool_t threadpool, pthreadpool_task_2d_tile_1d_dynamic_t function,
    void* context, size_t range_i, size_t range_j, size_t tile_j,
    uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    function(context, i, 0, range_j);
  }
}

void pthreadpool_parallelize_2d_tile_1d_dynamic_with_thread(
    pthreadpool_t threadpool,
    pthreadpool_task_2d_tile_1d_dynamic_with_id_t function, void* context,
    size_t range_i, size_t range_j, size_t tile_j, uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    function(context, /*thread_id=*/0, i, 0, range_j);
  }
}

void pthreadpool_parallelize_2d_tile_1d_dynamic_with_uarch_with_thread(
    pthreadpool_t threadpool,
    pthreadpool_task_2d_tile_1d_dynamic_with_id_with_thread_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t tile_j, uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    function(context, default_uarch_index, /*thread_id=*/0, i, 0, range_j);
  }
}

void pthreadpool_parallelize_2d_tile_2d(pthreadpool_t threadpool,
                                        pthreadpool_task_2d_tile_2d_t function,
                                        void* context, size_t range_i,
                                        size_t range_j, size_t tile_i,
                                        size_t tile_j, uint32_t flags) {
  for (size_t i = 0; i < range_i; i += tile_i) {
    for (size_t j = 0; j < range_j; j += tile_j) {
      function(context, i, j, min(range_i - i, tile_i),
               min(range_j - j, tile_j));
    }
  }
}

void pthreadpool_parallelize_2d_tile_2d_with_uarch(
    pthreadpool_t threadpool, pthreadpool_task_2d_tile_2d_with_id_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t tile_i, size_t tile_j,
    uint32_t flags) {
  for (size_t i = 0; i < range_i; i += tile_i) {
    for (size_t j = 0; j < range_j; j += tile_j) {
      function(context, default_uarch_index, i, j, min(range_i - i, tile_i),
               min(range_j - j, tile_j));
    }
  }
}

void pthreadpool_parallelize_2d_tile_2d_dynamic(
    pthreadpool_t threadpool, pthreadpool_task_2d_tile_2d_dynamic_t function,
    void* context, size_t range_i, size_t range_j, size_t tile_i, size_t tile_j,
    uint32_t flags) {
  if (range_j <= tile_j) {
    function(context, /*index_i=*/0, /*index_j=*/0, range_i, range_j);
  } else {
    for (size_t index_i = 0; index_i < range_i; index_i += tile_i) {
      function(context, index_i, /*index_j=*/0, min(tile_i, range_i - index_i),
               range_j);
    }
  }
}

void pthreadpool_parallelize_2d_tile_2d_dynamic_with_uarch(
    pthreadpool_t threadpool,
    pthreadpool_task_2d_tile_2d_dynamic_with_id_t function, void* context,
    uint32_t default_uarch_index, uint32_t max_uarch_index, size_t range_i,
    size_t range_j, size_t tile_i, size_t tile_j, uint32_t flags) {
  if (range_j <= tile_j) {
    function(context, default_uarch_index, /*index_i=*/0, /*index_j=*/0,
             range_i, range_j);
  } else {
    for (size_t index_i = 0; index_i < range_i; index_i += tile_i) {
      function(context, default_uarch_index, index_i, /*index_j=*/0,
               min(tile_i, range_i - index_i), range_j);
    }
  }
}

void pthreadpool_parallelize_2d_tile_2d_dynamic_with_thread(
    pthreadpool_t threadpool,
    pthreadpool_task_2d_tile_2d_dynamic_with_id_t function, void* context,
    size_t range_i, size_t range_j, size_t tile_i, size_t tile_j,
    uint32_t flags) {
  if (range_j <= tile_j) {
    function(context, /*thread_id=*/0, /*index_i=*/0, /*index_j=*/0, range_i,
             range_j);
  } else {
    for (size_t index_i = 0; index_i < range_i; index_i += tile_i) {
      function(context, /*thread_id=*/0, index_i, /*index_j=*/0,
               min(tile_i, range_i - index_i), range_j);
    }
  }
}

void pthreadpool_parallelize_3d(pthreadpool_t threadpool,
                                pthreadpool_task_3d_t function, void* context,
                                size_t range_i, size_t range_j, size_t range_k,
                                uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j++) {
      for (size_t k = 0; k < range_k; k++) {
        function(context, i, j, k);
      }
    }
  }
}

void pthreadpool_parallelize_3d_tile_1d(pthreadpool_t threadpool,
                                        pthreadpool_task_3d_tile_1d_t function,
                                        void* context, size_t range_i,
                                        size_t range_j, size_t range_k,
                                        size_t tile_k, uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j++) {
      for (size_t k = 0; k < range_k; k += tile_k) {
        function(context, i, j, k, min(range_k - k, tile_k));
      }
    }
  }
}

void pthreadpool_parallelize_3d_tile_1d_with_thread(
    pthreadpool_t threadpool,
    pthreadpool_task_3d_tile_1d_with_thread_t function, void* context,
    size_t range_i, size_t range_j, size_t range_k, size_t tile_k,
    uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j++) {
      for (size_t k = 0; k < range_k; k += tile_k) {
        function(context, 0, i, j, k, min(range_k - k, tile_k));
      }
    }
  }
}

void pthreadpool_parallelize_3d_tile_1d_with_uarch(
    pthreadpool_t threadpool, pthreadpool_task_3d_tile_1d_with_id_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t range_k, size_t tile_k,
    uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j++) {
      for (size_t k = 0; k < range_k; k += tile_k) {
        function(context, default_uarch_index, i, j, k,
                 min(range_k - k, tile_k));
      }
    }
  }
}

void pthreadpool_parallelize_3d_tile_1d_with_uarch_with_thread(
    pthreadpool_t threadpool,
    pthreadpool_task_3d_tile_1d_with_id_with_thread_t function, void* context,
    uint32_t default_uarch_index, uint32_t max_uarch_index, size_t range_i,
    size_t range_j, size_t range_k, size_t tile_k, uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j++) {
      for (size_t k = 0; k < range_k; k += tile_k) {
        function(context, default_uarch_index, 0, i, j, k,
                 min(range_k - k, tile_k));
      }
    }
  }
}

void pthreadpool_parallelize_3d_tile_2d(pthreadpool_t threadpool,
                                        pthreadpool_task_3d_tile_2d_t function,
                                        void* context, size_t range_i,
                                        size_t range_j, size_t range_k,
                                        size_t tile_j, size_t tile_k,
                                        uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j += tile_j) {
      for (size_t k = 0; k < range_k; k += tile_k) {
        function(context, i, j, k, min(range_j - j, tile_j),
                 min(range_k - k, tile_k));
      }
    }
  }
}

void pthreadpool_parallelize_3d_tile_1d_dynamic(
    pthreadpool_t threadpool, pthreadpool_task_3d_tile_1d_dynamic_t function,
    void* context, size_t range_i, size_t range_j, size_t range_k,
    size_t tile_k, uint32_t flags) {
  for (size_t index_i = 0; index_i < range_i; index_i++) {
    for (size_t index_j = 0; index_j < range_j; index_j++) {
      function(context, index_i, index_j, /*index_k=*/0, range_k);
    }
  }
}

void pthreadpool_parallelize_3d_tile_1d_dynamic_with_thread(
    pthreadpool_t threadpool,
    pthreadpool_task_3d_tile_1d_dynamic_with_id_t function, void* context,
    size_t range_i, size_t range_j, size_t range_k, size_t tile_k,
    uint32_t flags) {
  for (size_t index_i = 0; index_i < range_i; index_i++) {
    for (size_t index_j = 0; index_j < range_j; index_j++) {
      function(context, /*thread_id=*/0, index_i, index_j, /*index_k=*/0,
               range_k);
    }
  }
}

void pthreadpool_parallelize_3d_tile_1d_dynamic_with_uarch_with_thread(
    pthreadpool_t threadpool,
    pthreadpool_task_3d_tile_1d_dynamic_with_id_with_thread_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t range_k, size_t tile_k,
    uint32_t flags) {
  for (size_t index_i = 0; index_i < range_i; index_i++) {
    for (size_t index_j = 0; index_j < range_j; index_j++) {
      function(context, default_uarch_index, /*thread_id=*/0, index_i, index_j,
               /*index_k=*/0, range_k);
    }
  }
}

void pthreadpool_parallelize_3d_tile_2d_with_uarch(
    pthreadpool_t threadpool, pthreadpool_task_3d_tile_2d_with_id_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t range_k, size_t tile_j,
    size_t tile_k, uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j += tile_j) {
      for (size_t k = 0; k < range_k; k += tile_k) {
        function(context, default_uarch_index, i, j, k,
                 min(range_j - j, tile_j), min(range_k - k, tile_k));
      }
    }
  }
}

void pthreadpool_parallelize_3d_tile_2d_dynamic(
    pthreadpool_t threadpool, pthreadpool_task_3d_tile_2d_dynamic_t function,
    void* context, size_t range_i, size_t range_j, size_t range_k,
    size_t tile_j, size_t tile_k, uint32_t flags) {
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
}

void pthreadpool_parallelize_3d_tile_2d_dynamic_with_thread(
    pthreadpool_t threadpool,
    pthreadpool_task_3d_tile_2d_dynamic_with_id_t function, void* context,
    size_t range_i, size_t range_j, size_t range_k, size_t tile_j,
    size_t tile_k, uint32_t flags) {
  if (range_k <= tile_k) {
    for (size_t index_i = 0; index_i < range_i; index_i++) {
      function(context, /*thread_id=*/0, index_i, /*index_j=*/0, /*index_k=*/0,
               range_j, range_k);
    }
  } else {
    for (size_t index_i = 0; index_i < range_i; index_i++) {
      for (size_t index_j = 0; index_j < range_j; index_j += tile_j) {
        function(context, /*thread_id=*/0, index_i, index_j, /*index_k=*/0,
                 min(tile_j, range_j - index_j), range_k);
      }
    }
  }
}

void pthreadpool_parallelize_4d(pthreadpool_t threadpool,
                                pthreadpool_task_4d_t function, void* context,
                                size_t range_i, size_t range_j, size_t range_k,
                                size_t range_l, uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j++) {
      for (size_t k = 0; k < range_k; k++) {
        for (size_t l = 0; l < range_l; l++) {
          function(context, i, j, k, l);
        }
      }
    }
  }
}

void pthreadpool_parallelize_4d_tile_1d(pthreadpool_t threadpool,
                                        pthreadpool_task_4d_tile_1d_t function,
                                        void* context, size_t range_i,
                                        size_t range_j, size_t range_k,
                                        size_t range_l, size_t tile_l,
                                        uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j++) {
      for (size_t k = 0; k < range_k; k++) {
        for (size_t l = 0; l < range_l; l += tile_l) {
          function(context, i, j, k, l, min(range_l - l, tile_l));
        }
      }
    }
  }
}

void pthreadpool_parallelize_4d_tile_2d(pthreadpool_t threadpool,
                                        pthreadpool_task_4d_tile_2d_t function,
                                        void* context, size_t range_i,
                                        size_t range_j, size_t range_k,
                                        size_t range_l, size_t tile_k,
                                        size_t tile_l, uint32_t flags) {
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
}

void pthreadpool_parallelize_4d_tile_2d_with_uarch(
    pthreadpool_t threadpool, pthreadpool_task_4d_tile_2d_with_id_t function,
    void* context, uint32_t default_uarch_index, uint32_t max_uarch_index,
    size_t range_i, size_t range_j, size_t range_k, size_t range_l,
    size_t tile_k, size_t tile_l, uint32_t flags) {
  for (size_t i = 0; i < range_i; i++) {
    for (size_t j = 0; j < range_j; j++) {
      for (size_t k = 0; k < range_k; k += tile_k) {
        for (size_t l = 0; l < range_l; l += tile_l) {
          function(context, default_uarch_index, i, j, k, l,
                   min(range_k - k, tile_k), min(range_l - l, tile_l));
        }
      }
    }
  }
}

void pthreadpool_parallelize_4d_tile_2d_dynamic(
    pthreadpool_t threadpool, pthreadpool_task_4d_tile_2d_dynamic_t function,
    void* context, size_t range_i, size_t range_j, size_t range_k,
    size_t range_l, size_t tile_k, size_t tile_l, uint32_t flags) {
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
}

void pthreadpool_parallelize_4d_tile_2d_dynamic_with_uarch(
    pthreadpool_t threadpool,
    pthreadpool_task_4d_tile_2d_dynamic_with_id_t function, void* context,
    uint32_t default_uarch_index, uint32_t max_uarch_index, size_t range_i,
    size_t range_j, size_t range_k, size_t range_l, size_t tile_k,
    size_t tile_l, uint32_t flags) {
  if (range_l <= tile_l) {
    for (size_t index_i = 0; index_i < range_i; index_i++) {
      for (size_t index_j = 0; index_j < range_j; index_j++) {
        function(context, default_uarch_index, index_i, index_j, /*index_k=*/0,
                 /*index_l=*/0, range_k, range_l);
      }
    }
  } else {
    for (size_t index_i = 0; index_i < range_i; index_i++) {
      for (size_t index_j = 0; index_j < range_j; index_j++) {
        for (size_t index_k = 0; index_k < range_k; index_k += tile_k) {
          function(context, default_uarch_index, index_i, index_j, index_k,
                   /*index_l=*/0, min(tile_k, range_k - index_k), range_l);
        }
      }
    }
  }
}

void pthreadpool_parallelize_5d(pthreadpool_t threadpool,
                                pthreadpool_task_5d_t function, void* context,
                                size_t range_i, size_t range_j, size_t range_k,
                                size_t range_l, size_t range_m,
                                uint32_t flags) {
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
}

void pthreadpool_parallelize_5d_tile_1d(pthreadpool_t threadpool,
                                        pthreadpool_task_5d_tile_1d_t function,
                                        void* context, size_t range_i,
                                        size_t range_j, size_t range_k,
                                        size_t range_l, size_t range_m,
                                        size_t tile_m, uint32_t flags) {
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
}

void pthreadpool_parallelize_5d_tile_2d(pthreadpool_t threadpool,
                                        pthreadpool_task_5d_tile_2d_t function,
                                        void* context, size_t range_i,
                                        size_t range_j, size_t range_k,
                                        size_t range_l, size_t range_m,
                                        size_t tile_l, size_t tile_m,
                                        uint32_t flags) {
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
}

void pthreadpool_parallelize_6d(pthreadpool_t threadpool,
                                pthreadpool_task_6d_t function, void* context,
                                size_t range_i, size_t range_j, size_t range_k,
                                size_t range_l, size_t range_m, size_t range_n,
                                uint32_t flags) {
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
}

void pthreadpool_parallelize_6d_tile_1d(pthreadpool_t threadpool,
                                        pthreadpool_task_6d_tile_1d_t function,
                                        void* context, size_t range_i,
                                        size_t range_j, size_t range_k,
                                        size_t range_l, size_t range_m,
                                        size_t range_n, size_t tile_n,
                                        uint32_t flags) {
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
}

void pthreadpool_parallelize_6d_tile_2d(pthreadpool_t threadpool,
                                        pthreadpool_task_6d_tile_2d_t function,
                                        void* context, size_t range_i,
                                        size_t range_j, size_t range_k,
                                        size_t range_l, size_t range_m,
                                        size_t range_n, size_t tile_m,
                                        size_t tile_n, uint32_t flags) {
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
}

void pthreadpool_destroy(struct pthreadpool* threadpool) {}
