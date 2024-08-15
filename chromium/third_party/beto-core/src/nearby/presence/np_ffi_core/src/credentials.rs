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

use crate::common::*;
use crate::utils::{FfiEnum, LocksLongerThan};
use crypto_provider_default::CryptoProviderImpl;
use handle_map::{
    declare_handle_map, HandleLike, HandleMapDimensions, HandleMapFullError,
    HandleMapTryAllocateError,
};
use std::sync::Arc;

/// Cryptographic information about a particular V0 discovery credential
/// necessary to match and decrypt encrypted V0 advertisements.
#[repr(C)]
pub struct V0DiscoveryCredential {
    key_seed: [u8; 32],
    legacy_metadata_key_hmac: [u8; 32],
}

impl V0DiscoveryCredential {
    /// Constructs a new V0 discovery credential with the given 32-byte key-seed
    /// and the given 32-byte HMAC for the (14-byte) legacy metadata key.
    pub fn new(key_seed: [u8; 32], legacy_metadata_key_hmac: [u8; 32]) -> Self {
        Self { key_seed, legacy_metadata_key_hmac }
    }
    fn into_internal(self) -> np_adv::credential::v0::V0DiscoveryCredential {
        np_adv::credential::v0::V0DiscoveryCredential::new(
            self.key_seed,
            self.legacy_metadata_key_hmac,
        )
    }
}

/// Cryptographic information about a particular V1 discovery credential
/// necessary to match and decrypt encrypted V1 advertisement sections.
#[repr(C)]
pub struct V1DiscoveryCredential {
    key_seed: [u8; 32],
    expected_unsigned_metadata_key_hmac: [u8; 32],
    expected_signed_metadata_key_hmac: [u8; 32],
    pub_key: [u8; 32],
}

impl V1DiscoveryCredential {
    /// Constructs a new V1 discovery credential with the given 32-byte key-seed,
    /// unsigned-variant HMAC of the metadata key, the signed-variant HMAC of
    /// the metadata key, and the given public key for signature verification.
    pub fn new(
        key_seed: [u8; 32],
        expected_unsigned_metadata_key_hmac: [u8; 32],
        expected_signed_metadata_key_hmac: [u8; 32],
        pub_key: [u8; 32],
    ) -> Self {
        Self {
            key_seed,
            expected_unsigned_metadata_key_hmac,
            expected_signed_metadata_key_hmac,
            pub_key,
        }
    }
    fn into_internal(self) -> np_adv::credential::v1::V1DiscoveryCredential {
        np_adv::credential::v1::V1DiscoveryCredential::new(
            self.key_seed,
            self.expected_unsigned_metadata_key_hmac,
            self.expected_signed_metadata_key_hmac,
            self.pub_key,
        )
    }
}

/// A [`MatchedCredential`] implementation for the purpose of
/// capturing match-data details across the FFI boundary.
/// Since we can't know what plaintext match-data the client
/// wants to keep around, we just expose an ID for them to do
/// their own look-up.
///
/// For the encrypted metadata, we need a slightly richer
/// representation, since we need to be able to decrypt
/// the metadata as part of an API call. Internally, we
/// keep this as an atomic-reference-counted pointer to
/// a byte array, and never expose this raw pointer across
/// the FFI boundary.
#[derive(Debug, Clone)]
pub struct MatchedCredential {
    cred_id: u32,
    encrypted_metadata_bytes: Arc<[u8]>,
}

impl MatchedCredential {
    /// Constructs a new matched credential from the given match-id
    /// (some arbitrary `u32` identifier) and encrypted metadata bytes,
    /// copied from the given slice.
    pub fn new(cred_id: u32, encrypted_metadata_bytes: &[u8]) -> Self {
        let encrypted_metadata_bytes = encrypted_metadata_bytes.to_vec().into_boxed_slice();
        let encrypted_metadata_bytes = Arc::from(encrypted_metadata_bytes);
        Self { cred_id, encrypted_metadata_bytes }
    }
    /// Gets the pre-specified numerical identifier for this matched-credential.
    pub(crate) fn id(&self) -> u32 {
        self.cred_id
    }
}

impl PartialEq<MatchedCredential> for MatchedCredential {
    fn eq(&self, other: &Self) -> bool {
        self.id() == other.id()
    }
}

impl Eq for MatchedCredential {}

impl np_adv::credential::MatchedCredential for MatchedCredential {
    type EncryptedMetadata = Arc<[u8]>;
    type EncryptedMetadataFetchError = core::convert::Infallible;
    fn fetch_encrypted_metadata(&self) -> Result<Arc<[u8]>, core::convert::Infallible> {
        Ok(self.encrypted_metadata_bytes.clone())
    }
}

/// Internals of a credential slab,
/// an intermediate used in the construction
/// of a credential-book.
pub struct CredentialSlabInternals {
    v0_creds:
        Vec<np_adv::credential::MatchableCredential<np_adv::credential::v0::V0, MatchedCredential>>,
    v1_creds:
        Vec<np_adv::credential::MatchableCredential<np_adv::credential::v1::V1, MatchedCredential>>,
}

impl CredentialSlabInternals {
    pub(crate) fn new() -> Self {
        Self { v0_creds: Vec::new(), v1_creds: Vec::new() }
    }
    /// Adds the given V0 discovery credential with the given
    /// identity match-data onto the end of the V0 credentials
    /// currently stored in this slab.
    pub(crate) fn add_v0(
        &mut self,
        discovery_credential: V0DiscoveryCredential,
        match_data: MatchedCredential,
    ) {
        let discovery_credential = discovery_credential.into_internal();
        let matchable_credential =
            np_adv::credential::MatchableCredential { discovery_credential, match_data };
        self.v0_creds.push(matchable_credential);
    }
    /// Adds the given V1 discovery credential with the given
    /// identity match-data onto the end of the V1 credentials
    /// currently stored in this slab.
    pub(crate) fn add_v1(
        &mut self,
        discovery_credential: V1DiscoveryCredential,
        match_data: MatchedCredential,
    ) {
        let discovery_credential = discovery_credential.into_internal();
        let matchable_credential =
            np_adv::credential::MatchableCredential { discovery_credential, match_data };
        self.v1_creds.push(matchable_credential);
    }
}

/// Discriminant for `CreateCredentialSlabResult`
#[repr(u8)]
pub enum CreateCredentialSlabResultKind {
    /// There was no space left to create a new credential slab
    NoSpaceLeft = 0,
    /// We created a new credential slab behind the given handle.
    /// The associated payload may be obtained via
    /// `CreateCredentialSlabResult#into_success()`.
    Success = 1,
}

/// Result type for `create_credential_slab`
#[repr(C)]
#[allow(missing_docs)]
pub enum CreateCredentialSlabResult {
    NoSpaceLeft,
    Success(CredentialSlab),
}

impl From<Result<CredentialSlab, HandleMapFullError>> for CreateCredentialSlabResult {
    fn from(result: Result<CredentialSlab, HandleMapFullError>) -> Self {
        match result {
            Ok(slab) => CreateCredentialSlabResult::Success(slab),
            Err(_) => CreateCredentialSlabResult::NoSpaceLeft,
        }
    }
}

/// Result type for trying to add a credential to a credential-slab.
#[repr(u8)]
pub enum AddCredentialToSlabResult {
    /// We succeeded in adding the credential to the slab.
    Success = 0,
    /// The handle to the slab was actually invalid.
    InvalidHandle = 1,
}

declare_handle_map! {
    mod credential_slab {
        #[dimensions = super::get_credential_slab_handle_map_dimensions()]
        type CredentialSlab: HandleLike<Object = super::CredentialSlabInternals>;
    }
}
use credential_slab::CredentialSlab;

fn get_credential_slab_handle_map_dimensions() -> HandleMapDimensions {
    HandleMapDimensions {
        num_shards: global_num_shards(),
        max_active_handles: global_max_num_credential_slabs(),
    }
}

impl CredentialSlab {
    /// Adds the given V0 discovery credential with some associated
    /// match-data to this credential slab.
    pub fn add_v0(
        &self,
        discovery_credential: V0DiscoveryCredential,
        match_data: MatchedCredential,
    ) -> AddCredentialToSlabResult {
        match self.get_mut() {
            Ok(mut write_guard) => {
                write_guard.add_v0(discovery_credential, match_data);
                AddCredentialToSlabResult::Success
            }
            Err(_) => AddCredentialToSlabResult::InvalidHandle,
        }
    }
    /// Adds the given V1 discovery credential with some associated
    /// match-data to this credential slab.
    pub fn add_v1(
        &self,
        discovery_credential: V1DiscoveryCredential,
        match_data: MatchedCredential,
    ) -> AddCredentialToSlabResult {
        match self.get_mut() {
            Ok(mut write_guard) => {
                write_guard.add_v1(discovery_credential, match_data);
                AddCredentialToSlabResult::Success
            }
            Err(_) => AddCredentialToSlabResult::InvalidHandle,
        }
    }
}

/// Allocates a new credential-slab, returning a handle to the created object
pub fn create_credential_slab() -> CreateCredentialSlabResult {
    CredentialSlab::allocate(CredentialSlabInternals::new).into()
}

impl FfiEnum for CreateCredentialSlabResult {
    type Kind = CreateCredentialSlabResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            CreateCredentialSlabResult::NoSpaceLeft => CreateCredentialSlabResultKind::NoSpaceLeft,
            CreateCredentialSlabResult::Success(_) => CreateCredentialSlabResultKind::Success,
        }
    }
}

impl CreateCredentialSlabResult {
    declare_enum_cast! {into_success, Success, CredentialSlab }
}

/// Internal, Rust-side implementation of a credential-book.
/// See [`CredentialBook`] for the FFI-side handles.
pub struct CredentialBookInternals {
    pub(crate) book: np_adv::credential::book::PrecalculatedOwnedCredentialBook<MatchedCredential>,
}

impl CredentialBookInternals {
    fn create_from_slab(credential_slab: CredentialSlabInternals) -> Self {
        let book = np_adv::credential::book::CredentialBookBuilder::build_precalculated_owned_book::<
            CryptoProviderImpl,
        >(credential_slab.v0_creds, credential_slab.v1_creds);
        Self { book }
    }
}

fn get_credential_book_handle_map_dimensions() -> HandleMapDimensions {
    HandleMapDimensions {
        num_shards: global_num_shards(),
        max_active_handles: global_max_num_credential_books(),
    }
}

declare_handle_map! {
    mod credential_book {
        #[dimensions = super::get_credential_book_handle_map_dimensions()]
        type CredentialBook: HandleLike<Object = super::CredentialBookInternals>;
    }
}
pub use credential_book::CredentialBook;

/// Discriminant for `CreateCredentialBookResult`
#[repr(u8)]
pub enum CreateCredentialBookResultKind {
    /// We created a new credential book behind the given handle.
    /// The associated payload may be obtained via
    /// `CreateCredentialBookResult#into_success()`.
    Success = 0,
    /// There was no space left to create a new credential book
    NoSpaceLeft = 1,
    /// The slab that we tried to create a credential-book from
    /// actually was an invalid handle.
    InvalidSlabHandle = 2,
}

/// Result type for `create_credential_book`
#[repr(u8)]
#[allow(missing_docs)]
pub enum CreateCredentialBookResult {
    Success(CredentialBook) = 0,
    NoSpaceLeft = 1,
    InvalidSlabHandle = 2,
}

impl LocksLongerThan<CredentialSlab> for CredentialBook {}

/// Allocates a new credential-book, returning a handle to the created object
pub fn create_credential_book_from_slab(
    credential_slab: CredentialSlab,
) -> CreateCredentialBookResult {
    // The credential-book allocation is on the outside, since we should ensure
    // that we have a slot available for construction before we try to deallocate
    // the credential-slab which was passed in.
    let op_result = CredentialBook::try_allocate(|| {
        credential_slab.deallocate().map(CredentialBookInternals::create_from_slab)
    });
    match op_result {
        Ok(book) => CreateCredentialBookResult::Success(book),
        Err(HandleMapTryAllocateError::ValueProviderFailed(_)) => {
            // Unable to deallocate the referenced credential-slab
            CreateCredentialBookResult::InvalidSlabHandle
        }
        Err(HandleMapTryAllocateError::HandleMapFull) => {
            // Unable to allocate space for a new credential-book
            CreateCredentialBookResult::NoSpaceLeft
        }
    }
}

impl FfiEnum for CreateCredentialBookResult {
    type Kind = CreateCredentialBookResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            CreateCredentialBookResult::Success(_) => CreateCredentialBookResultKind::Success,
            CreateCredentialBookResult::NoSpaceLeft => CreateCredentialBookResultKind::NoSpaceLeft,
            CreateCredentialBookResult::InvalidSlabHandle => {
                CreateCredentialBookResultKind::InvalidSlabHandle
            }
        }
    }
}

impl CreateCredentialBookResult {
    declare_enum_cast! {into_success, Success, CredentialBook}
}

/// Deallocates a credential-book by its handle
pub fn deallocate_credential_book(credential_book: CredentialBook) -> DeallocateResult {
    credential_book.deallocate().map(|_| ()).into()
}

/// Deallocates a credential-slab by its handle
pub fn deallocate_credential_slab(credential_slab: CredentialSlab) -> DeallocateResult {
    credential_slab.deallocate().map(|_| ()).into()
}
