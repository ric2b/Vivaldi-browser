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

#include "include/nearby_protocol.h"

#include "np_cpp_ffi_functions.h"

#ifdef __FILE_NAME__
#define __ASSERT_FILE_NAME __FILE_NAME__
#else /* __FILE_NAME__ */
#define __ASSERT_FILE_NAME __FILE__
#endif /* __FILE_NAME__ */

namespace nearby_protocol {

static void panic_handler(PanicReason reason);

struct PanicHandler {
  void (*handler)(PanicReason);
  bool set_by_client;
};

static PanicHandler gPanicHandler = PanicHandler{panic_handler, false};

// C++ layer internal panic handler
static void panic_handler(PanicReason reason) {
  // Give clients a chance to use their own panic handler, but if they don't
  // terminate the process we will make sure it happens.
  if (gPanicHandler.set_by_client) {
    gPanicHandler.handler(reason);
  }
  std::abort();
}

static void _assert_panic(bool condition, const char *func, const char *file,
                          int line) {
  if (!condition) {
    std::cout << "Assert failed: \n function: " << func << "\n file: " << file
              << "\n line: " << line << "\n";
    panic_handler(PanicReason::AssertFailed);
  }
}

#define assert_panic(e) _assert_panic(e, __func__, __ASSERT_FILE_NAME, __LINE__)

bool GlobalConfig::SetPanicHandler(void (*handler)(PanicReason)) {
  if (!gPanicHandler.set_by_client) {
    gPanicHandler.handler = handler;
    gPanicHandler.set_by_client = true;
    return np_ffi::internal::np_ffi_global_config_panic_handler(panic_handler);
  }
  return false;
}

void GlobalConfig::SetNumShards(uint8_t num_shards) {
  np_ffi::internal::np_ffi_global_config_set_num_shards(num_shards);
}

void GlobalConfig::SetMaxNumCredentialSlabs(uint32_t max_num_credential_slabs) {
  np_ffi::internal::np_ffi_global_config_set_max_num_credential_slabs(
      max_num_credential_slabs);
}

void GlobalConfig::SetMaxNumCredentialBooks(uint32_t max_num_credential_books) {
  np_ffi::internal::np_ffi_global_config_set_max_num_credential_books(
      max_num_credential_books);
}

void GlobalConfig::SetMaxNumDeserializedV0Advertisements(
    uint32_t max_num_deserialized_v0_advertisements) {
  np_ffi::internal::
      np_ffi_global_config_set_max_num_deserialized_v0_advertisements(
          max_num_deserialized_v0_advertisements);
}

void GlobalConfig::SetMaxNumDeserializedV1Advertisements(
    uint32_t max_num_deserialized_v1_advertisements) {
  np_ffi::internal::
      np_ffi_global_config_set_max_num_deserialized_v1_advertisements(
          max_num_deserialized_v1_advertisements);
}

absl::StatusOr<CredentialSlab> CredentialSlab::TryCreate() {
  auto result = np_ffi::internal::np_ffi_create_credential_slab();
  auto kind = np_ffi::internal::np_ffi_CreateCredentialSlabResult_kind(result);

  switch (kind) {
  case CreateCredentialSlabResultKind::Success: {
    auto slab = CredentialSlab(
        np_ffi::internal::np_ffi_CreateCredentialSlabResult_into_SUCCESS(
            result));
    return slab;
  }
  case CreateCredentialSlabResultKind::NoSpaceLeft: {
    return absl::ResourceExhaustedError(
        "No space left to create credential slab");
  }
  }
}

CredentialSlab::~CredentialSlab() {
  if (!this->moved_) {
    auto result =
        np_ffi::internal::np_ffi_deallocate_credential_slab(credential_slab_);
    assert_panic(result == np_ffi::internal::DeallocateResult::Success);
  }
}

CredentialSlab::CredentialSlab(CredentialSlab &&other) noexcept
    : credential_slab_(other.credential_slab_), moved_(other.moved_) {
  other.credential_slab_ = {};
  other.moved_ = true;
}

CredentialSlab &CredentialSlab::operator=(CredentialSlab &&other) noexcept {
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

absl::Status CredentialSlab::AddV0Credential(V0MatchableCredential v0_cred) {
  assert_panic(!this->moved_);
  auto result = np_ffi::internal::np_ffi_CredentialSlab_add_v0_credential(
      this->credential_slab_, v0_cred.internal_);
  switch (result) {
  case AddCredentialToSlabResult::Success: {
    return absl::OkStatus();
  }
  case AddCredentialToSlabResult::InvalidHandle: {
    return absl::InvalidArgumentError(
        "invalid credential slab handle provided");
  }
  }
}

absl::Status CredentialSlab::AddV1Credential(V1MatchableCredential v1_cred) {
  assert_panic(!this->moved_);
  auto result = np_ffi::internal::np_ffi_CredentialSlab_add_v1_credential(
      this->credential_slab_, v1_cred.internal_);
  switch (result) {
  case AddCredentialToSlabResult::Success: {
    return absl::OkStatus();
  }
  case AddCredentialToSlabResult::InvalidHandle: {
    return absl::InvalidArgumentError(
        "invalid credential slab handle provided");
  }
  }
}

absl::StatusOr<CredentialBook>
CredentialBook::TryCreateFromSlab(CredentialSlab &slab) {
  assert_panic(!slab.moved_);
  auto result = np_ffi::internal::np_ffi_create_credential_book_from_slab(
      slab.credential_slab_);
  auto kind = np_ffi::internal::np_ffi_CreateCredentialBookResult_kind(result);
  switch (kind) {
  case CreateCredentialBookResultKind::Success: {
    auto book =
        np_ffi::internal::np_ffi_CreateCredentialBookResult_into_SUCCESS(
            result);
    slab.moved_ = true;
    return CredentialBook(book);
  }
  case CreateCredentialBookResultKind::NoSpaceLeft: {
    return absl::ResourceExhaustedError(
        "No space left to create credential book");
  }
  case CreateCredentialBookResultKind::InvalidSlabHandle: {
    return absl::NotFoundError(
        "The slab referenced by the given handle was not found.");
  }
  }
}

CredentialBook::~CredentialBook() {
  if (!this->moved_) {
    auto result =
        np_ffi::internal::np_ffi_deallocate_credential_book(credential_book_);
    assert_panic(result == np_ffi::internal::DeallocateResult::Success);
  }
}

CredentialBook::CredentialBook(CredentialBook &&other) noexcept
    : credential_book_(other.credential_book_), moved_(other.moved_) {
  other.credential_book_ = {};
  other.moved_ = true;
}

CredentialBook &CredentialBook::operator=(CredentialBook &&other) noexcept {
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

DeserializeAdvertisementResult
Deserializer::DeserializeAdvertisement(RawAdvertisementPayload &payload,
                                       const CredentialBook &credential_book) {
  assert_panic(!credential_book.moved_);
  auto result = np_ffi::internal::np_ffi_deserialize_advertisement(
      {payload.buffer_.internal_}, credential_book.credential_book_);
  return DeserializeAdvertisementResult(result);
}

DeserializeAdvertisementResultKind DeserializeAdvertisementResult::GetKind() {
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
    DeserializeAdvertisementResult &&other) noexcept
    : result_(other.result_), moved_(other.moved_) {
  other.result_ = {};
  other.moved_ = true;
}

DeserializeAdvertisementResult &DeserializeAdvertisementResult::operator=(
    DeserializeAdvertisementResult &&other) noexcept {
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
    DeserializedV0Advertisement &&other) noexcept
    : v0_advertisement_(other.v0_advertisement_), moved_(other.moved_) {
  other.v0_advertisement_ = {};
  other.moved_ = true;
}

DeserializedV0Advertisement &DeserializedV0Advertisement::operator=(
    DeserializedV0Advertisement &&other) noexcept {
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
DeserializedV0Advertisement::GetKind() {
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
    LegibleDeserializedV0Advertisement &&other) noexcept
    : legible_v0_advertisement_(other.legible_v0_advertisement_),
      moved_(other.moved_) {
  other.moved_ = true;
  other.legible_v0_advertisement_ = {};
}

LegibleDeserializedV0Advertisement &
LegibleDeserializedV0Advertisement::operator=(
    LegibleDeserializedV0Advertisement &&other) noexcept {
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

DeserializedV0IdentityKind
LegibleDeserializedV0Advertisement::GetIdentityKind() {
  assert_panic(!this->moved_);
  auto result = np_ffi::internal::
      np_ffi_LegibleDeserializedV0Advertisement_get_identity_kind(
          legible_v0_advertisement_);
  return result;
}

uint8_t LegibleDeserializedV0Advertisement::GetNumberOfDataElements() {
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

V0Payload::V0Payload(V0Payload &&other) noexcept
    : v0_payload_(other.v0_payload_), moved_(other.moved_) {
  other.v0_payload_ = {};
  other.moved_ = true;
}

V0Payload &V0Payload::operator=(V0Payload &&other) noexcept {
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

absl::StatusOr<V0DataElement> V0Payload::TryGetDataElement(uint8_t index) {
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

V0DataElementKind V0DataElement::GetKind() {
  return np_ffi::internal::np_ffi_V0DataElement_kind(v0_data_element_);
}

TxPower V0DataElement::AsTxPower() {
  return np_ffi::internal::np_ffi_V0DataElement_into_TX_POWER(v0_data_element_);
}

V0Actions V0DataElement::AsActions() {
  auto internal =
      np_ffi::internal::np_ffi_V0DataElement_into_ACTIONS(v0_data_element_);
  return V0Actions(internal);
}

uint32_t V0Actions::GetAsU32() {
  return np_ffi::internal::np_ffi_V0Actions_as_u32(actions_);
}

bool V0Actions::HasAction(BooleanActionType action) {
  return np_ffi::internal::np_ffi_V0Actions_has_action(actions_, action);
}

uint8_t V0Actions::GetContextSyncSequenceNumber() {
  return np_ffi::internal::np_ffi_V0Actions_get_context_sync_sequence_number(
      actions_);
}

// This is called after all references to the shared_ptr have gone out of scope
auto DeallocateV1Adv(
    np_ffi::internal::DeserializedV1Advertisement *v1_advertisement) {
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
    DeserializedV1Advertisement &&other) noexcept
    : v1_advertisement_(std::move(other.v1_advertisement_)) {}

DeserializedV1Advertisement &DeserializedV1Advertisement::operator=(
    DeserializedV1Advertisement &&other) noexcept {
  if (this != &other) {
    this->v1_advertisement_ = std::move(other.v1_advertisement_);
  }
  return *this;
}

// V1 Stuff
uint8_t DeserializedV1Advertisement::GetNumLegibleSections() {
  assert_panic(this->v1_advertisement_ != nullptr);
  return np_ffi::internal::
      np_ffi_DeserializedV1Advertisement_get_num_legible_sections(
          *v1_advertisement_);
}

uint8_t DeserializedV1Advertisement::GetNumUndecryptableSections() {
  assert_panic(this->v1_advertisement_ != nullptr);
  return np_ffi::internal::
      np_ffi_DeserializedV1Advertisement_get_num_undecryptable_sections(
          *v1_advertisement_);
}

absl::StatusOr<DeserializedV1Section>
DeserializedV1Advertisement::TryGetSection(uint8_t section_index) {
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

uint8_t DeserializedV1Section::NumberOfDataElements() {
  return np_ffi::internal::np_ffi_DeserializedV1Section_get_num_des(section_);
}

DeserializedV1IdentityKind DeserializedV1Section::GetIdentityKind() {
  return np_ffi::internal::np_ffi_DeserializedV1Section_get_identity_kind(
      section_);
}

absl::StatusOr<V1DataElement>
DeserializedV1Section::TryGetDataElement(uint8_t index) {
  auto result =
      np_ffi::internal::np_ffi_DeserializedV1Section_get_de(section_, index);
  auto kind = np_ffi::internal::np_ffi_GetV1DEResult_kind(result);
  switch (kind) {
  case np_ffi::internal::GetV1DEResultKind::Error: {
    return absl::OutOfRangeError("Invalid data element index for this section");
  }
  case np_ffi::internal::GetV1DEResultKind::Success: {
    return V1DataElement(
        np_ffi::internal::np_ffi_GetV1DEResult_into_SUCCESS(result));
  }
  }
}

uint32_t V1DataElement::GetDataElementTypeCode() const {
  return np_ffi::internal::np_ffi_V1DEType_to_uint32_t(
      v1_data_element_.generic._0.de_type);
}

ByteBuffer<127> V1DataElement::GetPayload() const {
  return ByteBuffer(v1_data_element_.generic._0.payload);
}

MatchedCredentialData::MatchedCredentialData(
    uint32_t cred_id, std::span<uint8_t> metadata_bytes) {
  this->data_ = {cred_id, metadata_bytes.data(), metadata_bytes.size()};
}

template <typename T, size_t N>
static void CopyToRawArray(T (&dest)[N], const std::array<T, N> &src) {
  memcpy(dest, src.data(), sizeof(T) * N);
}

V0MatchableCredential::V0MatchableCredential(
    std::array<uint8_t, 32> key_seed,
    std::array<uint8_t, 32> legacy_metadata_key_hmac,
    MatchedCredentialData matched_credential_data) {
  np_ffi::internal::V0DiscoveryCredential discovery_cred{};
  CopyToRawArray(discovery_cred.key_seed, key_seed);
  CopyToRawArray(discovery_cred.legacy_metadata_key_hmac,
                 legacy_metadata_key_hmac);
  this->internal_ = {discovery_cred, matched_credential_data.data_};
}

V1MatchableCredential::V1MatchableCredential(
    std::array<uint8_t, 32> key_seed,
    std::array<uint8_t, 32> expected_unsigned_metadata_key_hmac,
    std::array<uint8_t, 32> expected_signed_metadata_key_hmac,
    std::array<uint8_t, 32> pub_key,
    MatchedCredentialData matched_credential_data) {
  np_ffi::internal::V1DiscoveryCredential discovery_cred{};
  CopyToRawArray(discovery_cred.key_seed, key_seed);
  CopyToRawArray(discovery_cred.expected_unsigned_metadata_key_hmac,
                 expected_unsigned_metadata_key_hmac);
  CopyToRawArray(discovery_cred.expected_signed_metadata_key_hmac,
                 expected_signed_metadata_key_hmac);
  CopyToRawArray(discovery_cred.pub_key, pub_key);
  this->internal_ = {discovery_cred, matched_credential_data.data_};
}
} // namespace nearby_protocol
