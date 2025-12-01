/*
 * Copyright 2020 Google LLC
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

#include "fcp/base/process_unique_id.h"

#include <thread>  // NOLINT(build/c++11)

#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"

namespace fcp {

TEST(UniqueIdGeneratorTest, CallingNextYieldsDifferentIds) {
  uint64_t id1 = ProcessUniqueId::Next().value();
  uint64_t id2 = ProcessUniqueId::Next().value();
  EXPECT_NE(id1, id2);
}

TEST(UniqueIdGeneratorTest, MultipleThreads) {
  const int n = 15;
  std::thread threads[n];
  uint64_t ids[n];
  for (int i = 0; i < n; i++) {
    threads[i] =
        std::thread([&ids, i]() { ids[i] = ProcessUniqueId::Next().value(); });
  }
  for (int i = 0; i < n; i++) {
    threads[i].join();
  }
  absl::flat_hash_set<uint64_t> id_set;
  for (int i = 0; i < n; i++) {
    bool inserted = id_set.insert(ids[i]).second;
    EXPECT_TRUE(inserted);
  }
}

}  // namespace fcp
