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
//! Core NP Rust FFI structures and methods for advertisement deserialization.

use crate::common::*;
use crate::credentials::CredentialBook;
use crate::deserialize::v0::*;
use crate::deserialize::v1::*;
use crate::utils::FfiEnum;
use crypto_provider_default::CryptoProviderImpl;
use handle_map::{declare_handle_map, HandleLike, HandleMapFullError, HandleNotPresentError};
use np_adv::deserialization_arena;

pub mod v0;
pub mod v1;

/// Discriminant for `DeserializeAdvertisementResult`.
#[repr(u8)]
pub enum DeserializeAdvertisementResultKind {
    /// Deserializing the advertisement failed, for some reason or another.
    Error = 0,
    /// The advertisement was correctly deserialized, and it's a V0 advertisement.
    /// `DeserializeAdvertisementResult#into_v0()` is the corresponding cast
    /// to the associated enum variant.
    V0 = 1,
    /// The advertisement was correctly deserialized, and it's a V1 advertisement.
    /// `DeserializeAdvertisementResult#into_v1()` is the corresponding cast
    /// to the associated enum variant.
    V1 = 2,
}

/// The result of calling `np_ffi_deserialize_advertisement`.
/// Must be explicitly deallocated after use with
/// a corresponding `np_ffi_deallocate_deserialize_advertisement_result`
#[repr(u8)]
pub enum DeserializeAdvertisementResult {
    /// Deserializing the advertisement failed, for some reason or another.
    /// `DeserializeAdvertisementResultKind::Error` is the associated enum tag.
    Error,
    /// The advertisement was correctly deserialized, and it's a V0 advertisement.
    /// `DeserializeAdvertisementResultKind::V0` is the associated enum tag.
    V0(DeserializedV0Advertisement),
    /// The advertisement was correctly deserialized, and it's a V1 advertisement.
    /// `DeserializeAdvertisementResultKind::V1` is the associated enum tag.
    V1(DeserializedV1Advertisement),
}

impl FfiEnum for DeserializeAdvertisementResult {
    type Kind = DeserializeAdvertisementResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            DeserializeAdvertisementResult::Error => DeserializeAdvertisementResultKind::Error,
            DeserializeAdvertisementResult::V0(_) => DeserializeAdvertisementResultKind::V0,
            DeserializeAdvertisementResult::V1(_) => DeserializeAdvertisementResultKind::V1,
        }
    }
}

impl DeserializeAdvertisementResult {
    declare_enum_cast! {into_v0, V0, DeserializedV0Advertisement}
    declare_enum_cast! {into_v1, V1, DeserializedV1Advertisement}

    /// Deallocates any internal data referenced by a `DeserializeAdvertisementResult`. This takes
    /// ownership of any internal handles.
    pub fn deallocate(self) -> DeallocateResult {
        match self {
            DeserializeAdvertisementResult::Error => DeallocateResult::Success,
            DeserializeAdvertisementResult::V0(adv) => adv.deallocate(),
            DeserializeAdvertisementResult::V1(adv) => adv.deallocate(),
        }
    }
}

//TODO: Once the `FromResidual` trait is stabilized, we won't need to do this
enum DeserializeAdvertisementSuccess {
    V0(DeserializedV0Advertisement),
    V1(DeserializedV1Advertisement),
}

pub(crate) struct DeserializeAdvertisementError;

impl From<HandleMapFullError> for DeserializeAdvertisementError {
    fn from(_: HandleMapFullError) -> Self {
        DeserializeAdvertisementError
    }
}

impl From<HandleNotPresentError> for DeserializeAdvertisementError {
    fn from(_: HandleNotPresentError) -> Self {
        DeserializeAdvertisementError
    }
}

impl From<np_adv::AdvDeserializationError> for DeserializeAdvertisementError {
    fn from(_: np_adv::AdvDeserializationError) -> Self {
        DeserializeAdvertisementError
    }
}

fn deserialize_advertisement_from_slice_internal(
    adv_payload: &[u8],
    credential_book: CredentialBook,
) -> Result<DeserializeAdvertisementSuccess, DeserializeAdvertisementError> {
    // Deadlock Safety: Credential-book locks always live longer than deserialized advs.
    let credential_book_read_guard = credential_book.get()?;

    let cred_book = &credential_book_read_guard.book;

    let arena = deserialization_arena!();
    let deserialized_advertisement =
        np_adv::deserialize_advertisement::<_, CryptoProviderImpl>(arena, adv_payload, cred_book)?;
    match deserialized_advertisement {
        np_adv::DeserializedAdvertisement::V0(adv_contents) => {
            let adv_handle = DeserializedV0Advertisement::allocate_with_contents(adv_contents)?;
            Ok(DeserializeAdvertisementSuccess::V0(adv_handle))
        }
        np_adv::DeserializedAdvertisement::V1(adv_contents) => {
            let adv_handle = DeserializedV1Advertisement::allocate_with_contents(adv_contents)?;
            Ok(DeserializeAdvertisementSuccess::V1(adv_handle))
        }
    }
}

/// Attempts to deserialize an advertisement with the given payload. Suitable for langs which have
/// a suitably expressive slice-type. This uses the given `credential_book` handle but does not
/// take ownership of it. The caller is given ownership of any handles in the returned structure.
pub fn deserialize_advertisement_from_slice(
    adv_payload: &[u8],
    credential_book: CredentialBook,
) -> DeserializeAdvertisementResult {
    let result = deserialize_advertisement_from_slice_internal(adv_payload, credential_book);
    match result {
        Ok(DeserializeAdvertisementSuccess::V0(x)) => DeserializeAdvertisementResult::V0(x),
        Ok(DeserializeAdvertisementSuccess::V1(x)) => DeserializeAdvertisementResult::V1(x),
        Err(_) => DeserializeAdvertisementResult::Error,
    }
}

/// Attempts to deserialize an advertisement with the given payload.  Suitable for langs which
/// don't have an expressive-enough slice type. This uses the given `credential_book` handle but
/// does not take ownership of it. The caller is given ownership of any handles in the returned
/// structure.
pub fn deserialize_advertisement(
    adv_payload: &RawAdvertisementPayload,
    credential_book: CredentialBook,
) -> DeserializeAdvertisementResult {
    deserialize_advertisement_from_slice(adv_payload.as_slice(), credential_book)
}

/// Errors returned from attempting to decrypt metadata
pub(crate) enum DecryptMetadataError {
    /// The advertisement payload handle was either deallocated
    /// or corresponds to a public advertisement, and so we
    /// don't have any metadata to decrypt.
    EncryptedMetadataNotAvailable,
    /// Decryption of the raw metadata bytes failed.
    DecryptionFailed,
}

/// The result of decrypting metadata from either a V0Payload or DeserializedV1Section
#[repr(C)]
#[allow(missing_docs)]
pub enum DecryptMetadataResult {
    Success(DecryptedMetadata),
    Error,
}

/// Discriminant for `DecryptMetadataResult`.
#[repr(u8)]
pub enum DecryptMetadataResultKind {
    /// The attempt to decrypt the metadata of the associated credential succeeded
    /// The associated payload may be obtained via
    /// `DecryptMetadataResult#into_success`.
    Success,
    /// The attempt to decrypt the metadata failed, either the payload had no matching identity
    /// ie it was a public advertisement OR the decrypt attempt itself was unsuccessful
    Error,
}

impl FfiEnum for DecryptMetadataResult {
    type Kind = DecryptMetadataResultKind;

    fn kind(&self) -> Self::Kind {
        match self {
            DecryptMetadataResult::Success(_) => DecryptMetadataResultKind::Success,
            DecryptMetadataResult::Error => DecryptMetadataResultKind::Error,
        }
    }
}

impl DecryptMetadataResult {
    declare_enum_cast! {into_success, Success, DecryptedMetadata}
}

/// Internals of decrypted metadata
pub struct DecryptedMetadataInternals {
    decrypted_bytes: Box<[u8]>,
}

/// A `#[repr(C)]` handle to a value of type `DecryptedMetadataInternals`
#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct DecryptedMetadata {
    handle_id: u64,
}

declare_handle_map!(
    decrypted_metadata,
    crate::common::default_handle_map_dimensions(),
    super::DecryptedMetadata,
    super::DecryptedMetadataInternals
);

/// The pointer and length of the decrypted metadata byte buffer
#[repr(C)]
pub struct MetadataBufferParts {
    ptr: *const u8,
    len: usize,
}

#[repr(C)]
#[allow(missing_docs)]
pub enum GetMetadataBufferPartsResult {
    Success(MetadataBufferParts),
    Error,
}

impl GetMetadataBufferPartsResult {
    declare_enum_cast! {into_success, Success, MetadataBufferParts}
}

#[repr(u8)]
#[allow(missing_docs)]
pub enum GetMetadataBufferPartsResultKind {
    Success = 0,
    Error = 1,
}

impl FfiEnum for GetMetadataBufferPartsResult {
    type Kind = GetMetadataBufferPartsResultKind;

    fn kind(&self) -> Self::Kind {
        match self {
            GetMetadataBufferPartsResult::Success(_) => GetMetadataBufferPartsResultKind::Success,
            GetMetadataBufferPartsResult::Error => GetMetadataBufferPartsResultKind::Error,
        }
    }
}

fn allocate_decrypted_metadata_handle(metadata: Vec<u8>) -> DecryptMetadataResult {
    let allocate_result = DecryptedMetadata::allocate(move || DecryptedMetadataInternals {
        decrypted_bytes: metadata.into_boxed_slice(),
    });
    match allocate_result {
        Ok(decrypted) => DecryptMetadataResult::Success(decrypted),
        Err(_) => DecryptMetadataResult::Error,
    }
}

impl DecryptedMetadata {
    /// Gets the raw parts, pointer + length representation of the metadata byte buffer. This uses
    /// the handle but does not take ownership of it.
    pub fn get_metadata_buffer_parts(&self) -> GetMetadataBufferPartsResult {
        match self.get() {
            Ok(metadata_internals) => {
                let result = MetadataBufferParts {
                    ptr: metadata_internals.decrypted_bytes.as_ptr(),
                    len: metadata_internals.decrypted_bytes.len(),
                };
                GetMetadataBufferPartsResult::Success(result)
            }
            Err(_) => GetMetadataBufferPartsResult::Error,
        }
    }

    /// Unwraps the buffer of decrypted bytes. This is for Rust usage. This takes ownership of the
    /// handle and deallocates it.
    pub fn take_buffer(self) -> Result<Box<[u8]>, HandleNotPresentError> {
        self.deallocate().map(|internals| internals.decrypted_bytes)
    }

    /// Frees the underlying decrypted metadata buffer. This takes ownership of the handle.
    pub fn deallocate_metadata(self) -> DeallocateResult {
        self.deallocate().map(|_| ()).into()
    }
}
