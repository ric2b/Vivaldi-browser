// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "nearby_protocol.h"
#include "np_cpp_test.h"
#include "shared_test_util.h"

#include "gtest/gtest.h"

TEST_F(NpCppTest, TestSetMaxCredSlabs) {
  auto slab1_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab1_result.ok());

  auto slab2_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab2_result.ok());

  auto slab3_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab3_result.ok());

  auto slab4_result = nearby_protocol::CredentialSlab::TryCreate();

  ASSERT_FALSE(slab4_result.ok());
  ASSERT_TRUE(absl::IsResourceExhausted(slab4_result.status()));
}

TEST_F(NpCppTest, TestSlabMoveConstructor) {
  auto slab = nearby_protocol::CredentialSlab::TryCreate().value();
  // It should be possible to move the slab into a new object
  // and use the moved version to successfully construct a
  // credential-book.
  nearby_protocol::CredentialSlab next_slab(std::move(slab));

  auto maybe_book =
      nearby_protocol::CredentialBook::TryCreateFromSlab(next_slab);
  ASSERT_TRUE(maybe_book.ok());

  // Now, both slabs should be moved-out-of, since `TryCreateFromSlab` takes
  // ownership. Verify that this is the case, and attempts to re-use the slabs
  // result in an assert failure.
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   nearby_protocol::CredentialBook::TryCreateFromSlab(
                       slab), // NOLINT(bugprone-use-after-move)
               "");
  ASSERT_DEATH(
      [[maybe_unused]] auto failure =
          nearby_protocol::CredentialBook::TryCreateFromSlab(next_slab),
      "");
}

TEST_F(NpCppTest, TestSlabDestructor) {
  {
    auto slab1_result = nearby_protocol::CredentialSlab::TryCreate();
    ASSERT_TRUE(slab1_result.ok());

    auto slab2_result = nearby_protocol::CredentialSlab::TryCreate();
    ASSERT_TRUE(slab2_result.ok());

    auto slab3_result = nearby_protocol::CredentialSlab::TryCreate();
    ASSERT_TRUE(slab3_result.ok());

    auto slab4_result = nearby_protocol::CredentialSlab::TryCreate();

    ASSERT_FALSE(slab4_result.ok());
    ASSERT_TRUE(absl::IsResourceExhausted(slab4_result.status()));
  }

  // Now that the above variables have gone out of scope we should verify that
  // the destructor succeeded in cleaning up those resources
  auto slab_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab_result.ok());
}

TEST_F(NpCppTest, TestSlabMoveAssignment) {
  auto slab_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab_result.ok());

  // create a second slab
  auto other_slab_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(other_slab_result.ok());

  // move assignment should override currently assigned slab with new one,
  // freeing the existing one.
  auto other_slab = std::move(slab_result.value());
  auto maybe_book =
      nearby_protocol::CredentialBook::TryCreateFromSlab(other_slab);
  ASSERT_TRUE(maybe_book.ok());

  // The old object should now lead to use after moved assert failure
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   nearby_protocol::CredentialBook::TryCreateFromSlab(
                       slab_result.value()), // NOLINT(bugprone-use-after-move)
               "");

  // moving again should still lead to a use after moved assert failure
  auto another_moved_book = std::move(slab_result.value());
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   nearby_protocol::CredentialBook::TryCreateFromSlab(
                       another_moved_book), // NOLINT(bugprone-use-after-move)
               "");
}

TEST_F(NpCppTest, TestAddV0Credential) {
  auto slab_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab_result.ok());

  uint8_t metadata[] = {1, 2, 3};
  std::span<uint8_t> metadata_span(metadata);

  nearby_protocol::MatchedCredentialData match_data(111, metadata_span);
  std::array<uint8_t, 32> key_seed {1, 2, 3};
  std::array<uint8_t, 32> legacy_metadata_key_hmac {1, 2, 3};

  nearby_protocol::V0MatchableCredential v0_cred(
      key_seed, legacy_metadata_key_hmac, match_data);
  auto add_result = slab_result.value().AddV0Credential(v0_cred);
  ASSERT_EQ(add_result, absl::OkStatus());
}

TEST_F(NpCppTest, TestAddV0CredentialAfterMoved) {
  auto slab_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab_result.ok());

  // creating a book will move the slab
  auto maybe_book =
      nearby_protocol::CredentialBook::TryCreateFromSlab(slab_result.value());
  ASSERT_TRUE(maybe_book.ok());

  uint8_t metadata[] = {1, 2, 3};
  std::span<uint8_t> metadata_span(metadata);
  nearby_protocol::MatchedCredentialData match_data(111, metadata_span);
  std::array<uint8_t, 32> key_seed {1, 2, 3};
  std::array<uint8_t, 32> legacy_metadata_key_hmac {1, 2, 3};
  nearby_protocol::V0MatchableCredential v0_cred(
      key_seed, legacy_metadata_key_hmac, match_data);

  ASSERT_DEATH([[maybe_unused]] auto add_result =
                   slab_result.value().AddV0Credential(v0_cred);
               , "");
}

TEST_F(NpCppTest, TestAddV1Credential) {
  auto slab_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab_result.ok());

  uint8_t metadata[] = {1, 2, 3};
  std::span<uint8_t> metadata_span(metadata);
  nearby_protocol::MatchedCredentialData match_data(111, metadata_span);
  std::array<uint8_t, 32> key_seed {1, 2, 3};
  std::array<uint8_t, 32> expected_unsigned_metadata_key_hmac {1, 2, 3};
  std::array<uint8_t, 32> expected_signed_metadata_key_hmac {1, 2, 3};
  std::array<uint8_t, 32> pub_key {1, 2, 3};
  nearby_protocol::V1MatchableCredential v1_cred(
      key_seed, expected_unsigned_metadata_key_hmac,
      expected_signed_metadata_key_hmac, pub_key, match_data);

  auto add_result = slab_result.value().AddV1Credential(v1_cred);
  ASSERT_EQ(add_result, absl::OkStatus());
}

TEST_F(NpCppTest, TestAddV1CredentialAfterMoved) {
  auto slab_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab_result.ok());

  // creating a book will move the slab
  auto maybe_book =
      nearby_protocol::CredentialBook::TryCreateFromSlab(slab_result.value());
  ASSERT_TRUE(maybe_book.ok());

  uint8_t metadata[] = {1, 2, 3};
  std::span<uint8_t> metadata_span(metadata);
  nearby_protocol::MatchedCredentialData match_data(111, metadata_span);
  std::array<uint8_t, 32> key_seed {1, 2, 3};
  std::array<uint8_t, 32> expected_unsigned_metadata_key_hmac {1, 2, 3};
  std::array<uint8_t, 32> expected_signed_metadata_key_hmac {1, 2, 3};
  std::array<uint8_t, 32> pub_key {1, 2, 3};
  nearby_protocol::V1MatchableCredential v1_cred(
      key_seed, expected_unsigned_metadata_key_hmac,
      expected_signed_metadata_key_hmac, pub_key, match_data);

  ASSERT_DEATH([[maybe_unused]] auto add_result =
                   slab_result.value().AddV1Credential(v1_cred);
               , "");
}
