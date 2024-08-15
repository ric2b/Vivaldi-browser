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
#include "shared_test_util.h"
#include "np_cpp_test.h"

#include "gtest/gtest.h"

TEST_F(NpCppTest, TestSetMaxCredBooks) {
  auto slab1_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab1_result.ok());
  auto book1_result = nearby_protocol::CredentialBook::TryCreateFromSlab(slab1_result.value());
  ASSERT_TRUE(book1_result.ok());

  auto slab2_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab2_result.ok());
  auto book2_result = nearby_protocol::CredentialBook::TryCreateFromSlab(slab2_result.value());
  ASSERT_TRUE(book2_result.ok());

  auto slab3_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab3_result.ok());
  auto book3_result = nearby_protocol::CredentialBook::TryCreateFromSlab(slab3_result.value());

  ASSERT_FALSE(book3_result.ok());
  ASSERT_TRUE(absl::IsResourceExhausted(book3_result.status()));
}

TEST_F(NpCppTest, TestBookMoveConstructor) {
  auto slab = nearby_protocol::CredentialSlab::TryCreate().value();
  auto book = nearby_protocol::CredentialBook::TryCreateFromSlab(slab).value();
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvSimple,
                                                              book);
  ASSERT_EQ(deserialize_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);

  // Move the credential book into a new object. Using the new object should
  // still result in success
  nearby_protocol::CredentialBook next_book(std::move(book));
  auto deserialize_result_moved =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvSimple,
                                                              next_book);
  ASSERT_EQ(deserialize_result_moved.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);

  // The old object should now lead to use after moved assert failure
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   nearby_protocol::Deserializer::DeserializeAdvertisement(
                       V0AdvSimple, book), // NOLINT(bugprone-use-after-move)
               "");

  // moving again should still lead to a use after moved assert failure
  nearby_protocol::CredentialBook another_moved_book(std::move(book));
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   nearby_protocol::Deserializer::DeserializeAdvertisement(
                       V0AdvSimple, another_moved_book),
               "");
}

TEST_F(NpCppTest, TestBookMoveAssignment) {
  auto slab = nearby_protocol::CredentialSlab::TryCreate().value();
  auto book = nearby_protocol::CredentialBook::TryCreateFromSlab(slab).value();
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvSimple,
                                                              book);
  ASSERT_EQ(deserialize_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);

  // create a second empty credential book
  auto other_slab = nearby_protocol::CredentialSlab::TryCreate().value();
  auto other_book = nearby_protocol::CredentialBook::TryCreateFromSlab(other_slab).value();
  other_book = std::move(book);

  // new credential book should still be successful
  auto deserialize_result_other =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvSimple,
                                                              other_book);
  ASSERT_EQ(deserialize_result_other.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);

  // The old object should now lead to use after moved assert failure
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   nearby_protocol::Deserializer::DeserializeAdvertisement(
                       V0AdvSimple, book), // NOLINT(bugprone-use-after-move)
               "");

  // moving again should still lead to a use after moved assert failure
  auto another_moved_book = std::move(book);
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   nearby_protocol::Deserializer::DeserializeAdvertisement(
                       V0AdvSimple, another_moved_book),
               "");
}
