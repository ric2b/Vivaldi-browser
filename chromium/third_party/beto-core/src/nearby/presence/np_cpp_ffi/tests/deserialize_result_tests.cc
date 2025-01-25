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
#include <utility>

#include "absl/strings/escaping.h"
#include "gtest/gtest.h"
#include "nearby_protocol.h"
#include "np_cpp_ffi_types.h"
#include "np_cpp_test.h"
#include "shared_test_util.h"

TEST_F(NpCppTest, TestResultMoveConstructor) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  ASSERT_EQ(result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);

  // Now move the result into a new value, and make sure its still valid
  nearby_protocol::DeserializeAdvertisementResult moved_result(
      std::move(result));
  ASSERT_EQ(moved_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);
  auto v0 = moved_result.IntoV0();
  ASSERT_EQ(v0.GetKind(),
            np_ffi::internal::DeserializedV0AdvertisementKind::Legible);

  // trying to use the moved object should result in a use after free which
  // triggers an abort
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   result.IntoV0(),  // NOLINT(bugprone-use-after-move)
               "");
  ASSERT_DEATH([[maybe_unused]] auto failure = result.GetKind(), "");

  // moving again should still preserve the moved state and also lead to an
  // abort
  nearby_protocol::DeserializeAdvertisementResult moved_again(
      std::move(moved_result));
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.IntoV0(), "");
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.GetKind(), "");
}

TEST_F(NpCppTest, DeserializeFromStringView) {
  std::string bytes;
  ASSERT_TRUE(absl::HexStringToBytes("001503", &bytes));
  auto buffer = nearby_protocol::ByteBuffer<
      nearby_protocol::MAX_ADV_PAYLOAD_SIZE>::TryFromString(bytes);
  ASSERT_TRUE(buffer.ok());

  const nearby_protocol::RawAdvertisementPayload adv(buffer.value());
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(adv,
                                                              credential_book);
  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V0);
  auto v0_adv = deserialize_result.IntoV0();

  ASSERT_EQ(v0_adv.GetKind(),
            nearby_protocol::DeserializedV0AdvertisementKind::Legible);
  auto legible_adv = v0_adv.IntoLegible();
  auto identity = legible_adv.GetIdentityKind();
  ASSERT_EQ(identity, nearby_protocol::DeserializedV0IdentityKind::Plaintext);

  auto num_des = legible_adv.GetNumberOfDataElements();
  ASSERT_EQ(num_des, 1);
  auto payload = legible_adv.IntoPayload();

  auto de_result = payload.TryGetDataElement(0);
  ASSERT_TRUE(de_result.ok());
  auto de = de_result.value();

  ASSERT_EQ(de.GetKind(), nearby_protocol::V0DataElementKind::TxPower);
  auto tx_power = de.AsTxPower();
  ASSERT_EQ(tx_power.GetAsI8(), 3);
}

TEST_F(NpCppTest, TestResultMoveAssignment) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  ASSERT_EQ(result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);

  // create a second result
  auto another_result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  ASSERT_EQ(another_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);

  // move result into another_result. The original another_result should be
  // de-allocated.
  another_result = std::move(result);
  auto v0 = another_result.IntoV0();
  ASSERT_EQ(v0.GetKind(),
            np_ffi::internal::DeserializedV0AdvertisementKind::Legible);

  // original result should now be invalid, using it will trigger a use after
  // free abort.
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   result.IntoV0(),  // NOLINT(bugprone-use-after-move)
               "");
  ASSERT_DEATH([[maybe_unused]] auto failure = result.GetKind(), "");

  // moving again should still lead to an error
  auto moved_again = std::move(result);
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.IntoV0(), "");
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.GetKind(), "");
}

TEST_F(NpCppTest, TestInvalidPayloadHeader) {
  // An invalid header result should result in error
  const std::array<uint8_t, 1> InvalidHeaderPayloadBytes{0xFF};
  const nearby_protocol::RawAdvertisementPayload InvalidHeaderPayload(
      (nearby_protocol::ByteBuffer<255>(InvalidHeaderPayloadBytes)));
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(
          InvalidHeaderPayload, credential_book);

  // Errors cannot be casted into further types
  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::Error);
  ASSERT_DEATH({ [[maybe_unused]] auto failure = deserialize_result.IntoV0(); },
               "");
  ASSERT_DEATH({ [[maybe_unused]] auto failure = deserialize_result.IntoV1(); },
               "");
}

TEST_F(NpCppTest, TestInvalidV0Cast) {
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V1AdvPlaintext,
                                                              credential_book);

  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V1);
  ASSERT_DEATH({ [[maybe_unused]] auto failure = deserialize_result.IntoV0(); },
               "");
}

TEST_F(NpCppTest, TestInvalidV1Cast) {
  // Create an empty credential book and verify that is is successful
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvPlaintext,
                                                              credential_book);

  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V0);
  ASSERT_DEATH({ [[maybe_unused]] auto failure = deserialize_result.IntoV1(); },
               "");
}

TEST_F(NpCppTest, V0UseResultTwice) {
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvPlaintext,
                                                              credential_book);
  ASSERT_EQ(deserialize_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);

  // Once this goes out of scope, the entire result will be invalid
  auto v0_adv = deserialize_result.IntoV0();
  // Calling intoV0 for a second time is a programmer error and will result
  // in a crash.
  ASSERT_DEATH({ [[maybe_unused]] auto failure = deserialize_result.IntoV0(); },
               "");
}

TEST_F(NpCppTest, V1UseResultTwice) {
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V1AdvPlaintext,
                                                              credential_book);
  ASSERT_EQ(deserialize_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V1);

  // Once this goes out of scope, the entire result will be invalid
  auto v1_adv = deserialize_result.IntoV1();
  // Calling intoV0 for a second time is a programmer error and will result
  // in a crash.
  ASSERT_DEATH({ [[maybe_unused]] auto failure = deserialize_result.IntoV1(); },
               "");
}

TEST_F(NpCppTest, IntoV0AfterOutOfScope) {
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvPlaintext,
                                                              credential_book);
  ASSERT_EQ(deserialize_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);

  // Once this goes out of scope, the entire result will be invalid
  { auto v0_adv = deserialize_result.IntoV0(); }

  // Calling intoV0 for a second time is a programmer error and will result
  // in a crash.
  ASSERT_DEATH({ [[maybe_unused]] auto failure = deserialize_result.IntoV0(); },
               "");
}

TEST_F(NpCppTest, IntoV1AfterOutOfScope) {
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V1AdvPlaintext,
                                                              credential_book);
  ASSERT_EQ(deserialize_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V1);

  // Once this goes out of scope, the entire result will be invalid
  { auto v0_adv = deserialize_result.IntoV1(); }

  // Calling intoV0 for a second time is a programmer error and will result
  // in a crash.
  ASSERT_DEATH({ [[maybe_unused]] auto failure = deserialize_result.IntoV1(); },
               "");
}

TEST_F(NpCppTest, V0ResultKindAfterOutOfScope) {
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvPlaintext,
                                                              credential_book);
  ASSERT_EQ(deserialize_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);

  // Once this goes out of scope, the entire result will be invalid
  { auto v0_adv = deserialize_result.IntoV0(); }

  // Calling intoV0 for a second time is a programmer error and will result
  // in a crash.
  ASSERT_DEATH(
      { [[maybe_unused]] auto failure = deserialize_result.GetKind(); }, "");
}

TEST_F(NpCppTest, V1ResultKindAfterOutOfScope) {
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V1AdvPlaintext,
                                                              credential_book);
  ASSERT_EQ(deserialize_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V1);

  // Once this goes out of scope, the entire result will be invalid
  { auto v0_adv = deserialize_result.IntoV1(); }

  // Calling intoV0 for a second time is a programmer error and will result
  // in a crash.
  ASSERT_DEATH(
      { [[maybe_unused]] auto failure = deserialize_result.GetKind(); }, "");
}
