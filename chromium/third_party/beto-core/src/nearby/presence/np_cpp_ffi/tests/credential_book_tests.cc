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

#include <utility>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "nearby_protocol.h"
#include "np_cpp_ffi_types.h"
#include "np_cpp_test.h"
#include "shared_test_util.h"

TEST_F(NpCppTest, TestCredBookMoveConstructor) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvPlaintext,
                                                              book);
  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V0);

  // Move the credential book into a new object. Using the new object should
  // still result in success
  const nearby_protocol::CredentialBook next_book(std::move(book));
  auto deserialize_result_moved =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvPlaintext,
                                                              next_book);
  ASSERT_EQ(deserialize_result_moved.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V0);

  // The old object should now lead to use after moved assert failure
  ASSERT_DEATH(
      [[maybe_unused]] auto failure =
          nearby_protocol::Deserializer::DeserializeAdvertisement(
              V0AdvPlaintext, book),  // NOLINT(bugprone-use-after-move)
      "");

  // moving again should still lead to a use after moved assert failure
  const nearby_protocol::CredentialBook another_moved_book(std::move(book));
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   nearby_protocol::Deserializer::DeserializeAdvertisement(
                       V0AdvPlaintext, another_moved_book),
               "");
}

TEST_F(NpCppTest, TestCredBookDestructor) {
  nearby_protocol::CredentialSlab slab1;
  auto current_allocations =
      nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
  ASSERT_EQ(current_allocations.cred_slab, 1U);
  nearby_protocol::CredentialBook book1(slab1);
  current_allocations =
      nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
  ASSERT_EQ(current_allocations.cred_book, 1U);
  ASSERT_EQ(current_allocations.cred_slab, 0U);

  {
    nearby_protocol::CredentialSlab slab2;
    nearby_protocol::CredentialBook book2(slab2);
    current_allocations =
        nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
    ASSERT_EQ(current_allocations.cred_book, 2U);
  }

  // After above RAII class goes out of scope, its de-allocation should be
  // reflected in the handle allocation count
  current_allocations =
      nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
  ASSERT_EQ(current_allocations.cred_book, 1U);
}

TEST_F(NpCppTest, TestCredBookMoveAssignment) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvPlaintext,
                                                              book);
  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V0);

  // create a second empty credential book
  nearby_protocol::CredentialSlab other_slab;
  nearby_protocol::CredentialBook other_book(other_slab);
  other_book = std::move(book);

  // new credential book should still be successful
  auto deserialize_result_other =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvPlaintext,
                                                              other_book);
  ASSERT_EQ(deserialize_result_other.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V0);

  // The old object should now lead to use after moved assert failure
  ASSERT_DEATH(
      [[maybe_unused]] auto failure =
          nearby_protocol::Deserializer::DeserializeAdvertisement(
              V0AdvPlaintext, book),  // NOLINT(bugprone-use-after-move)
      "");

  // moving again should still lead to a use after moved assert failure
  auto another_moved_book = std::move(book);
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   nearby_protocol::Deserializer::DeserializeAdvertisement(
                       V0AdvPlaintext, another_moved_book),
               "");
}
