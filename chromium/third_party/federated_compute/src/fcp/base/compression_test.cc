/*
 * Copyright 2024 Google LLC
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

#include "fcp/base/compression.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/testing/testing.h"

namespace fcp::base {
namespace {

TEST(CompressionTest, CompressDecompressEmptyString) {
  auto compressed = CompressWithGzip("");
  ASSERT_OK(compressed);

  auto uncompressed = UncompressWithGzip(*compressed);
  ASSERT_OK(uncompressed);

  EXPECT_EQ(*uncompressed, "");
}

TEST(CompressionTest, CompressDecompressNonEmptyString) {
  auto data = "foobar";
  auto compressed = CompressWithGzip(data);
  ASSERT_OK(compressed);

  auto uncompressed = UncompressWithGzip(*compressed);
  ASSERT_OK(uncompressed);

  EXPECT_EQ(*uncompressed, data);
}
}  // namespace
}  // namespace fcp::base
