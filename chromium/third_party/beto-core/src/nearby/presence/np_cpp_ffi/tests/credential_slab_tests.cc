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

#include <array>
#include <cstdint>
#include <span>
#include <utility>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "nearby_protocol.h"
#include "np_cpp_test.h"

// NOLINTBEGIN(readability-magic-numbers)
TEST_F(NpCppTest, TestSlabMoveConstructor) {
  nearby_protocol::CredentialSlab slab;
  // It should be possible to move the slab into a new object
  // and use the moved version to successfully construct a
  // credential-book.
  nearby_protocol::CredentialSlab next_slab(std::move(slab));
  nearby_protocol::CredentialBook book(next_slab);

  // Now, both slabs should be moved-out-of, since `CreateFromSlab` takes
  // ownership. Verify that this is the case, and attempts to re-use the slabs
  // result in an assert failure.
  ASSERT_DEATH([[maybe_unused]] nearby_protocol::CredentialBook failure(
                   slab),  // NOLINT(bugprone-use-after-move)
               "");
  ASSERT_DEATH([[maybe_unused]] nearby_protocol::CredentialBook failure(
                   next_slab),  // NOLINT(bugprone-use-after-move)
               "");
}

TEST_F(NpCppTest, TestSlabMoveAssignment) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialSlab other_slab;

  // move assignment should override currently assigned slab with new one,
  // freeing the existing one.
  other_slab = std::move(slab);
  nearby_protocol::CredentialBook book(other_slab);

  // The old object should now lead to use after moved assert failure
  ASSERT_DEATH([[maybe_unused]] nearby_protocol::CredentialBook failure(
                   slab),  // NOLINT(bugprone-use-after-move)
               "");

  // moving again should still lead to a use after moved assert failure
  auto another_moved_book = std::move(slab);
  ASSERT_DEATH([[maybe_unused]] nearby_protocol::CredentialBook failure(
                   another_moved_book),  // NOLINT(bugprone-use-after-move)
               "");
}

TEST_F(NpCppTest, TestAddV0Credential) {
  nearby_protocol::CredentialSlab slab;

  uint8_t metadata[] = {1, 2, 3};
  const std::span<uint8_t> metadata_span(metadata);

  const nearby_protocol::MatchedCredentialData match_data(111, metadata_span);
  const std::array<uint8_t, 32> key_seed{1, 2, 3};
  const std::array<uint8_t, 32> legacy_metadata_key_hmac{1, 2, 3};

  const nearby_protocol::V0MatchableCredential v0_cred(
      key_seed, legacy_metadata_key_hmac, match_data);
  slab.AddV0Credential(v0_cred);
}

TEST_F(NpCppTest, TestAddV0CredentialAfterMoved) {
  nearby_protocol::CredentialSlab slab;
  // creating a book will move the slab
  nearby_protocol::CredentialBook book(slab);

  uint8_t metadata[] = {1, 2, 3};
  const std::span<uint8_t> metadata_span(metadata);
  const nearby_protocol::MatchedCredentialData match_data(111, metadata_span);
  const std::array<uint8_t, 32> key_seed{1, 2, 3};
  const std::array<uint8_t, 32> legacy_metadata_key_hmac{1, 2, 3};
  const nearby_protocol::V0MatchableCredential v0_cred(
      key_seed, legacy_metadata_key_hmac, match_data);
  ASSERT_DEATH(slab.AddV0Credential(v0_cred);, "");
}

TEST_F(NpCppTest, TestAddV1Credential) {
  nearby_protocol::CredentialSlab slab;

  uint8_t metadata[] = {1, 2, 3};
  const std::span<uint8_t> metadata_span(metadata);
  const nearby_protocol::MatchedCredentialData match_data(111, metadata_span);
  const std::array<uint8_t, 32> key_seed{1, 2, 3};
  const std::array<uint8_t, 32> expected_unsigned_metadata_key_hmac{1, 2, 3};
  const std::array<uint8_t, 32> expected_signed_metadata_key_hmac{1, 2, 3};
  const std::array<uint8_t, 32> pub_key{1, 2, 3};
  const nearby_protocol::V1MatchableCredential v1_cred(
      key_seed, expected_unsigned_metadata_key_hmac,
      expected_signed_metadata_key_hmac, pub_key, match_data);

  auto add_result = slab.AddV1Credential(v1_cred);
  ASSERT_EQ(add_result, absl::OkStatus());
}

TEST_F(NpCppTest, TestAddV1CredentialAfterMoved) {
  nearby_protocol::CredentialSlab slab;
  // creating a book will move the slab
  nearby_protocol::CredentialBook book(slab);
  uint8_t metadata[] = {1, 2, 3};
  const std::span<uint8_t> metadata_span(metadata);
  const nearby_protocol::MatchedCredentialData match_data(111, metadata_span);
  const std::array<uint8_t, 32> key_seed{1, 2, 3};
  const std::array<uint8_t, 32> expected_unsigned_metadata_key_hmac{1, 2, 3};
  const std::array<uint8_t, 32> expected_signed_metadata_key_hmac{1, 2, 3};
  const std::array<uint8_t, 32> pub_key{1, 2, 3};
  const nearby_protocol::V1MatchableCredential v1_cred(
      key_seed, expected_unsigned_metadata_key_hmac,
      expected_signed_metadata_key_hmac, pub_key, match_data);

  ASSERT_DEATH([[maybe_unused]] auto add_result = slab.AddV1Credential(v1_cred);
               , "");
}

// make sure the book can be populated with many credentials
TEST_F(NpCppTest, TestAddManyCredentials) {
  nearby_protocol::CredentialSlab slab;
  // Should be able to load the slab up with many credentials
  for (int i = 0; i < 500; i++) {
    uint8_t metadata[] = {1, 2, 3};
    const std::span<uint8_t> metadata_span(metadata);
    const nearby_protocol::MatchedCredentialData match_data(111, metadata_span);
    const std::array<uint8_t, 32> key_seed{1, 2, 3};
    const std::array<uint8_t, 32> legacy_metadata_key_hmac{1, 2, 3};
    const nearby_protocol::V0MatchableCredential v0_cred(
        key_seed, legacy_metadata_key_hmac, match_data);
    slab.AddV0Credential(v0_cred);

    const std::array<uint8_t, 32> v1_key_seed{1, 2, 3};
    const std::array<uint8_t, 32> v1_expected_unsigned_metadata_key_hmac{1, 2,
                                                                         3};
    const std::array<uint8_t, 32> v1_expected_signed_metadata_key_hmac{1, 2, 3};
    const std::array<uint8_t, 32> v1_pub_key{1, 2, 3};
    const nearby_protocol::V1MatchableCredential v1_cred(
        v1_key_seed, v1_expected_unsigned_metadata_key_hmac,
        v1_expected_signed_metadata_key_hmac, v1_pub_key, match_data);

    auto add_v1_result = slab.AddV1Credential(v1_cred);
    ASSERT_EQ(add_v1_result, absl::OkStatus());
  }
  nearby_protocol::CredentialBook book(slab);
}

TEST_F(NpCppTest, TestSlabDestructor) {
  {
    nearby_protocol::CredentialSlab slab;
    nearby_protocol::CredentialSlab slab2;
    nearby_protocol::CredentialSlab slab3;
    auto alloc_count =
        nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
    ASSERT_EQ(alloc_count.cred_slab, 3U);
  }
  auto alloc_count =
      nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
  ASSERT_EQ(alloc_count.cred_slab, 0U);
}
// NOLINTEND(readability-magic-numbers)