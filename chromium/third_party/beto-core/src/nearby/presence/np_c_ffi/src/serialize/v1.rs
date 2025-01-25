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

//! NP Rust C FFI functionality for V1 advertisement serialization.

use crate::{unwrap, PanicReason};
use np_ffi_core::common::{ByteBuffer, FixedSizeArray};
use np_ffi_core::credentials::V1BroadcastCredential;
use np_ffi_core::serialize::v1::*;
use np_ffi_core::serialize::AdvertisementBuilderKind;
use np_ffi_core::utils::FfiEnum;
use np_ffi_core::v1::V1VerificationMode;

/// Attempts to create a builder for a new public section within
/// the advertisement builder behind this handle,
/// returning a handle to the newly-created section builder if successful.
///
/// This method may fail if there is another currently-active
/// section builder for the same advertisement builder, if the
/// kind of section being added does not match the advertisement
/// type (public/encrypted), or if the section would not manage
/// to fit within the enclosing advertisement.
#[no_mangle]
pub extern "C" fn np_ffi_V1AdvertisementBuilder_public_section_builder(
    adv_builder: V1AdvertisementBuilder,
) -> CreateV1SectionBuilderResult {
    adv_builder.public_section_builder()
}

/// Attempts to create a builder for a new encrypted section within
/// the advertisement builder behind the given handle,
/// returning a handle to the newly-created section builder if successful.
///
/// The identity details for the new section builder may be specified
/// via providing the broadcast credential data, the kind of encrypted
/// identity being broadcast (private/trusted/provisioned), and the
/// verification mode (MIC/Signature) to be used for the encrypted section.
///
/// This method may fail if there is another currently-active
/// section builder for the same advertisement builder, if the
/// kind of section being added does not match the advertisement
/// type (public/encrypted), or if the section would not manage
/// to fit within the enclosing advertisement.
#[no_mangle]
pub extern "C" fn np_ffi_V1AdvertisementBuilder_encrypted_section_builder(
    adv_builder: V1AdvertisementBuilder,
    broadcast_cred: V1BroadcastCredential,
    verification_mode: V1VerificationMode,
) -> CreateV1SectionBuilderResult {
    adv_builder.encrypted_section_builder(broadcast_cred, verification_mode)
}

/// Attempts to serialize the contents of the advertisement builder
/// behind this handle to bytes. Assuming that the handle is valid,
/// this operation will always result in the contents behind the
/// advertisement builder handle being deallocated.
#[no_mangle]
pub extern "C" fn np_ffi_V1AdvertisementBuilder_into_advertisement(
    adv_builder: V1AdvertisementBuilder,
) -> SerializeV1AdvertisementResult {
    adv_builder.into_advertisement()
}

/// Creates a new V1 advertisement builder for the given advertisement
/// kind (public/encrypted).
#[no_mangle]
pub extern "C" fn np_ffi_create_v1_advertisement_builder(
    kind: AdvertisementBuilderKind,
) -> V1AdvertisementBuilder {
    unwrap(
        create_v1_advertisement_builder(kind).into_success(),
        PanicReason::ExceededMaxHandleAllocations,
    )
}

/// Gets the tag of a `SerializeV1AdvertisementResult` tagged-union.
#[no_mangle]
pub extern "C" fn np_ffi_SerializeV1AdvertisementResult_kind(
    result: SerializeV1AdvertisementResult,
) -> SerializeV1AdvertisementResultKind {
    result.kind()
}

/// Casts a `SerializeV1AdvertisementResult` to the `Success` variant,
/// panicking in the case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_SerializeV1AdvertisementResult_into_SUCCESS(
    result: SerializeV1AdvertisementResult,
) -> ByteBuffer<250> {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}

/// Gets the tag of a `CreateV1SectionBuilderResult` tagged-union.
#[no_mangle]
pub extern "C" fn np_ffi_CreateV1SectionBuilderResult_kind(
    result: CreateV1SectionBuilderResult,
) -> CreateV1SectionBuilderResultKind {
    result.kind()
}

/// Casts a `CreateV1SectionBuilderResult` to the `Success` variant,
/// panicking in the case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_CreateV1SectionBuilderResult_into_SUCCESS(
    result: CreateV1SectionBuilderResult,
) -> V1SectionBuilder {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}

/// Gets the tag of a `NextV1DE16ByteSaltResult` tagged-union.
#[no_mangle]
pub extern "C" fn np_ffi_NextV1DE16ByteSaltResult_kind(
    result: NextV1DE16ByteSaltResult,
) -> NextV1DE16ByteSaltResultKind {
    result.kind()
}

/// Casts a `NextV1DE16ByteSaltResult` to the `Success` variant,
/// panicking in the case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_NextV1DE16ByteSaltResult_into_SUCCESS(
    result: NextV1DE16ByteSaltResult,
) -> FixedSizeArray<16> {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}

/// Adds the section constructed behind the given handle to
/// a section builder to the containing advertisement it originated from.
/// After this call, the section builder handle will become invalid.
#[no_mangle]
pub extern "C" fn np_ffi_V1SectionBuilder_add_to_advertisement(
    section_builder: V1SectionBuilder,
) -> AddV1SectionToAdvertisementResult {
    section_builder.add_to_advertisement()
}

/// Attempts to get the derived 16-byte V1 DE salt for the next
/// DE to be added to the passed section builder. May fail if this
/// section builder handle is invalid, or if the section
/// is a public section.
#[no_mangle]
pub extern "C" fn np_ffi_V1SectionBuilder_next_de_salt(
    section_builder: V1SectionBuilder,
) -> NextV1DE16ByteSaltResult {
    section_builder.next_de_salt()
}

/// Attempts to add the given DE to the section builder behind
/// this handle. The passed DE may have a payload of up to 127
/// bytes, the maximum for a V1 DE.
#[no_mangle]
pub extern "C" fn np_ffi_V1SectionBuilder_add_127_byte_buffer_de(
    section_builder: V1SectionBuilder,
    de: V1DE127ByteBuffer,
) -> AddV1DEResult {
    section_builder.add_127_byte_buffer_de(de)
}
