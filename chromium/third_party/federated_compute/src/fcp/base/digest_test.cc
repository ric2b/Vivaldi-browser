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

#include "fcp/base/digest.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"

namespace fcp::base {
namespace {
using testing::BeginEndDistanceIs;
using testing::Ge;

TEST(DigestTest, HashOverEmptyStringIsCorrect) {
  EXPECT_EQ(absl::BytesToHexString(ComputeSHA256("")),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(DigestTest, HashOverNonEmptyStringIsCorrect) {
  EXPECT_EQ(absl::BytesToHexString(ComputeSHA256("foobar")),
            "c3ab8ff13720e8ad9047dd39466b3c8974e592c2fa383d4a3960714caef0c4f2");
  EXPECT_EQ(absl::BytesToHexString(ComputeSHA256("bazfoz")),
            "9491b1103aa3c2cecd90b4cb4cc500441784fc1162a15f3db24f58eda5819fd6");
}

TEST(DigestTest, HashOverEmptyCordIsCorrect) {
  EXPECT_EQ(absl::BytesToHexString(ComputeSHA256(absl::Cord(""))),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(DigestTest, HashOverNonEmptyCordIsCorrect) {
  EXPECT_EQ(absl::BytesToHexString(ComputeSHA256(absl::Cord("foobar"))),
            "c3ab8ff13720e8ad9047dd39466b3c8974e592c2fa383d4a3960714caef0c4f2");
  EXPECT_EQ(absl::BytesToHexString(ComputeSHA256(absl::Cord("bazfoz"))),
            "9491b1103aa3c2cecd90b4cb4cc500441784fc1162a15f3db24f58eda5819fd6");
}

// Tests that the hash function handles absl::Cords with more than one fragment
// correctly.
TEST(DigestTest, HashOverNonEmptyCordWithMultipleChunksIsCorrect) {
  absl::Cord cord;
  // Populate the cord with 128 'a' chars, followed by 128 'b' chars, followed
  // by 128 'c' chars, which should cause it to gain multiple separate cord
  // buffers internally, which in turn should cause the
  // 'absl::Cord::Chunks' method to return multiple chunks.
  cord.Append(std::string(128, 'a'));
  cord.Append(std::string(128, 'b'));
  cord.Append(std::string(128, 'c'));
  EXPECT_THAT(cord.Chunks(), BeginEndDistanceIs(Ge(2)));
  EXPECT_EQ(absl::BytesToHexString(ComputeSHA256(cord)),
            "d8a99ca286ff7a93ed9388ecb36c3da3b55a1f198924aa47bb9698a78dd185f4");
}

}  // namespace
}  // namespace fcp::base
