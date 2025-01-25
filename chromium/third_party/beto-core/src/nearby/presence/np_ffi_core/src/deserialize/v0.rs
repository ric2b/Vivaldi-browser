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
//! Core NP Rust FFI structures and methods for v0 advertisement deserialization.

use crate::common::*;
use crate::credentials::CredentialBook;
use crate::credentials::MatchedCredential;
use crate::deserialize::{
    allocate_decrypted_metadata_handle, DecryptMetadataError, DecryptMetadataResult,
};
use crate::utils::{FfiEnum, LocksLongerThan};
use crate::v0::V0DataElement;
use crypto_provider_default::CryptoProviderImpl;
use handle_map::{declare_handle_map, HandleLike, HandleMapFullError};
use np_adv::credential::matched::HasIdentityMatch;
use np_adv::legacy;

/// Discriminant for possible results of V0 advertisement deserialization
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum DeserializedV0AdvertisementKind {
    /// The deserialized V0 advertisement was legible.
    /// The associated payload may be obtained via
    /// `DeserializedV0Advertisement#into_legible`.
    Legible = 1,
    /// The deserialized V0 advertisement is illegible,
    /// likely meaning that the receiver does not hold
    /// the proper credentials to be able to read
    /// the received advertisement.
    NoMatchingCredentials = 2,
}

/// Represents a deserialized V0 advertisement
#[repr(C)]
#[allow(missing_docs)]
pub enum DeserializedV0Advertisement {
    Legible(LegibleDeserializedV0Advertisement),
    NoMatchingCredentials,
}

impl FfiEnum for DeserializedV0Advertisement {
    type Kind = DeserializedV0AdvertisementKind;
    fn kind(&self) -> Self::Kind {
        match self {
            DeserializedV0Advertisement::Legible(_) => DeserializedV0AdvertisementKind::Legible,
            DeserializedV0Advertisement::NoMatchingCredentials => {
                DeserializedV0AdvertisementKind::NoMatchingCredentials
            }
        }
    }
}

impl DeserializedV0Advertisement {
    /// Attempts to deallocate memory utilized internally by this V0 advertisement (which contains
    /// a handle to actual advertisement contents behind-the-scenes). This function takes ownership
    /// of that internal handle.
    pub fn deallocate(self) -> DeallocateResult {
        match self {
            DeserializedV0Advertisement::Legible(adv) => adv.deallocate(),
            DeserializedV0Advertisement::NoMatchingCredentials => DeallocateResult::Success,
        }
    }

    pub(crate) fn allocate_with_contents(
        contents: legacy::V0AdvertisementContents<
            np_adv::credential::matched::ReferencedMatchedCredential<MatchedCredential>,
        >,
    ) -> Result<Self, DeserializeAdvertisementError> {
        match contents {
            legacy::V0AdvertisementContents::Plaintext(plaintext_contents) => {
                let adv = LegibleDeserializedV0Advertisement::allocate_with_plaintext_contents(
                    plaintext_contents,
                )?;
                Ok(Self::Legible(adv))
            }
            legacy::V0AdvertisementContents::Decrypted(decrypted_contents) => {
                let decrypted_contents = decrypted_contents.clone_match_data();
                let adv = LegibleDeserializedV0Advertisement::allocate_with_decrypted_contents(
                    decrypted_contents,
                )?;
                Ok(Self::Legible(adv))
            }
            legacy::V0AdvertisementContents::NoMatchingCredentials => {
                Ok(Self::NoMatchingCredentials)
            }
        }
    }

    declare_enum_cast! {into_legible, Legible, LegibleDeserializedV0Advertisement}
}

/// Represents a deserialized V0 advertisement whose DE contents may be read
#[repr(C)]
pub struct LegibleDeserializedV0Advertisement {
    num_des: u8,
    payload: V0Payload,
    identity_kind: DeserializedV0IdentityKind,
}

impl LegibleDeserializedV0Advertisement {
    pub(crate) fn allocate_with_plaintext_contents(
        contents: legacy::deserialize::UnencryptedAdvContents,
    ) -> Result<Self, DeserializeAdvertisementError> {
        let data_elements = contents
            .data_elements()
            .collect::<Result<Vec<_>, _>>()
            .map_err(|_| DeserializeAdvertisementError)?;
        let num_des = data_elements.len() as u8;
        let payload = V0Payload::allocate_with_plaintext_data_elements(data_elements)?;
        Ok(Self { num_des, payload, identity_kind: DeserializedV0IdentityKind::Plaintext })
    }
    pub(crate) fn allocate_with_decrypted_contents(
        contents: np_adv::credential::matched::WithMatchedCredential<
            MatchedCredential,
            legacy::deserialize::DecryptedAdvContents,
        >,
    ) -> Result<Self, DeserializeAdvertisementError> {
        let data_elements = contents
            .contents()
            .data_elements()
            .collect::<Result<Vec<_>, _>>()
            .map_err(|_| DeserializeAdvertisementError)?;
        let num_des = data_elements.len() as u8;

        let salt = contents.contents().salt();

        // Reduce the information contained in the contents to just
        // the metadata key, since we're done copying over the DEs
        // and other data into an FFI-friendly form.
        let match_data = contents.map(|x| x.identity_token());

        let payload = V0Payload::allocate_with_decrypted_contents(salt, match_data, data_elements)?;

        Ok(Self { num_des, payload, identity_kind: DeserializedV0IdentityKind::Decrypted })
    }
    /// Gets the number of data-elements in this adv's payload. This number is suitable as an
    /// iteration bound for `Self.into_payload().get_de(...)`.
    pub fn num_des(&self) -> u8 {
        self.num_des
    }
    /// Get a copy of the payload handle. This does not copy the underlying payload. The copied
    /// handle shares a lifetime with the payload handle in this struct; it's the same handle.
    pub fn payload(&self) -> V0Payload {
        self.payload
    }
    /// Destructures this legible advertisement into just the discriminant
    /// for the kind of identity (plaintext/encrypted) used for its contents.
    pub fn identity_kind(&self) -> DeserializedV0IdentityKind {
        self.identity_kind
    }
    /// Deallocates the underlying handle of the payload. This function takes ownership of the
    /// payload handle.
    pub fn deallocate(self) -> DeallocateResult {
        self.payload.deallocate().map(|_| ()).into()
    }
}

/// Discriminant for deserialized information about the V0
/// identity utilized by a deserialized V0 advertisement.
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum DeserializedV0IdentityKind {
    /// The deserialized identity was a plaintext identity.
    Plaintext = 1,
    /// The deserialized identity was some decrypted identity.
    Decrypted = 2,
}

/// Information about the identity which matched a
/// decrypted V0 advertisement.
#[derive(Clone, Copy)]
#[repr(C)]
pub struct DeserializedV0IdentityDetails {
    /// The ID of the credential which
    /// matched the deserialized adv
    cred_id: i64,
    /// The 14-byte legacy identity token
    identity_token: [u8; 14],
    /// The 2-byte advertisement salt
    salt: [u8; 2],
}

impl DeserializedV0IdentityDetails {
    pub(crate) fn new(
        cred_id: i64,
        salt: ldt_np_adv::V0Salt,
        identity_token: ldt_np_adv::V0IdentityToken,
    ) -> Self {
        let salt = salt.bytes();
        Self { cred_id, salt, identity_token: identity_token.bytes() }
    }
    /// Returns the ID of the credential which matched the deserialized adv
    pub fn cred_id(&self) -> i64 {
        self.cred_id
    }
    /// Returns the 14-byte legacy metadata key
    pub fn identity_token(&self) -> [u8; 14] {
        self.identity_token
    }
    /// Returns the 2-byte advertisement salt
    pub fn salt(&self) -> [u8; 2] {
        self.salt
    }
}

/// Discriminant for `GetV0IdentityDetailsResult`
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum GetV0IdentityDetailsResultKind {
    /// The attempt to get the identity details
    /// for the advertisement failed, possibly
    /// due to the advertisement being a public
    /// advertisement, or the underlying
    /// advertisement has already been deallocated.
    Error = 0,
    /// The attempt to get the identity details succeeded.
    /// The wrapped identity details may be obtained via
    /// `GetV0IdentityDetailsResult#into_success`.
    Success = 1,
}

/// The result of attempting to get the identity details
/// for a V0 advertisement via
/// `DeserializedV0Advertisement#get_identity_details`.
#[repr(C)]
#[allow(missing_docs)]
pub enum GetV0IdentityDetailsResult {
    Error,
    Success(DeserializedV0IdentityDetails),
}

impl FfiEnum for GetV0IdentityDetailsResult {
    type Kind = GetV0IdentityDetailsResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            GetV0IdentityDetailsResult::Error => GetV0IdentityDetailsResultKind::Error,
            GetV0IdentityDetailsResult::Success(_) => GetV0IdentityDetailsResultKind::Success,
        }
    }
}

impl GetV0IdentityDetailsResult {
    declare_enum_cast! {into_success, Success, DeserializedV0IdentityDetails}
}

/// Internal implementation of a deserialized V0 identity.
pub(crate) struct DeserializedV0IdentityInternals {
    /// The details about the identity, suitable
    /// for direct communication over FFI
    details: DeserializedV0IdentityDetails,
    /// The metadata key, together with the matched
    /// credential and enough information to decrypt
    /// the credential metadata, if desired.
    match_data: np_adv::credential::matched::WithMatchedCredential<
        MatchedCredential,
        ldt_np_adv::V0IdentityToken,
    >,
}

impl DeserializedV0IdentityInternals {
    pub(crate) fn new(
        salt: ldt_np_adv::V0Salt,
        match_data: np_adv::credential::matched::WithMatchedCredential<
            MatchedCredential,
            ldt_np_adv::V0IdentityToken,
        >,
    ) -> Self {
        let cred_id = match_data.matched_credential().id();
        let identity_token = match_data.contents();
        let details = DeserializedV0IdentityDetails::new(cred_id, salt, *identity_token);
        Self { details, match_data }
    }
    /// Gets the directly-transmissible details about
    /// this deserialized V0 identity. Does not include
    /// decrypted metadata bytes.
    pub(crate) fn details(&self) -> DeserializedV0IdentityDetails {
        self.details
    }
    /// Attempts to decrypt the metadata associated
    /// with this identity.
    pub(crate) fn decrypt_metadata(&self) -> Option<Vec<u8>> {
        self.match_data.decrypt_metadata::<CryptoProviderImpl>().ok()
    }
}

/// The internal data-structure used for storing
/// the payload of a deserialized V0 advertisement.
pub struct V0PayloadInternals {
    identity: Option<DeserializedV0IdentityInternals>,
    des: Vec<V0DataElement>,
}

impl V0PayloadInternals {
    /// Attempts to get the DE with the given index
    /// in this v0 payload.
    fn get_de(&self, index: u8) -> GetV0DEResult {
        match self.des.get(index as usize) {
            Some(de) => GetV0DEResult::Success(de.clone()),
            None => GetV0DEResult::Error,
        }
    }
    /// Gets the identity details for this V0 payload,
    /// if this payload was associated with an identity.
    fn get_identity_details(&self) -> GetV0IdentityDetailsResult {
        match &self.identity {
            Some(x) => GetV0IdentityDetailsResult::Success(x.details()),
            None => GetV0IdentityDetailsResult::Error,
        }
    }
    /// Attempts to decrypt the metadata for the matched
    /// credential for this V0 payload (if any)
    fn decrypt_metadata(&self) -> Result<Vec<u8>, DecryptMetadataError> {
        match &self.identity {
            None => Err(DecryptMetadataError::EncryptedMetadataNotAvailable),
            Some(identity) => {
                identity.decrypt_metadata().ok_or(DecryptMetadataError::DecryptionFailed)
            }
        }
    }
}

/// A `#[repr(C)]` handle to a value of type `V0PayloadInternals`
#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct V0Payload {
    handle_id: u64,
}

declare_handle_map!(
    v0_payload,
    crate::common::default_handle_map_dimensions(),
    super::V0Payload,
    super::V0PayloadInternals
);

use super::DeserializeAdvertisementError;

impl LocksLongerThan<V0Payload> for CredentialBook {}

impl V0Payload {
    pub(crate) fn allocate_with_plaintext_data_elements(
        data_elements: Vec<legacy::deserialize::DeserializedDataElement<legacy::Plaintext>>,
    ) -> Result<Self, HandleMapFullError> {
        Self::allocate(move || {
            let des = data_elements.into_iter().map(V0DataElement::from).collect();
            let identity = None;
            V0PayloadInternals { des, identity }
        })
    }
    pub(crate) fn allocate_with_decrypted_contents(
        salt: ldt_np_adv::V0Salt,
        match_data: np_adv::credential::matched::WithMatchedCredential<
            MatchedCredential,
            ldt_np_adv::V0IdentityToken,
        >,
        data_elements: Vec<legacy::deserialize::DeserializedDataElement<legacy::Ciphertext>>,
    ) -> Result<Self, HandleMapFullError> {
        Self::allocate(move || {
            let des = data_elements.into_iter().map(V0DataElement::from).collect();
            let identity = Some(DeserializedV0IdentityInternals::new(salt, match_data));
            V0PayloadInternals { des, identity }
        })
    }
    /// Gets the data-element with the given index in this v0 adv payload. This uses the handle but
    /// does not take ownership of it.
    pub fn get_de(&self, index: u8) -> GetV0DEResult {
        match self.get() {
            Ok(read_guard) => read_guard.get_de(index),
            Err(_) => GetV0DEResult::Error,
        }
    }

    /// Gets the identity details for this V0 payload, if this payload was associated with an
    /// identity (i.e: encrypted advertisements). This uses the handle but does not take ownership
    /// of it.
    pub fn get_identity_details(&self) -> GetV0IdentityDetailsResult {
        match self.get() {
            Ok(read_guard) => read_guard.get_identity_details(),
            Err(_) => GetV0IdentityDetailsResult::Error,
        }
    }

    /// Attempts to decrypt the metadata for the matched credential for this V0 payload (if any).
    /// This uses the handle but does not take ownership of it. The caller is given ownership of
    /// the returned `DecryptedMetadata` handle if present.
    pub fn decrypt_metadata(&self) -> DecryptMetadataResult {
        match self.get() {
            Ok(read_guard) => match read_guard.decrypt_metadata() {
                Ok(decrypted_metadata) => allocate_decrypted_metadata_handle(decrypted_metadata),
                Err(_) => DecryptMetadataResult::Error,
            },
            Err(_) => DecryptMetadataResult::Error,
        }
    }

    /// Deallocates any underlying data held by a `V0Payload`. This takes ownership of the handle.
    pub fn deallocate_payload(&self) -> DeallocateResult {
        self.deallocate().map(|_| ()).into()
    }
}

/// Discriminant of `GetV0DEResult`.
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum GetV0DEResultKind {
    /// The attempt to get the DE succeeded.
    /// The associated payload may be obtained via
    /// `GetV0DEResult#into_success`.
    Success = 0,
    /// The attempt to get the DE failed,
    /// possibly due to the requested index being
    /// out-of-bounds or due to the advertisement
    /// having been previously deallocated.
    Error = 1,
}

/// The result of `V0Payload#get_de`.
#[repr(C)]
#[allow(missing_docs)]
pub enum GetV0DEResult {
    Success(V0DataElement),
    Error,
}

impl FfiEnum for GetV0DEResult {
    type Kind = GetV0DEResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            GetV0DEResult::Success(_) => GetV0DEResultKind::Success,
            GetV0DEResult::Error => GetV0DEResultKind::Error,
        }
    }
}

impl GetV0DEResult {
    declare_enum_cast! {into_success, Success, V0DataElement}
}
