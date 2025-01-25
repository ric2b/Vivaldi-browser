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
use np_ffi_core::common::DeallocateResult;
use np_ffi_core::deserialize::v0::*;
use np_ffi_core::deserialize::DecryptMetadataResult;
use np_ffi_core::utils::FfiEnum;
use np_ffi_core::v0::*;

/// Gets the tag of a `DeserializedV0Advertisement` tagged-union.
#[no_mangle]
pub extern "C" fn np_ffi_DeserializedV0Advertisement_kind(
    result: DeserializedV0Advertisement,
) -> DeserializedV0AdvertisementKind {
    result.kind()
}

/// Casts a `DeserializedV0Advertisement` to the `Legible` variant, panicking in the
/// case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_DeserializedV0Advertisement_into_LEGIBLE(
    adv: DeserializedV0Advertisement,
) -> LegibleDeserializedV0Advertisement {
    unwrap(adv.into_legible(), PanicReason::EnumCastFailed)
}

/// Gets the number of DEs in a legible deserialized advertisement.
/// Suitable as an iteration bound for `V0Payload#get_de`.
#[no_mangle]
pub extern "C" fn np_ffi_LegibleDeserializedV0Advertisement_get_num_des(
    adv: LegibleDeserializedV0Advertisement,
) -> u8 {
    adv.num_des()
}

/// Gets just the data-element payload of a `LegibleDeserializedV0Advertisement`.
#[no_mangle]
pub extern "C" fn np_ffi_LegibleDeserializedV0Advertisement_into_payload(
    adv: LegibleDeserializedV0Advertisement,
) -> V0Payload {
    adv.payload()
}

/// Gets just the identity kind associated with a `LegibleDeserializedV0Advertisement`.
#[no_mangle]
pub extern "C" fn np_ffi_LegibleDeserializedV0Advertisement_get_identity_kind(
    adv: LegibleDeserializedV0Advertisement,
) -> DeserializedV0IdentityKind {
    adv.identity_kind()
}

/// Deallocates any internal data of a `LegibleDeserializedV0Advertisement`
#[no_mangle]
pub extern "C" fn np_ffi_deallocate_legible_v0_advertisement(
    adv: LegibleDeserializedV0Advertisement,
) -> DeallocateResult {
    adv.deallocate()
}

/// Attempts to get the data-element with the given index in the passed v0 adv payload
#[no_mangle]
pub extern "C" fn np_ffi_V0Payload_get_de(payload: V0Payload, index: u8) -> GetV0DEResult {
    payload.get_de(index)
}

/// Attempts to decrypt the metadata for the matched credential for this V0 payload (if any)
#[no_mangle]
pub extern "C" fn np_ffi_V0Payload_decrypt_metadata(payload: V0Payload) -> DecryptMetadataResult {
    payload.decrypt_metadata()
}

/// Gets the identity details for this V0 payload, or returns an error if this payload does not have
/// any associated identity (public advertisement)
#[no_mangle]
pub extern "C" fn np_ffi_V0Payload_get_identity_details(
    payload: V0Payload,
) -> GetV0IdentityDetailsResult {
    payload.get_identity_details()
}

/// Gets the tag of a `GetV0IdentityDetailsResult` tagged-union. On success the wrapped identity
/// details may be obtained via `GetV0IdentityDetailsResult#into_success`.
#[no_mangle]
pub extern "C" fn np_ffi_GetV0IdentityDetailsResult_kind(
    result: GetV0IdentityDetailsResult,
) -> GetV0IdentityDetailsResultKind {
    result.kind()
}

/// Casts a `GetV0IdentityDetailsResult` to the `Success` variant, panicking in the
/// case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_GetV0IdentityDetailsResult_into_SUCCESS(
    result: GetV0IdentityDetailsResult,
) -> DeserializedV0IdentityDetails {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}

/// Deallocates any internal data of a `V0Payload`
#[no_mangle]
pub extern "C" fn np_ffi_deallocate_v0_payload(payload: V0Payload) -> DeallocateResult {
    payload.deallocate_payload()
}

/// Gets the tag of a `GetV0DEResult` tagged-union.
#[no_mangle]
pub extern "C" fn np_ffi_GetV0DEResult_kind(result: GetV0DEResult) -> GetV0DEResultKind {
    result.kind()
}

/// Casts a `GetV0DEResult` to the `Success` variant, panicking in the
/// case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_GetV0DEResult_into_SUCCESS(result: GetV0DEResult) -> V0DataElement {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}
