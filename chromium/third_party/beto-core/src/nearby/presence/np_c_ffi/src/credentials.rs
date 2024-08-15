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
//! Credential-related data-types and functions

use crate::{unwrap, PanicReason};
use core::slice;
use np_ffi_core::common::*;
use np_ffi_core::credentials::credential_book::CredentialBook;
use np_ffi_core::credentials::credential_slab::CredentialSlab;
use np_ffi_core::credentials::*;
use np_ffi_core::utils::FfiEnum;

/// Allocates a new credential-book from the given slab, returning a handle
/// to the created object. The slab will be deallocated by this call.
#[no_mangle]
pub extern "C" fn np_ffi_create_credential_book_from_slab(
    slab: CredentialSlab,
) -> CreateCredentialBookResult {
    create_credential_book_from_slab(slab)
}

/// Gets the tag of a `CreateCredentialBookResult` tagged enum.
#[no_mangle]
pub extern "C" fn np_ffi_CreateCredentialBookResult_kind(
    result: CreateCredentialBookResult,
) -> CreateCredentialBookResultKind {
    result.kind()
}

/// Casts a `CreateCredentialBookResult` to the `SUCCESS` variant, panicking in the
/// case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_CreateCredentialBookResult_into_SUCCESS(
    result: CreateCredentialBookResult,
) -> CredentialBook {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}

/// Deallocates a credential-slab by its handle.
#[no_mangle]
pub extern "C" fn np_ffi_deallocate_credential_slab(
    credential_slab: CredentialSlab,
) -> DeallocateResult {
    deallocate_credential_slab(credential_slab)
}

/// Deallocates a credential-book by its handle
#[no_mangle]
pub extern "C" fn np_ffi_deallocate_credential_book(
    credential_book: CredentialBook,
) -> DeallocateResult {
    deallocate_credential_book(credential_book)
}

/// Allocates a new credential-slab, returning a handle to the created object
#[no_mangle]
pub extern "C" fn np_ffi_create_credential_slab() -> CreateCredentialSlabResult {
    create_credential_slab()
}

/// Gets the tag of a `CreateCredentialSlabResult` tagged enum.
#[no_mangle]
pub extern "C" fn np_ffi_CreateCredentialSlabResult_kind(
    result: CreateCredentialSlabResult,
) -> CreateCredentialSlabResultKind {
    result.kind()
}

/// Casts a `CreateCredentialSlabResult` to the `SUCCESS` variant, panicking in the
/// case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_CreateCredentialSlabResult_into_SUCCESS(
    result: CreateCredentialSlabResult,
) -> CredentialSlab {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}

/// Representation of a V0 credential that contains additional data to provide back to caller once it
/// is matched. The credential_id can be used by the caller to correlate it back to the full
/// credentials details.
#[repr(C)]
pub struct V0MatchableCredential {
    discovery_cred: V0DiscoveryCredential,
    matched_cred: FfiMatchedCredential,
}

/// Representation of a V1 credential that contains additional data to provide back to caller once it
/// is matched. The credential_id can be used by the caller to correlate it back to the full
/// credentials details.
#[repr(C)]
pub struct V1MatchableCredential {
    discovery_cred: V1DiscoveryCredential,
    matched_cred: FfiMatchedCredential,
}

/// A representation of a MatchedCredential which is passable across the FFI boundary
#[repr(C)]
pub struct FfiMatchedCredential {
    cred_id: u32,
    encrypted_metadata_bytes_buffer: *const u8,
    encrypted_metadata_bytes_len: usize,
}

/// Adds the given V0 discovery credential with some associated
/// match-data to this credential slab.
///
/// Safety: this is safe if the provided pointer points to a valid memory address
/// which contains the correct len amount of bytes. The copy from the memory address isn't atomic,
/// so concurrent modification of the array from another thread would cause undefined behavior.
#[no_mangle]
pub extern "C" fn np_ffi_CredentialSlab_add_v0_credential(
    credential_slab: CredentialSlab,
    v0_cred: V0MatchableCredential,
) -> AddCredentialToSlabResult {
    #[allow(unsafe_code)]
    let metadata_slice = unsafe {
        slice::from_raw_parts(
            v0_cred.matched_cred.encrypted_metadata_bytes_buffer,
            v0_cred.matched_cred.encrypted_metadata_bytes_len,
        )
    };

    let matched_credential = MatchedCredential::new(v0_cred.matched_cred.cred_id, metadata_slice);
    credential_slab.add_v0(v0_cred.discovery_cred, matched_credential)
}

/// Adds the given V1 discovery credential with some associated
/// match-data to this credential slab.
///
/// Safety: this is safe if the provided pointer points to a valid memory address
/// which contains the correct len amount of bytes. The copy from the memory address isn't atomic,
/// so concurrent modification of the array from another thread would cause undefined behavior.
#[no_mangle]
pub extern "C" fn np_ffi_CredentialSlab_add_v1_credential(
    credential_slab: CredentialSlab,
    v1_cred: V1MatchableCredential,
) -> AddCredentialToSlabResult {
    #[allow(unsafe_code)]
    let metadata_slice = unsafe {
        slice::from_raw_parts(
            v1_cred.matched_cred.encrypted_metadata_bytes_buffer,
            v1_cred.matched_cred.encrypted_metadata_bytes_len,
        )
    };

    let matched_credential = MatchedCredential::new(v1_cred.matched_cred.cred_id, metadata_slice);
    credential_slab.add_v1(v1_cred.discovery_cred, matched_credential)
}
