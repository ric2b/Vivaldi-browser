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

use crate::{unwrap, PanicReason};
use np_ffi_core::common::FixedSizeArray;
use np_ffi_core::deserialize::v1::*;
use np_ffi_core::deserialize::DecryptMetadataResult;
use np_ffi_core::utils::FfiEnum;

/// Gets the number of legible sections on a deserialized V1 advertisement.
/// Suitable as an index bound for the second argument of
/// `np_ffi_DeserializedV1Advertisement#get_section`.
#[no_mangle]
pub extern "C" fn np_ffi_DeserializedV1Advertisement_get_num_legible_sections(
    adv: DeserializedV1Advertisement,
) -> u8 {
    adv.num_legible_sections()
}

/// Gets the number of sections on a deserialized V1 advertisement which
/// were unable to be decrypted with the credentials that the receiver possesses.
#[no_mangle]
pub extern "C" fn np_ffi_DeserializedV1Advertisement_get_num_undecryptable_sections(
    adv: DeserializedV1Advertisement,
) -> u8 {
    adv.num_undecryptable_sections()
}

/// Gets the legible section with the given index in a deserialized V1 advertisement.
#[no_mangle]
pub extern "C" fn np_ffi_DeserializedV1Advertisement_get_section(
    adv: DeserializedV1Advertisement,
    legible_section_index: u8,
) -> GetV1SectionResult {
    adv.get_section(legible_section_index)
}

/// Gets the tag of the `GetV1SectionResult` tagged-union.
#[no_mangle]
pub extern "C" fn np_ffi_GetV1SectionResult_kind(
    result: GetV1SectionResult,
) -> GetV1SectionResultKind {
    result.kind()
}

/// Casts a `GetV1SectionResult` to the `Success` variant, panicking in the
/// case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_GetV1SectionResult_into_SUCCESS(
    result: GetV1SectionResult,
) -> DeserializedV1Section {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}

/// Gets the number of data elements in a deserialized v1 section.
/// Suitable as an iteration bound for the second argument of
/// `np_ffi_DeserializedV1Section_get_de`.
#[no_mangle]
pub extern "C" fn np_ffi_DeserializedV1Section_get_num_des(section: DeserializedV1Section) -> u8 {
    section.num_des()
}

/// Gets the tag of the identity tagged-union used for the passed section.
#[no_mangle]
pub extern "C" fn np_ffi_DeserializedV1Section_get_identity_kind(
    section: DeserializedV1Section,
) -> DeserializedV1IdentityKind {
    section.identity_kind()
}

/// Gets the data-element with the given index in the passed section.
#[no_mangle]
pub extern "C" fn np_ffi_DeserializedV1Section_get_de(
    section: DeserializedV1Section,
    de_index: u8,
) -> GetV1DEResult {
    section.get_de(de_index)
}

/// Gets the identity details used to decrypt this V1 section, or returns an error if this payload
/// does not have any associated identity (public advertisement)
#[no_mangle]
pub extern "C" fn np_ffi_DeserializedV1Section_get_identity_details(
    section: DeserializedV1Section,
) -> GetV1IdentityDetailsResult {
    section.get_identity_details()
}

/// Gets the tag of a `GetV1IdentityDetailsResult` tagged-union. On success the wrapped identity
/// details may be obtained via `GetV0IdentityDetailsResult#into_success`.
#[no_mangle]
pub extern "C" fn np_ffi_GetV1IdentityDetailsResult_kind(
    result: GetV1IdentityDetailsResult,
) -> GetV1IdentityDetailsResultKind {
    result.kind()
}

/// Casts a `GetV1IdentityDetailsResult` to the `Success` variant, panicking in the
/// case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_GetV1IdentityDetailsResult_into_SUCCESS(
    result: GetV1IdentityDetailsResult,
) -> DeserializedV1IdentityDetails {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}

/// Attempts to decrypt the metadata for the matched credential for this V0 payload (if any)
#[no_mangle]
pub extern "C" fn np_ffi_DeserializedV1Section_decrypt_metadata(
    section: DeserializedV1Section,
) -> DecryptMetadataResult {
    section.decrypt_metadata()
}

/// Attempts to derive a 16-byte DE salt for a DE in this section with the given DE offset. This
/// operation may fail if the passed offset is 255 (causes overflow) or if the section
/// is leveraging a public identity, and hence, doesn't have an associated salt.
#[no_mangle]
pub extern "C" fn np_ffi_DeserializedV1Section_derive_16_byte_salt_for_offset(
    section: DeserializedV1Section,
    offset: u8,
) -> GetV1DE16ByteSaltResult {
    section.derive_16_byte_salt_for_offset(offset)
}

/// Gets the tag of a `GetV1DE16ByteSaltResult` tagged-union. On success the wrapped identity
/// details may be obtained via `GetV1DE16ByteSaltResult#into_success`.
#[no_mangle]
pub extern "C" fn np_ffi_GetV1DE16ByteSaltResult_kind(
    result: GetV1DE16ByteSaltResult,
) -> GetV1DE16ByteSaltResultKind {
    result.kind()
}

/// Casts a `GetV1DE16ByteSaltResult` to the `Success` variant, panicking in the
/// case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_GetV1DE16ByteSaltResult_into_SUCCESS(
    result: GetV1DE16ByteSaltResult,
) -> FixedSizeArray<16> {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}

/// Gets the tag of the `GetV1DEResult` tagged-union.
#[no_mangle]
pub extern "C" fn np_ffi_GetV1DEResult_kind(result: GetV1DEResult) -> GetV1DEResultKind {
    result.kind()
}

/// Casts a `GetV1DEResult` to the `Success` variant, panicking in the
/// case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_GetV1DEResult_into_SUCCESS(result: GetV1DEResult) -> V1DataElement {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}
