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

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "np_cpp_ffi_functions.h"
#include "np_cpp_ffi_types.h"

namespace nearby_protocol {

void panic_handler(PanicReason reason);

struct PanicHandler {
  void (*handler)(PanicReason);
  bool set_by_client;
};

PanicHandler gPanicHandler = PanicHandler{panic_handler, false};

// C++ layer internal panic handler
void panic_handler(PanicReason reason) {
  // Give clients a chance to use their own panic handler, but if they don't
  // terminate the process we will make sure it happens.
  if (gPanicHandler.set_by_client) {
    gPanicHandler.handler(reason);
  }
  std::abort();
}

void _assert_panic(bool condition, const char* func, const char* file,
                   int line) {
  if (!condition) {
    std::cout << "Assert failed: \n function: " << func << "\n file: " << file
              << "\n line: " << line << "\n";
    panic_handler(PanicReason::AssertFailed);
  }
}

#define assert_panic(e) _assert_panic(e, __func__, __FILE__, __LINE__)

bool GlobalConfig::SetPanicHandler(void (*handler)(PanicReason)) {
  if (!gPanicHandler.set_by_client) {
    gPanicHandler.handler = handler;
    gPanicHandler.set_by_client = true;
    return np_ffi::internal::np_ffi_global_config_panic_handler(panic_handler);
  }
  return false;
}

void GlobalConfig::SetNumShards(const uint8_t num_shards) {
  np_ffi::internal::np_ffi_global_config_set_num_shards(num_shards);
}

CurrentHandleAllocations GlobalConfig::GetCurrentHandleAllocationCount() {
  return np_ffi::internal::np_ffi_global_config_get_current_allocation_count();
}

CredentialSlab::CredentialSlab() {
  this->credential_slab_ = np_ffi::internal::np_ffi_create_credential_slab();
  this->moved_ = false;
}

CredentialSlab::~CredentialSlab() {
  if (!this->moved_) {
    auto result =
        np_ffi::internal::np_ffi_deallocate_credential_slab(credential_slab_);
    assert_panic(result == np_ffi::internal::DeallocateResult::Success);
  }
}

CredentialSlab::CredentialSlab(CredentialSlab&& other) noexcept
    : credential_slab_(other.credential_slab_), moved_(other.moved_) {
  other.credential_slab_ = {};
  other.moved_ = true;
}

CredentialSlab& CredentialSlab::operator=(CredentialSlab&& other) noexcept {
  if (this != &other) {
    if (!this->moved_) {
      auto result = np_ffi::internal::np_ffi_deallocate_credential_slab(
          this->credential_slab_);
      assert_panic(result == np_ffi::internal::DeallocateResult::Success);
    }

    this->credential_slab_ = other.credential_slab_;
    this->moved_ = other.moved_;

    other.credential_slab_ = {};
    other.moved_ = true;
  }
  return *this;
}

void CredentialSlab::AddV0Credential(const V0MatchableCredential v0_cred) {
  assert_panic(!this->moved_);
  auto result = np_ffi::internal::np_ffi_CredentialSlab_add_v0_credential(
      this->credential_slab_, v0_cred.internal_);
  // Add V0 is infallible since the handle is guaranteed to be correct by the
  // C++ wrapper
  assert_panic(result == AddV0CredentialToSlabResult::Success);
}

absl::Status CredentialSlab::AddV1Credential(
    const V1MatchableCredential v1_cred) {
  assert_panic(!this->moved_);
  auto result = np_ffi::internal::np_ffi_CredentialSlab_add_v1_credential(
      this->credential_slab_, v1_cred.internal_);
  switch (result) {
    case AddV1CredentialToSlabResult::Success: {
      return absl::OkStatus();
    }
    case AddV1CredentialToSlabResult::InvalidHandle: {
      return absl::InvalidArgumentError(
          "invalid credential slab handle provided");
    }
    case AddV1CredentialToSlabResult::InvalidPublicKeyBytes: {
      return absl::InvalidArgumentError(
          "Invalid public key bytes in credential");
    }
  }
}

CredentialBook::CredentialBook(CredentialSlab& slab) {
  assert_panic(!slab.moved_);
  auto result = np_ffi::internal::np_ffi_create_credential_book_from_slab(
      slab.credential_slab_);
  auto kind = np_ffi::internal::np_ffi_CreateCredentialBookResult_kind(result);
  // Handle is guaranteed to be valid by the C++ wrapper
  assert_panic(kind == CreateCredentialBookResultKind::Success);
  slab.moved_ = true;
  this->credential_book_ =
      np_ffi::internal::np_ffi_CreateCredentialBookResult_into_SUCCESS(result);
  this->moved_ = false;
}

CredentialBook::~CredentialBook() {
  if (!this->moved_) {
    auto result =
        np_ffi::internal::np_ffi_deallocate_credential_book(credential_book_);
    assert_panic(result == np_ffi::internal::DeallocateResult::Success);
  }
}

CredentialBook::CredentialBook(CredentialBook&& other) noexcept
    : credential_book_(other.credential_book_), moved_(other.moved_) {
  other.credential_book_ = {};
  other.moved_ = true;
}

CredentialBook& CredentialBook::operator=(CredentialBook&& other) noexcept {
  if (this != &other) {
    if (!this->moved_) {
      auto result = np_ffi::internal::np_ffi_deallocate_credential_book(
          this->credential_book_);
      assert_panic(result == np_ffi::internal::DeallocateResult::Success);
    }

    this->credential_book_ = other.credential_book_;
    this->moved_ = other.moved_;

    other.credential_book_ = {};
    other.moved_ = true;
  }
  return *this;
}

DeserializeAdvertisementResult Deserializer::DeserializeAdvertisement(
    const RawAdvertisementPayload& payload,
    const CredentialBook& credential_book) {
  assert_panic(!credential_book.moved_);
  auto result = np_ffi::internal::np_ffi_deserialize_advertisement(
      {payload.buffer_.internal_}, credential_book.credential_book_);
  return DeserializeAdvertisementResult(result);
}

DeserializeAdvertisementResultKind DeserializeAdvertisementResult::GetKind()
    const {
  assert_panic(!this->moved_);
  return np_ffi::internal::np_ffi_DeserializeAdvertisementResult_kind(result_);
}

DeserializedV0Advertisement DeserializeAdvertisementResult::IntoV0() {
  assert_panic(!this->moved_);
  auto result =
      np_ffi::internal::np_ffi_DeserializeAdvertisementResult_into_V0(result_);
  this->moved_ = true;
  return DeserializedV0Advertisement(result);
}

DeserializedV1Advertisement DeserializeAdvertisementResult::IntoV1() {
  assert_panic(!this->moved_);
  auto v1_adv =
      np_ffi::internal::np_ffi_DeserializeAdvertisementResult_into_V1(result_);
  this->moved_ = true;
  return DeserializedV1Advertisement(v1_adv);
}

DeserializeAdvertisementResult::~DeserializeAdvertisementResult() {
  if (!this->moved_) {
    auto result =
        np_ffi::internal::np_ffi_deallocate_deserialize_advertisement_result(
            result_);
    assert_panic(result == np_ffi::internal::DeallocateResult::Success);
  }
}

DeserializeAdvertisementResult::DeserializeAdvertisementResult(
    DeserializeAdvertisementResult&& other) noexcept
    : result_(other.result_), moved_(other.moved_) {
  other.result_ = {};
  other.moved_ = true;
}

DeserializeAdvertisementResult& DeserializeAdvertisementResult::operator=(
    DeserializeAdvertisementResult&& other) noexcept {
  if (this != &other) {
    if (!this->moved_) {
      auto result =
          np_ffi::internal::np_ffi_deallocate_deserialize_advertisement_result(
              result_);
      assert_panic(result == np_ffi::internal::DeallocateResult::Success);
    }
    this->result_ = other.result_;
    this->moved_ = other.moved_;

    other.result_ = {};
    other.moved_ = true;
  }
  return *this;
}

// V0 Stuff
DeserializedV0Advertisement::DeserializedV0Advertisement(
    DeserializedV0Advertisement&& other) noexcept
    : v0_advertisement_(other.v0_advertisement_), moved_(other.moved_) {
  other.v0_advertisement_ = {};
  other.moved_ = true;
}

DeserializedV0Advertisement& DeserializedV0Advertisement::operator=(
    DeserializedV0Advertisement&& other) noexcept {
  if (this != &other) {
    if (!this->moved_) {
      auto result =
          np_ffi::internal::np_ffi_deallocate_deserialized_V0_advertisement(
              v0_advertisement_);
      assert_panic(result == np_ffi::internal::DeallocateResult::Success);
    }
    this->v0_advertisement_ = other.v0_advertisement_;
    this->moved_ = other.moved_;

    other.v0_advertisement_ = {};
    other.moved_ = true;
  }
  return *this;
}

DeserializedV0Advertisement::~DeserializedV0Advertisement() {
  if (!this->moved_) {
    auto result =
        np_ffi::internal::np_ffi_deallocate_deserialized_V0_advertisement(
            v0_advertisement_);
    assert_panic(result == np_ffi::internal::DeallocateResult::Success);
  }
}

np_ffi::internal::DeserializedV0AdvertisementKind
DeserializedV0Advertisement::GetKind() const {
  assert_panic(!this->moved_);
  return np_ffi::internal::np_ffi_DeserializedV0Advertisement_kind(
      v0_advertisement_);
}

LegibleDeserializedV0Advertisement DeserializedV0Advertisement::IntoLegible() {
  assert_panic(!this->moved_);
  auto result =
      np_ffi::internal::np_ffi_DeserializedV0Advertisement_into_LEGIBLE(
          v0_advertisement_);
  this->moved_ = true;
  this->v0_advertisement_ = {};
  return LegibleDeserializedV0Advertisement(result);
}

LegibleDeserializedV0Advertisement::LegibleDeserializedV0Advertisement(
    LegibleDeserializedV0Advertisement&& other) noexcept
    : legible_v0_advertisement_(other.legible_v0_advertisement_),
      moved_(other.moved_) {
  other.moved_ = true;
  other.legible_v0_advertisement_ = {};
}

LegibleDeserializedV0Advertisement&
LegibleDeserializedV0Advertisement::operator=(
    LegibleDeserializedV0Advertisement&& other) noexcept {
  if (this != &other) {
    if (!this->moved_) {
      auto result =
          np_ffi::internal::np_ffi_deallocate_legible_v0_advertisement(
              this->legible_v0_advertisement_);
      assert_panic(result == np_ffi::internal::DeallocateResult::Success);
    }
    this->legible_v0_advertisement_ = other.legible_v0_advertisement_;
    this->moved_ = other.moved_;

    other.moved_ = true;
    other.legible_v0_advertisement_ = {};
  }
  return *this;
}

LegibleDeserializedV0Advertisement::~LegibleDeserializedV0Advertisement() {
  if (!this->moved_) {
    auto result = np_ffi::internal::np_ffi_deallocate_legible_v0_advertisement(
        this->legible_v0_advertisement_);
    assert_panic(result == np_ffi::internal::DeallocateResult::Success);
  }
}

DeserializedV0IdentityKind LegibleDeserializedV0Advertisement::GetIdentityKind()
    const {
  assert_panic(!this->moved_);
  auto result = np_ffi::internal::
      np_ffi_LegibleDeserializedV0Advertisement_get_identity_kind(
          legible_v0_advertisement_);
  return result;
}

uint8_t LegibleDeserializedV0Advertisement::GetNumberOfDataElements() const {
  assert_panic(!this->moved_);
  return np_ffi::internal::
      np_ffi_LegibleDeserializedV0Advertisement_get_num_des(
          legible_v0_advertisement_);
}

V0Payload LegibleDeserializedV0Advertisement::IntoPayload() {
  assert_panic(!this->moved_);
  auto result = np_ffi_LegibleDeserializedV0Advertisement_into_payload(
      legible_v0_advertisement_);
  this->moved_ = true;
  return V0Payload(result);
}

V0Payload::V0Payload(V0Payload&& other) noexcept
    : v0_payload_(other.v0_payload_), moved_(other.moved_) {
  other.v0_payload_ = {};
  other.moved_ = true;
}

V0Payload& V0Payload::operator=(V0Payload&& other) noexcept {
  if (this != &other) {
    if (!this->moved_) {
      auto result =
          np_ffi::internal::np_ffi_deallocate_v0_payload(this->v0_payload_);
      assert_panic(result == np_ffi::internal::DeallocateResult::Success);
    }
    this->v0_payload_ = other.v0_payload_;
    this->moved_ = other.moved_;

    other.moved_ = true;
    other.v0_payload_ = {};
  }
  return *this;
}

V0Payload::~V0Payload() {
  if (!this->moved_) {
    auto result =
        np_ffi::internal::np_ffi_deallocate_v0_payload(this->v0_payload_);
    assert_panic(result == np_ffi::internal::DeallocateResult::Success);
  }
}

absl::StatusOr<V0DataElement> V0Payload::TryGetDataElement(
    const uint8_t index) const {
  assert_panic(!this->moved_);
  auto result = np_ffi::internal::np_ffi_V0Payload_get_de(v0_payload_, index);
  auto kind = np_ffi::internal::np_ffi_GetV0DEResult_kind(result);
  switch (kind) {
    case GetV0DEResultKind::Success: {
      auto de = np_ffi_GetV0DEResult_into_SUCCESS(result);
      return V0DataElement(de);
    }
    case GetV0DEResultKind::Error: {
      return absl::OutOfRangeError("Invalid Data Element index");
    }
  }
}

absl::StatusOr<std::vector<uint8_t>> MetadataResultToVec(
    np_ffi::internal::DecryptMetadataResult decrypt_result) {
  auto kind =
      np_ffi::internal::np_ffi_DecryptMetadataResult_kind(decrypt_result);
  switch (kind) {
    case np_ffi::internal::DecryptMetadataResultKind::Success: {
      auto metadata =
          np_ffi::internal::np_ffi_DecryptMetadataResult_into_SUCCESS(
              decrypt_result);
      auto parts_result =
          np_ffi::internal::np_ffi_DecryptedMetadata_get_metadata_buffer_parts(
              metadata);
      // The handle is guaranteed to be valid by the C++ wrapper so this should
      // never fail
      assert_panic(np_ffi::internal::np_ffi_GetMetadataBufferPartsResult_kind(
                       parts_result) ==
                   np_ffi::internal::GetMetadataBufferPartsResultKind::Success);
      auto parts =
          np_ffi::internal::np_ffi_GetMetadataBufferPartsResult_into_SUCCESS(
              parts_result);
      std::vector<uint8_t> result(parts.ptr, parts.ptr + parts.len);

      // Now that the contents have been copied into the vec, the underlying
      // handle can be de-allocated
      auto deallocate_result =
          np_ffi::internal::np_ffi_deallocate_DecryptedMetadata(metadata);
      assert_panic(deallocate_result ==
                   np_ffi::internal::DeallocateResult::Success);
      return result;
    }
    case np_ffi::internal::DecryptMetadataResultKind::Error: {
      return absl::InvalidArgumentError(
          "Decrypt attempt failed, identity is missing or invalid");
    }
  }
}

absl::StatusOr<DeserializedV0IdentityDetails> V0Payload::TryGetIdentityDetails()
    const {
  assert_panic(!this->moved_);
  auto result = np_ffi::internal::np_ffi_V0Payload_get_identity_details(
      this->v0_payload_);
  auto kind = np_ffi::internal::np_ffi_GetV0IdentityDetailsResult_kind(result);
  switch (kind) {
    case np_ffi::internal::GetV0IdentityDetailsResultKind::Error: {
      return absl::InvalidArgumentError(
          "Identity details not available for public advertisements");
    }
    case np_ffi::internal::GetV0IdentityDetailsResultKind::Success: {
      return np_ffi::internal::np_ffi_GetV0IdentityDetailsResult_into_SUCCESS(
          result);
    }
  }
}

absl::StatusOr<std::vector<uint8_t>> V0Payload::TryDecryptMetadata() const {
  assert_panic(!this->moved_);
  auto decrypt_result =
      np_ffi::internal::np_ffi_V0Payload_decrypt_metadata(this->v0_payload_);
  return MetadataResultToVec(decrypt_result);
}

V0DataElementKind V0DataElement::GetKind() const {
  return np_ffi::internal::np_ffi_V0DataElement_kind(v0_data_element_);
}

TxPower V0DataElement::AsTxPower() const {
  return TxPower(
      np_ffi::internal::np_ffi_V0DataElement_into_TX_POWER(v0_data_element_));
}

V0Actions V0DataElement::AsActions() const {
  auto internal =
      np_ffi::internal::np_ffi_V0DataElement_into_ACTIONS(v0_data_element_);
  return V0Actions(internal);
}

V0DataElement::V0DataElement(TxPower tx_power) {
  v0_data_element_ =
      np_ffi::internal::np_ffi_TxPower_into_V0DataElement(tx_power.tx_power_);
}

V0DataElement::V0DataElement(V0Actions actions) {
  v0_data_element_ =
      np_ffi::internal::np_ffi_V0Actions_into_V0DataElement(actions.actions_);
}

uint32_t V0Actions::GetAsU32() const {
  return np_ffi::internal::np_ffi_V0Actions_as_u32(actions_);
}

bool V0Actions::HasAction(ActionType action) const {
  return np_ffi::internal::np_ffi_V0Actions_has_action(actions_, action);
}

absl::Status V0Actions::TrySetAction(ActionType action, bool value) {
  auto result =
      np_ffi::internal::np_ffi_V0Actions_set_action(actions_, action, value);
  auto kind = np_ffi::internal::np_ffi_SetV0ActionResult_kind(result);
  switch (kind) {
    case np_ffi::internal::SetV0ActionResultKind::Success: {
      actions_ =
          np_ffi::internal::np_ffi_SetV0ActionResult_into_SUCCESS(result);
      return absl::OkStatus();
    }
    case np_ffi::internal::SetV0ActionResultKind::Error: {
      actions_ = np_ffi::internal::np_ffi_SetV0ActionResult_into_ERROR(result);
      return absl::InvalidArgumentError(
          "The requested action bit may not be set for the requested adv "
          "encoding");
    }
  }
}

V0Actions V0Actions::BuildNewZeroed(AdvertisementBuilderKind kind) {
  auto actions = np_ffi::internal::np_ffi_build_new_zeroed_V0Actions(kind);
  return V0Actions(actions);
}

int8_t TxPower::GetAsI8() const {
  return np_ffi::internal::np_ffi_TxPower_as_signed_byte(tx_power_);
}

absl::StatusOr<TxPower> TxPower::TryBuildFromI8(int8_t value) {
  auto result = np_ffi::internal::np_ffi_TxPower_build_from_signed_byte(value);
  auto kind = np_ffi::internal::np_ffi_BuildTxPowerResult_kind(result);
  switch (kind) {
    case np_ffi::internal::BuildTxPowerResultKind::Success: {
      return TxPower(
          np_ffi::internal::np_ffi_BuildTxPowerResult_into_SUCCESS(result));
    }
    case np_ffi::internal::BuildTxPowerResultKind::OutOfRange: {
      return absl::InvalidArgumentError(
          "Could not build a tx power for the requested byte value.");
    }
  }
}

// This is called after all references to the shared_ptr have gone out of
// scope
auto DeallocateV1Adv(
    np_ffi::internal::DeserializedV1Advertisement* v1_advertisement) {
  auto result =
      np_ffi::internal::np_ffi_deallocate_deserialized_V1_advertisement(
          *v1_advertisement);
  assert_panic(result == np_ffi::internal::DeallocateResult::Success);
  delete v1_advertisement;
}

DeserializedV1Advertisement::DeserializedV1Advertisement(
    np_ffi::internal::DeserializedV1Advertisement v1_advertisement) {
  v1_advertisement_ =
      std::shared_ptr<np_ffi::internal::DeserializedV1Advertisement>(
          new np_ffi::internal::DeserializedV1Advertisement(v1_advertisement),
          DeallocateV1Adv);
}

DeserializedV1Advertisement::DeserializedV1Advertisement(
    DeserializedV1Advertisement&& other) noexcept
    : v1_advertisement_(std::move(other.v1_advertisement_)) {}

DeserializedV1Advertisement& DeserializedV1Advertisement::operator=(
    DeserializedV1Advertisement&& other) noexcept {
  if (this != &other) {
    this->v1_advertisement_ = std::move(other.v1_advertisement_);
  }
  return *this;
}

// V1 Stuff
uint8_t DeserializedV1Advertisement::GetNumLegibleSections() const {
  assert_panic(this->v1_advertisement_ != nullptr);
  return np_ffi::internal::
      np_ffi_DeserializedV1Advertisement_get_num_legible_sections(
          *v1_advertisement_);
}

uint8_t DeserializedV1Advertisement::GetNumUndecryptableSections() const {
  assert_panic(this->v1_advertisement_ != nullptr);
  return np_ffi::internal::
      np_ffi_DeserializedV1Advertisement_get_num_undecryptable_sections(
          *v1_advertisement_);
}

absl::StatusOr<DeserializedV1Section>
DeserializedV1Advertisement::TryGetSection(const uint8_t section_index) const {
  assert_panic(this->v1_advertisement_ != nullptr);
  auto result =
      np_ffi::internal::np_ffi_DeserializedV1Advertisement_get_section(
          *v1_advertisement_, section_index);
  auto kind = np_ffi::internal::np_ffi_GetV1SectionResult_kind(result);
  switch (kind) {
    case np_ffi::internal::GetV1SectionResultKind::Error: {
      return absl::OutOfRangeError("Invalid section index");
    }
    case np_ffi::internal::GetV1SectionResultKind::Success: {
      auto section =
          np_ffi::internal::np_ffi_GetV1SectionResult_into_SUCCESS(result);
      return DeserializedV1Section(section, v1_advertisement_);
    }
  }
}

uint8_t DeserializedV1Section::NumberOfDataElements() const {
  return np_ffi::internal::np_ffi_DeserializedV1Section_get_num_des(section_);
}

DeserializedV1IdentityKind DeserializedV1Section::GetIdentityKind() const {
  return np_ffi::internal::np_ffi_DeserializedV1Section_get_identity_kind(
      section_);
}

absl::StatusOr<V1DataElement> DeserializedV1Section::TryGetDataElement(
    const uint8_t index) const {
  auto result =
      np_ffi::internal::np_ffi_DeserializedV1Section_get_de(section_, index);
  auto kind = np_ffi::internal::np_ffi_GetV1DEResult_kind(result);
  switch (kind) {
    case np_ffi::internal::GetV1DEResultKind::Error: {
      return absl::OutOfRangeError(
          "Invalid data element index for this section");
    }
    case np_ffi::internal::GetV1DEResultKind::Success: {
      return V1DataElement(
          np_ffi::internal::np_ffi_GetV1DEResult_into_SUCCESS(result));
    }
  }
}

absl::StatusOr<std::vector<uint8_t>> DeserializedV1Section::TryDecryptMetadata()
    const {
  assert_panic(this->owning_v1_advertisement_ != nullptr);
  auto decrypt_result =
      np_ffi::internal::np_ffi_DeserializedV1Section_decrypt_metadata(
          this->section_);
  return MetadataResultToVec(decrypt_result);
}

absl::StatusOr<DeserializedV1IdentityDetails>
DeserializedV1Section::GetIdentityDetails() const {
  assert_panic(this->owning_v1_advertisement_ != nullptr);
  auto result =
      np_ffi::internal::np_ffi_DeserializedV1Section_get_identity_details(
          this->section_);
  auto kind = np_ffi::internal::np_ffi_GetV1IdentityDetailsResult_kind(result);
  switch (kind) {
    case np_ffi::internal::GetV1IdentityDetailsResultKind::Error: {
      return absl::InvalidArgumentError(
          "Identity details are not available for public advertisements");
    }
    case np_ffi::internal::GetV1IdentityDetailsResultKind::Success: {
      return np_ffi::internal::np_ffi_GetV1IdentityDetailsResult_into_SUCCESS(
          result);
    }
  }
}

absl::StatusOr<std::array<uint8_t, DERIVED_SALT_SIZE>>
DeserializedV1Section::DeriveSaltForOffset(const uint8_t offset) const {
  auto result = np_ffi::internal::
      np_ffi_DeserializedV1Section_derive_16_byte_salt_for_offset(
          this->section_, offset);
  auto kind = np_ffi::internal::np_ffi_GetV1DE16ByteSaltResult_kind(result);
  switch (kind) {
    case np_ffi::internal::GetV1DE16ByteSaltResultKind::Error: {
      return absl::InvalidArgumentError("Failed to derive salt for offset");
    }
    case np_ffi::internal::GetV1DE16ByteSaltResultKind::Success: {
      auto buffer =
          np_ffi::internal::np_ffi_GetV1DE16ByteSaltResult_into_SUCCESS(result);
      return std::to_array(buffer._0);
    }
  }
}

uint32_t V1DataElement::GetDataElementTypeCode() const {
  return np_ffi::internal::np_ffi_V1DEType_to_uint32_t(
      np_ffi::internal::np_ffi_V1DataElement_to_generic(this->v1_data_element_)
          .de_type);
}

ByteBuffer<MAX_V1_DE_PAYLOAD_SIZE> V1DataElement::GetPayload() const {
  return ByteBuffer(
      np_ffi::internal::np_ffi_V1DataElement_to_generic(this->v1_data_element_)
          .payload);
}

uint8_t V1DataElement::GetOffset() const {
  return np_ffi::internal::np_ffi_V1DataElement_to_generic(
             this->v1_data_element_)
      .offset;
}

MatchedCredentialData::MatchedCredentialData(
    const uint32_t cred_id, std::span<const uint8_t> metadata_bytes) {
  this->data_ = {cred_id, metadata_bytes.data(), metadata_bytes.size()};
}

template <typename T, size_t N>
void CopyToRawArray(T (&dest)[N], const std::array<T, N>& src) {
  memcpy(dest, src.data(), sizeof(T) * N);
}

V0MatchableCredential::V0MatchableCredential(
    const std::array<uint8_t, 32> key_seed,
    const std::array<uint8_t, 32> legacy_metadata_key_hmac,
    const MatchedCredentialData matched_credential_data) {
  np_ffi::internal::V0DiscoveryCredential discovery_cred{};
  CopyToRawArray(discovery_cred.key_seed, key_seed);
  CopyToRawArray(discovery_cred.identity_token_hmac, legacy_metadata_key_hmac);
  this->internal_ = {discovery_cred, matched_credential_data.data_};
}

V1MatchableCredential::V1MatchableCredential(
    const std::array<uint8_t, 32> key_seed,
    const std::array<uint8_t, 32>
        expected_mic_extended_salt_identity_token_hmac,
    const std::array<uint8_t, 32> expected_signature_identity_token_hmac,
    const std::array<uint8_t, 32> pub_key,
    const MatchedCredentialData matched_credential_data) {
  np_ffi::internal::V1DiscoveryCredential discovery_cred{};
  CopyToRawArray(discovery_cred.key_seed, key_seed);
  CopyToRawArray(discovery_cred.expected_mic_extended_salt_identity_token_hmac,
                 expected_mic_extended_salt_identity_token_hmac);
  CopyToRawArray(discovery_cred.expected_signature_identity_token_hmac,
                 expected_signature_identity_token_hmac);
  CopyToRawArray(discovery_cred.pub_key, pub_key);
  this->internal_ = {discovery_cred, matched_credential_data.data_};
}

V0BroadcastCredential::V0BroadcastCredential(
    std::array<uint8_t, 32> key_seed, std::array<uint8_t, 14> identity_token) {
  CopyToRawArray(internal_.key_seed, key_seed);
  CopyToRawArray(internal_.identity_token, identity_token);
}

V0AdvertisementBuilder::~V0AdvertisementBuilder() {
  if (!this->moved_) {
    auto result = np_ffi::internal::np_ffi_deallocate_v0_advertisement_builder(
        adv_builder_);
    assert_panic(result == np_ffi::internal::DeallocateResult::Success);
  }
}

V0AdvertisementBuilder::V0AdvertisementBuilder(
    V0AdvertisementBuilder&& other) noexcept
    : adv_builder_(other.adv_builder_), moved_(other.moved_) {
  other.adv_builder_ = {};
  other.moved_ = true;
}

V0AdvertisementBuilder& V0AdvertisementBuilder::operator=(
    V0AdvertisementBuilder&& other) noexcept {
  if (this != &other) {
    if (!this->moved_) {
      auto result =
          np_ffi::internal::np_ffi_deallocate_v0_advertisement_builder(
              this->adv_builder_);
      assert_panic(result == np_ffi::internal::DeallocateResult::Success);
    }

    this->adv_builder_ = other.adv_builder_;
    this->moved_ = other.moved_;

    other.adv_builder_ = {};
    other.moved_ = true;
  }
  return *this;
}

absl::Status V0AdvertisementBuilder::TryAddDE(V0DataElement de) {
  assert_panic(!this->moved_);
  auto result = np_ffi::internal::np_ffi_V0AdvertisementBuilder_add_de(
      this->adv_builder_, de.v0_data_element_);
  switch (result) {
    case AddV0DEResult::Success: {
      return absl::OkStatus();
    }
    case AddV0DEResult::InvalidAdvertisementBuilderHandle: {
      return absl::InvalidArgumentError(
          "invalid v0 advertisement builder handle provided");
    }
    case AddV0DEResult::InsufficientAdvertisementSpace: {
      return absl::ResourceExhaustedError(
          "insufficient advertisement space to add DE");
    }
    case AddV0DEResult::InvalidIdentityTypeForDataElement: {
      return absl::InvalidArgumentError(
          "the DE may not be added to this advertisement builder due to an "
          "identity type mismatch");
    }
  }
}

[[nodiscard]] absl::StatusOr<ByteBuffer<24>>
V0AdvertisementBuilder::TrySerialize() {
  assert_panic(!this->moved_);
  auto result =
      np_ffi::internal::np_ffi_V0AdvertisementBuilder_into_advertisement(
          this->adv_builder_);
  auto kind =
      np_ffi::internal::np_ffi_SerializeV0AdvertisementResult_kind(result);
  // Regardless of what happens, we've invalidated the original adv builder.
  this->moved_ = true;

  switch (kind) {
    case SerializeV0AdvertisementResultKind::Success: {
      auto bytes =
          np_ffi::internal::np_ffi_SerializeV0AdvertisementResult_into_SUCCESS(
              result);
      return ByteBuffer(bytes);
    }
    case SerializeV0AdvertisementResultKind::LdtError: {
      return absl::OutOfRangeError(
          "The advertisement contents were not of a proper size for LDT "
          "encryption");
    }
    case SerializeV0AdvertisementResultKind::UnencryptedError: {
      return absl::OutOfRangeError(
          "The advertisement contents did not meet the length requirements");
    }
    case SerializeV0AdvertisementResultKind::
        InvalidAdvertisementBuilderHandle: {
      return absl::InvalidArgumentError(
          "The advertisement builder handle was invalid");
    }
  }
}

[[nodiscard]] V0AdvertisementBuilder V0AdvertisementBuilder::CreatePublic() {
  auto result =
      np_ffi::internal::np_ffi_create_v0_public_advertisement_builder();
  return V0AdvertisementBuilder(result);
}

template <uintptr_t N>
[[nodiscard]] np_ffi::internal::FixedSizeArray<N> ToFFIArray(
    std::array<uint8_t, N> value) {
  np_ffi::internal::FixedSizeArray<N> result;
  std::copy(std::begin(value), std::end(value), result._0);
  return result;
}

[[nodiscard]] V0AdvertisementBuilder V0AdvertisementBuilder::CreateEncrypted(
    V0BroadcastCredential broadcast_cred, std::array<uint8_t, 2> salt) {
  auto result =
      np_ffi::internal::np_ffi_create_v0_encrypted_advertisement_builder(
          broadcast_cred.internal_, ToFFIArray(salt));
  return V0AdvertisementBuilder(result);
}

}  // namespace nearby_protocol
