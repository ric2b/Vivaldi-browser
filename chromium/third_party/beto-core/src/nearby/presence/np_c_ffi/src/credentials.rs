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

use crate::{panic, unwrap, PanicReason};
use core::slice;
use np_ffi_core::common::*;
use np_ffi_core::credentials::{
    create_credential_book_from_slab, create_credential_slab, deallocate_credential_book,
    deallocate_credential_slab, AddV0CredentialToSlabResult, AddV1CredentialToSlabResult,
    CredentialBook, CredentialSlab, MatchedCredential, V0DiscoveryCredential,
    V1DiscoveryCredential,
};
use np_ffi_core::declare_enum_cast;
use np_ffi_core::deserialize::DecryptedMetadata;
use np_ffi_core::deserialize::{
    DecryptMetadataResult, DecryptMetadataResultKind, GetMetadataBufferPartsResult,
    GetMetadataBufferPartsResultKind, MetadataBufferParts,
};
use np_ffi_core::utils::FfiEnum;

/// Allocates a new credential-book from the given slab, returning a handle
/// to the created object. The slab will be deallocated by this call.
#[no_mangle]
pub extern "C" fn np_ffi_create_credential_book_from_slab(
    slab: CredentialSlab,
) -> CreateCredentialBookResult {
    create_credential_book_from_slab(slab).into()
}

/// Result type for `create_credential_book`
#[repr(u8)]
#[allow(missing_docs)]
pub enum CreateCredentialBookResult {
    Success(CredentialBook) = 0,
    InvalidSlabHandle = 2,
}

//TODO: unwrap allocation errors at the ffi-core layer and remove this, after design has been
// agreed upon
impl From<np_ffi_core::credentials::CreateCredentialBookResult> for CreateCredentialBookResult {
    fn from(value: np_ffi_core::credentials::CreateCredentialBookResult) -> Self {
        match value {
            np_ffi_core::credentials::CreateCredentialBookResult::Success(v) => {
                CreateCredentialBookResult::Success(v)
            }
            np_ffi_core::credentials::CreateCredentialBookResult::InvalidSlabHandle => {
                CreateCredentialBookResult::InvalidSlabHandle
            }
            np_ffi_core::credentials::CreateCredentialBookResult::NoSpaceLeft => {
                panic(PanicReason::ExceededMaxHandleAllocations)
            }
        }
    }
}

/// Discriminant for `CreateCredentialBookResult`
#[repr(u8)]
pub enum CreateCredentialBookResultKind {
    /// We created a new credential book behind the given handle.
    /// The associated payload may be obtained via
    /// `CreateCredentialBookResult#into_success()`.
    Success = 0,
    /// The slab that we tried to create a credential-book from
    /// actually was an invalid handle.
    InvalidSlabHandle = 1,
}

impl np_ffi_core::utils::FfiEnum for CreateCredentialBookResult {
    type Kind = CreateCredentialBookResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            CreateCredentialBookResult::Success(_) => CreateCredentialBookResultKind::Success,
            CreateCredentialBookResult::InvalidSlabHandle => {
                CreateCredentialBookResultKind::InvalidSlabHandle
            }
        }
    }
}

impl CreateCredentialBookResult {
    declare_enum_cast! {into_success, Success, CredentialBook}
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
pub extern "C" fn np_ffi_create_credential_slab() -> CredentialSlab {
    unwrap(create_credential_slab().into_success(), PanicReason::ExceededMaxHandleAllocations)
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
    cred_id: i64,
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
) -> AddV0CredentialToSlabResult {
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
) -> AddV1CredentialToSlabResult {
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

/// Frees the underlying resources of the decrypted metadata buffer
#[no_mangle]
pub extern "C" fn np_ffi_deallocate_DecryptedMetadata(
    metadata: DecryptedMetadata,
) -> DeallocateResult {
    metadata.deallocate_metadata()
}

/// Gets the tag of a `DecryptMetadataResult` tagged-union. On success the wrapped identity
/// details may be obtained via `DecryptMetadataResult#into_success`.
#[no_mangle]
pub extern "C" fn np_ffi_DecryptMetadataResult_kind(
    result: DecryptMetadataResult,
) -> DecryptMetadataResultKind {
    result.kind()
}

/// Casts a `DecryptMetadataResult` to the `Success` variant, panicking in the
/// case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_DecryptMetadataResult_into_SUCCESS(
    result: DecryptMetadataResult,
) -> DecryptedMetadata {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}

/// Gets the pointer and length of the heap allocated byte buffer of decrypted metadata
#[no_mangle]
pub extern "C" fn np_ffi_DecryptedMetadata_get_metadata_buffer_parts(
    metadata: DecryptedMetadata,
) -> GetMetadataBufferPartsResult {
    metadata.get_metadata_buffer_parts()
}

/// Gets the tag of a `GetMetadataBufferPartsResult` tagged-union. On success the wrapped identity
/// details may be obtained via `GetMetadataBufferPartsResult#into_success`.
#[no_mangle]
pub extern "C" fn np_ffi_GetMetadataBufferPartsResult_kind(
    result: GetMetadataBufferPartsResult,
) -> GetMetadataBufferPartsResultKind {
    result.kind()
}

/// Casts a `GetMetadataBufferPartsResult` to the `Success` variant, panicking in the
/// case where the passed value is of a different enum variant. This returns the pointer and length
/// of the byte buffer containing the decrypted metadata.  There can be a data-race between attempts
/// to access the contents of the buffer and attempts to free the handle from different threads.
#[no_mangle]
pub extern "C" fn np_ffi_GetMetadataBufferPartsResult_into_SUCCESS(
    result: GetMetadataBufferPartsResult,
) -> MetadataBufferParts {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}
