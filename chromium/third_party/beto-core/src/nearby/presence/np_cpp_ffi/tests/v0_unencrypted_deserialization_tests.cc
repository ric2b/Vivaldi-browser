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

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "nearby_protocol.h"
#include "np_cpp_ffi_types.h"
#include "np_cpp_test.h"
#include "shared_test_util.h"

TEST_F(NpCppTest, InvalidCast) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvPlaintext,
                                                              book);
  ASSERT_EQ(deserialize_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);

  // Now try to cast the result into the wrong type and verify the process
  // aborts
  ASSERT_DEATH({ [[maybe_unused]] auto failure = deserialize_result.IntoV1(); },
               "");
}

TEST_F(NpCppTest, V0DeserializeSingleDataElementTxPower) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook credential_book(slab);

  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvPlaintext,
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

TEST_F(NpCppTest, V0LengthOneActionsDataElement) {
  const std::array<uint8_t, 3> V0AdvPlaintextLengthOneActions{0x00, 0x16, 0x00};
  const nearby_protocol::RawAdvertisementPayload adv(
      (nearby_protocol::ByteBuffer<255>(V0AdvPlaintextLengthOneActions)));

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

  ASSERT_EQ(de.GetKind(), nearby_protocol::V0DataElementKind::Actions);
  auto actions = de.AsActions();
  ASSERT_EQ(actions.GetAsU32(), (uint32_t)0);
}

TEST_F(NpCppTest, V0LengthTwoActionsDataElement) {
  const std::array<uint8_t, 4> V0AdvPlaintextLengthTwoActions{0x00, 0x26, 0x40,
                                                              0x40};
  const nearby_protocol::RawAdvertisementPayload adv(
      (nearby_protocol::ByteBuffer<255>(V0AdvPlaintextLengthTwoActions)));

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

  ASSERT_EQ(de.GetKind(), nearby_protocol::V0DataElementKind::Actions);
  auto actions = de.AsActions();
  ASSERT_EQ(actions.GetAsU32(), 0x40400000U);

  ASSERT_TRUE(actions.HasAction(nearby_protocol::ActionType::CrossDevSdk));
  ASSERT_TRUE(actions.HasAction(nearby_protocol::ActionType::NearbyShare));

  ASSERT_FALSE(actions.HasAction(nearby_protocol::ActionType::ActiveUnlock));
  ASSERT_FALSE(
      actions.HasAction(nearby_protocol::ActionType::InstantTethering));
  ASSERT_FALSE(actions.HasAction(nearby_protocol::ActionType::PhoneHub));
}

TEST_F(NpCppTest, V0MultipleDataElements) {
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);

  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(
          V0AdvPlaintextMultiDe, credential_book);

  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V0);
  auto v0_adv = deserialize_result.IntoV0();

  ASSERT_EQ(v0_adv.GetKind(),
            nearby_protocol::DeserializedV0AdvertisementKind::Legible);
  auto legible_adv = v0_adv.IntoLegible();
  auto identity = legible_adv.GetIdentityKind();
  ASSERT_EQ(identity, nearby_protocol::DeserializedV0IdentityKind::Plaintext);

  auto num_des = legible_adv.GetNumberOfDataElements();
  ASSERT_EQ(num_des, 2);
  auto payload = legible_adv.IntoPayload();

  auto first_de_result = payload.TryGetDataElement(0);
  ASSERT_TRUE(first_de_result.ok());
  auto first_de = first_de_result.value();

  ASSERT_EQ(first_de.GetKind(), nearby_protocol::V0DataElementKind::TxPower);
  auto power = first_de.AsTxPower();
  ASSERT_EQ(power.GetAsI8(), 5);

  auto second_de_result = payload.TryGetDataElement(1);
  ASSERT_TRUE(second_de_result.ok());
  auto second_de = second_de_result.value();

  ASSERT_EQ(second_de.GetKind(), nearby_protocol::V0DataElementKind::Actions);
  auto actions = second_de.AsActions();
  ASSERT_EQ(actions.GetAsU32(), (uint32_t)0x40400000);
}

TEST_F(NpCppTest, V0EmptyPayload) {
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);

  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvEmpty,
                                                              credential_book);

  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::Error);
}

TEST_F(NpCppTest, TestV0AdvMoveConstructor) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  ASSERT_EQ(result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);
  auto adv = result.IntoV0();

  // Now move the adv into a new value, and make sure its still valid
  const nearby_protocol::DeserializedV0Advertisement moved_adv(std::move(adv));
  ASSERT_EQ(moved_adv.GetKind(),
            np_ffi::internal::DeserializedV0AdvertisementKind::Legible);

  // trying to use the moved object should result in a use after free which
  // triggers an abort
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   adv.IntoLegible(),  // NOLINT(bugprone-use-after-move)
               "");
  ASSERT_DEATH([[maybe_unused]] auto failure = adv.GetKind(), "");

  // moving again should still preserve the moved state and also lead to an
  // abort
  nearby_protocol::DeserializedV0Advertisement moved_again(std::move(adv));
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.IntoLegible(), "");
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.GetKind(), "");
}

TEST_F(NpCppTest, TestV0AdvMoveAssignment) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  ASSERT_EQ(result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);
  auto adv = result.IntoV0();

  // create a second result
  auto another_result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  ASSERT_EQ(another_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);
  auto adv2 = another_result.IntoV0();

  // move adv2 into adv, the original should be deallocated by assignment
  adv2 = std::move(adv);
  ASSERT_EQ(adv2.GetKind(),
            np_ffi::internal::DeserializedV0AdvertisementKind::Legible);

  // original result should now be invalid, using it will trigger a use after
  // free abort.
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   adv.IntoLegible(),  // NOLINT(bugprone-use-after-move)
               "");
  ASSERT_DEATH([[maybe_unused]] auto failure = adv.GetKind(), "");

  // moving again should still lead to an error
  auto moved_again = std::move(adv);
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.IntoLegible(), "");
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.GetKind(), "");
}

nearby_protocol::DeserializeAdvertisementResult CreateAdv(
    nearby_protocol::CredentialBook &book) {
  auto adv = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  assert(adv.GetKind() ==
         np_ffi::internal::DeserializeAdvertisementResultKind::V0);
  return adv;
}

TEST_F(NpCppTest, V0AdvDestructor) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  {
    auto deserialize_result = CreateAdv(book);
    auto deserialize_result2 = CreateAdv(book);
    auto allocations =
        nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
    ASSERT_EQ(allocations.v0_payload, 2U);

    // Calling IntoV0() should move the underlying resources into the v0
    // object when both go out of scope only one should be freed
    auto v0_adv = deserialize_result.IntoV0();
  }
  auto allocations =
      nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
  ASSERT_EQ(allocations.v0_payload, 0U);
}

TEST_F(NpCppTest, V0AdvUseAfterMove) {
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);

  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvPlaintext,
                                                              credential_book);

  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V0);
  auto v0_adv = deserialize_result.IntoV0();

  ASSERT_EQ(v0_adv.GetKind(),
            nearby_protocol::DeserializedV0AdvertisementKind::Legible);

  // Moves the adv into a legible adv, so the original v0_adv is no longer valid
  [[maybe_unused]] auto legible_adv = v0_adv.IntoLegible();
  ASSERT_DEATH([[maybe_unused]] auto failure = v0_adv.GetKind(), "");
  ASSERT_DEATH([[maybe_unused]] auto failure = v0_adv.IntoLegible(), "");
}

TEST_F(NpCppTest, TestLegibleAdvMoveConstructor) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  ASSERT_EQ(result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);
  auto legible = result.IntoV0().IntoLegible();

  // Now move the adv into a new value, and make sure its still valid
  const nearby_protocol::LegibleDeserializedV0Advertisement moved(
      std::move(legible));
  ASSERT_EQ(moved.GetNumberOfDataElements(), 1);
  ASSERT_EQ(moved.GetIdentityKind(),
            np_ffi::internal::DeserializedV0IdentityKind::Plaintext);

  // trying to use the moved object should result in a use after free which
  // triggers an abort
  ASSERT_DEATH(
      [[maybe_unused]] auto failure =
          legible.GetIdentityKind(),  // NOLINT(bugprone-use-after-move)
      "");
  ASSERT_DEATH(
      [[maybe_unused]] auto failure = legible.GetNumberOfDataElements(), "");
  ASSERT_DEATH([[maybe_unused]] auto failure = legible.IntoPayload(), "");

  // moving again should still preserve the moved state and also lead to an
  // abort
  nearby_protocol::LegibleDeserializedV0Advertisement moved_again(
      std::move(legible));
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.GetIdentityKind(),
               "");
  ASSERT_DEATH(
      [[maybe_unused]] auto failure = moved_again.GetNumberOfDataElements(),
      "");
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.IntoPayload(), "");
}

TEST_F(NpCppTest, TestLegibleAdvMoveAssignment) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  ASSERT_EQ(result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);
  auto legible = result.IntoV0().IntoLegible();

  // create a second result
  auto another_result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  ASSERT_EQ(another_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);
  auto legible2 = another_result.IntoV0().IntoLegible();

  // move adv2 into adv, the original should be deallocated by assignment
  legible2 = std::move(legible);
  ASSERT_EQ(legible2.GetIdentityKind(),
            np_ffi::internal::DeserializedV0IdentityKind::Plaintext);

  // original result should now be invalid, using it will trigger a use after
  // free abort.
  ASSERT_DEATH(
      [[maybe_unused]] auto failure =
          legible.GetIdentityKind(),  // NOLINT(bugprone-use-after-move)
      "");
  ASSERT_DEATH(
      [[maybe_unused]] auto failure = legible.GetNumberOfDataElements(), "");
  ASSERT_DEATH([[maybe_unused]] auto failure = legible.IntoPayload(), "");

  // moving again should still lead to an error
  auto moved_again = std::move(legible);
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.IntoPayload(), "");
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.GetIdentityKind(),
               "");
  ASSERT_DEATH(
      [[maybe_unused]] auto failure = moved_again.GetNumberOfDataElements(),
      "");
}

nearby_protocol::LegibleDeserializedV0Advertisement CreateLegibleAdv(
    nearby_protocol::CredentialBook &book) {
  auto adv = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  auto v0_adv = adv.IntoV0();
  auto legible = v0_adv.IntoLegible();
  assert(legible.GetIdentityKind() ==
         nearby_protocol::DeserializedV0IdentityKind::Plaintext);
  assert(legible.GetNumberOfDataElements() == 1U);
  return legible;
}

TEST_F(NpCppTest, V0LegibleAdvUseAfterMove) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto legible_adv = CreateLegibleAdv(book);

  // Should be able to use the valid legible adv even though its original parent
  // is now out of scope.
  [[maybe_unused]] auto payload = legible_adv.IntoPayload();

  // now that the legible adv has moved into the payload it should no longer be
  // valid
  ASSERT_DEATH([[maybe_unused]] auto failure = legible_adv.GetIdentityKind(),
               "");
  ASSERT_DEATH(
      [[maybe_unused]] auto failure = legible_adv.GetNumberOfDataElements(),
      "");
  ASSERT_DEATH([[maybe_unused]] auto failure = legible_adv.IntoPayload(), "");
}

TEST_F(NpCppTest, LegibleAdvDestructor) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  {
    auto legible_adv = CreateLegibleAdv(book);
    auto legible_adv2 = CreateLegibleAdv(book);
    auto allocations =
        nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
    ASSERT_EQ(allocations.v0_payload, 2U);
  }
  // Verify the handles were de-allocated when legible advs went out of scope
  auto allocations =
      nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
  ASSERT_EQ(allocations.v0_payload, 0U);
}

nearby_protocol::V0Payload CreatePayload(
    nearby_protocol::CredentialBook &book) {
  auto legible_adv = CreateLegibleAdv(book);
  return legible_adv.IntoPayload();
}

TEST_F(NpCppTest, TestV0PayloadMoveConstructor) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  ASSERT_EQ(result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);
  auto payload = result.IntoV0().IntoLegible().IntoPayload();

  // Now move the adv into a new value, and make sure its still valid
  const nearby_protocol::V0Payload moved(std::move(payload));
  ASSERT_TRUE(moved.TryGetDataElement(0).ok());
  ASSERT_TRUE(absl::IsOutOfRange(moved.TryGetDataElement(1).status()));

  // trying to use the moved object should result in a use after free which
  // triggers an abort
  ASSERT_DEATH(
      [[maybe_unused]] auto failure =
          payload.TryGetDataElement(  // NOLINT(bugprone-use-after-move)
              0),
      "");

  // moving again should still preserve the moved state and also lead to an
  // abort
  const nearby_protocol::V0Payload moved_again(std::move(payload));
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.TryGetDataElement(0),
               "");
}

TEST_F(NpCppTest, TestV0PayloadMoveAssignment) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  ASSERT_EQ(result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);
  auto payload = result.IntoV0().IntoLegible().IntoPayload();

  // create a second result
  auto another_result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V0AdvPlaintext, book);
  ASSERT_EQ(another_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V0);
  auto payload2 = another_result.IntoV0().IntoLegible().IntoPayload();

  // original should be deallocated by assignment
  payload2 = std::move(payload);
  ASSERT_TRUE(payload2.TryGetDataElement(0).ok());

  // original result should now be invalid, using it will trigger a use after
  // free abort.
  ASSERT_DEATH(
      [[maybe_unused]] auto failure =
          payload.TryGetDataElement(  // NOLINT(bugprone-use-after-move)
              0),
      "");

  // moving again should still lead to an error
  auto moved_again = std::move(payload);
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.TryGetDataElement(0),
               "");
}

TEST_F(NpCppTest, V0PayloadDestructor) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  {
    auto payload = CreatePayload(book);
    auto payload2 = CreatePayload(book);

    // check that payload adv is valid even though its parent is out of scope
    ASSERT_TRUE(payload.TryGetDataElement(0).ok());
    ASSERT_TRUE(payload2.TryGetDataElement(0).ok());
    auto allocations =
        nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
    ASSERT_EQ(allocations.v0_payload, 2U);
  }

  // Verify the handle was de-allocated when legible advs went out of scope
  auto allocations =
      nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
  ASSERT_EQ(allocations.v0_payload, 0U);
}

TEST_F(NpCppTest, InvalidDataElementCast) {
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);

  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvPlaintext,
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
  ASSERT_DEATH([[maybe_unused]] auto failure = de.AsActions();, "");
}

TEST_F(NpCppTest, InvalidDataElementIndex) {
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);

  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V0AdvPlaintext,
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

  auto de_result = payload.TryGetDataElement(1);
  ASSERT_FALSE(de_result.ok());
  ASSERT_TRUE(absl::IsOutOfRange(de_result.status()));
}
