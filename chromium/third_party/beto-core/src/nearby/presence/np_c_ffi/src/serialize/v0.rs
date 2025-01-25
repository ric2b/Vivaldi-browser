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

//! NP Rust C FFI functionality for V0 advertisement serialization.

use crate::{panic_if_invalid, unwrap, PanicReason};
use np_ffi_core::common::{ByteBuffer, DeallocateResult, FixedSizeArray};
use np_ffi_core::credentials::V0BroadcastCredential;
use np_ffi_core::serialize::v0::*;
use np_ffi_core::utils::FfiEnum;
use np_ffi_core::v0::V0DataElement;

/// Attempts to add the given data element to the V0
/// advertisement builder behind the passed handle.
///
/// This method may invoke the panic handler if the passed DE
/// has an invalid layout, which may indicate that the backing
/// data on the stack was somehow tampered with in an unintended way.
#[no_mangle]
pub extern "C" fn np_ffi_V0AdvertisementBuilder_add_de(
    adv_builder: V0AdvertisementBuilder,
    de: V0DataElement,
) -> AddV0DEResult {
    panic_if_invalid(adv_builder.add_de(de))
}

/// Attempts to serialize the contents of the advertisement builder
/// behind this handle to bytes. Assuming that the handle is valid,
/// this operation will always result in the contents behind the
/// advertisement builder handle being deallocated.
#[no_mangle]
pub extern "C" fn np_ffi_V0AdvertisementBuilder_into_advertisement(
    adv_builder: V0AdvertisementBuilder,
) -> SerializeV0AdvertisementResult {
    adv_builder.into_advertisement()
}

/// Attempts to deallocate the v0 advertisement builder behind
/// the given handle.
#[no_mangle]
pub extern "C" fn np_ffi_deallocate_v0_advertisement_builder(
    adv_builder: V0AdvertisementBuilder,
) -> DeallocateResult {
    adv_builder.deallocate_handle()
}

/// Creates a new V0 advertisement builder for a public advertisement.
#[no_mangle]
pub extern "C" fn np_ffi_create_v0_public_advertisement_builder() -> V0AdvertisementBuilder {
    unwrap(
        create_v0_public_advertisement_builder().into_success(),
        PanicReason::ExceededMaxHandleAllocations,
    )
}

/// Creates a new V0 advertisement builder for an encrypted advertisement.
#[no_mangle]
pub extern "C" fn np_ffi_create_v0_encrypted_advertisement_builder(
    broadcast_cred: V0BroadcastCredential,
    salt: FixedSizeArray<2>,
) -> V0AdvertisementBuilder {
    unwrap(
        create_v0_encrypted_advertisement_builder(broadcast_cred, salt).into_success(),
        PanicReason::ExceededMaxHandleAllocations,
    )
}

/// Gets the tag of a `SerializeV0AdvertisementResult` tagged-union.
#[no_mangle]
pub extern "C" fn np_ffi_SerializeV0AdvertisementResult_kind(
    result: SerializeV0AdvertisementResult,
) -> SerializeV0AdvertisementResultKind {
    result.kind()
}

/// Casts a `SerializeV0AdvertisementResult` to the `Success` variant,
/// panicking in the case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_SerializeV0AdvertisementResult_into_SUCCESS(
    result: SerializeV0AdvertisementResult,
) -> ByteBuffer<24> {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}
