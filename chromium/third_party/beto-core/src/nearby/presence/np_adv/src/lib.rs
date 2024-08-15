// Copyright 2022 Google LLC
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

//! Serialization and deserialization for v0 (legacy) and v1 (extended) Nearby Presence
//! advertisements.
//!
//! See `tests/examples_v0.rs` and `tests/examples_v1.rs` for some tests that show common
//! deserialization scenarios.

#![no_std]
#![allow(clippy::expect_used, clippy::indexing_slicing, clippy::panic)]

use crate::{
    credential::{
        book::CredentialBook, v0::V0DiscoveryCryptoMaterial, v0::V0, v1::V1DiscoveryCryptoMaterial,
        v1::V1, DiscoveryCryptoMaterial, MatchedCredential, ProtocolVersion,
        ReferencedMatchedCredential,
    },
    deserialization_arena::ArenaOutOfSpace,
    extended::deserialize::{
        encrypted_section::*, parse_sections, CiphertextSection, DataElementParseError,
        DataElementParsingIterator, DecryptedSection, IntermediateSection, PlaintextSection,
        Section, SectionDeserializeError,
    },
    legacy::deserialize::{
        DecryptError, DecryptedAdvContents, IntermediateAdvContents, PlaintextAdvContents,
    },
};

#[cfg(any(test, feature = "alloc"))]
extern crate alloc;
#[cfg(any(test, feature = "alloc"))]
use alloc::vec::Vec;

use array_vec::ArrayVecOption;
#[cfg(feature = "devtools")]
use array_view::ArrayView;
use core::fmt::Debug;
use crypto_provider::CryptoProvider;
use deserialization_arena::{DeserializationArena, DeserializationArenaAllocator};
#[cfg(feature = "devtools")]
use extended::NP_ADV_MAX_SECTION_LEN;
use extended::NP_V1_ADV_MAX_ENCRYPTED_SECTION_COUNT;
use legacy::{data_elements::DataElementDeserializeError, deserialize::AdvDeserializeError};
use nom::{combinator, number};
pub use strum;

mod array_vec;
pub mod credential;
pub mod de_type;
#[cfg(test)]
mod deser_v0_tests;
#[cfg(test)]
mod deser_v1_tests;
pub mod deserialization_arena;
pub mod extended;
pub mod filter;
#[cfg(test)]
mod header_parse_tests;
pub mod legacy;
pub mod shared_data;

/// Canonical form of NP's service UUID.
///
/// Note that UUIDs are encoded in BT frames in little-endian order, so these bytes may need to be
/// reversed depending on the host BT API.
pub const NP_SVC_UUID: [u8; 2] = [0xFC, 0xF1];

/// Parse, deserialize, decrypt, and validate a complete NP advertisement (the entire contents of
/// the service data for the NP UUID).
pub fn deserialize_advertisement<'adv, 'cred, B, P>(
    arena: DeserializationArena<'adv>,
    adv: &'adv [u8],
    cred_book: &'cred B,
) -> Result<DeserializedAdvertisement<'adv, B::Matched>, AdvDeserializationError>
where
    B: CredentialBook<'cred>,
    P: CryptoProvider,
{
    let (remaining, header) =
        parse_adv_header(adv).map_err(|_e| AdvDeserializationError::HeaderParseError)?;
    match header {
        AdvHeader::V0 => {
            deser_decrypt_v0::<B, P>(cred_book, remaining).map(DeserializedAdvertisement::V0)
        }
        AdvHeader::V1(header) => deser_decrypt_v1::<B, P>(arena, cred_book, remaining, header)
            .map(DeserializedAdvertisement::V1),
    }
}

/// The encryption scheme used for a V1 advertisement.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum V1EncryptionScheme {
    /// Indicates MIC-based encryption and verification.
    Mic,
    /// Indicates signature-based encryption and verification.
    Signature,
}

/// Error in decryption operations for `deser_decrypt_v1_section_bytes_for_dev_tools`.
#[cfg(feature = "devtools")]
#[derive(Debug, Clone)]
pub enum AdvDecryptionError {
    /// Cannot decrypt because the input section is not encrypted.
    InputNotEncrypted,
    /// Error parsing the given section.
    ParseError,
    /// No suitable credential found to decrypt the given section.
    NoMatchingCredentials,
}

/// Decrypt, but do not further deserialize the v1 bytes, intended for developer tooling uses only.
/// Production uses should use [deserialize_advertisement] instead, which deserializes to a
/// structured format and provides extra type safety.
#[cfg(feature = "devtools")]
pub fn deser_decrypt_v1_section_bytes_for_dev_tools<'adv, 'cred, B, P>(
    arena: DeserializationArena<'adv>,
    cred_book: &'cred B,
    header_byte: u8,
    section_bytes: &'adv [u8],
) -> Result<(ArrayView<u8, NP_ADV_MAX_SECTION_LEN>, V1EncryptionScheme), AdvDecryptionError>
where
    B: CredentialBook<'cred>,
    P: CryptoProvider,
{
    let header = V1Header { header_byte };
    let int_sections =
        parse_sections(header, section_bytes).map_err(|_| AdvDecryptionError::ParseError)?;
    let cipher_section = match &int_sections[0] {
        IntermediateSection::Plaintext(_) => Err(AdvDecryptionError::InputNotEncrypted)?,
        IntermediateSection::Ciphertext(section) => section,
    };

    let mut allocator = arena.into_allocator();
    for (crypto_material, _) in cred_book.v1_iter() {
        if let Some(plaintext) = cipher_section
            .try_resolve_identity_and_decrypt::<_, P>(&mut allocator, &crypto_material)
        {
            let pt = plaintext.expect(concat!(
                "Should not run out of space because DeserializationArenaAllocator is big ",
                "enough to hold a single advertisement, and we exit immediately upon ",
                "successful decryption",
            ));

            let encryption_scheme = match cipher_section {
                CiphertextSection::SignatureEncryptedIdentity(_) => V1EncryptionScheme::Signature,
                CiphertextSection::MicEncryptedIdentity(_) => V1EncryptionScheme::Mic,
            };
            return Ok((pt, encryption_scheme));
        }
    }
    Err(AdvDecryptionError::NoMatchingCredentials)
}

/// A ciphertext section which has not yet been
/// resolved to an identity, but for which some
/// `SectionIdentityResolutionContents` have been
/// pre-computed for speedy identity-resolution.
struct ResolvableCiphertextSection<'a> {
    identity_resolution_contents: SectionIdentityResolutionContents,
    ciphertext_section: CiphertextSection<'a>,
}

/// A collection of possibly-deserialized sections which are separated according
/// to whether/not they're intermediate encrypted sections (of either type)
/// or fully-deserialized, with a running count of the number of malformed sections.
/// Each potentially-valid section is tagged with a 0-based index derived from the original
/// section ordering as they appeared within the original advertisement to ensure
/// that the fully-deserialized advertisement may be correctly reconstructed.
struct SectionsInProcessing<'adv, M: MatchedCredential> {
    deserialized_sections: ArrayVecOption<
        (usize, V1DeserializedSection<'adv, M>),
        { NP_V1_ADV_MAX_ENCRYPTED_SECTION_COUNT },
    >,
    encrypted_sections: ArrayVecOption<
        (usize, ResolvableCiphertextSection<'adv>),
        { NP_V1_ADV_MAX_ENCRYPTED_SECTION_COUNT },
    >,
    malformed_sections_count: usize,
}

impl<'adv, M: MatchedCredential> SectionsInProcessing<'adv, M> {
    /// Attempts to parse a V1 advertisement's contents after the version header
    /// into a collection of not-yet-fully-deserialized sections which may
    /// require credentials to be decrypted.
    fn from_advertisement_contents<C: CryptoProvider>(
        header: V1Header,
        remaining: &'adv [u8],
    ) -> Result<Self, AdvDeserializationError> {
        let int_sections =
            parse_sections(header, remaining).map_err(|_| AdvDeserializationError::ParseError {
                details_hazmat: AdvDeserializationErrorDetailsHazmat::AdvertisementDeserializeError,
            })?;
        let mut deserialized_sections = ArrayVecOption::default();
        let mut encrypted_sections = ArrayVecOption::default();
        // keep track of ordering for later sorting during `self.finished_with_decryption_attempts()`.
        for (idx, s) in int_sections.into_iter().enumerate() {
            match s {
                IntermediateSection::Plaintext(p) => {
                    deserialized_sections.push((idx, V1DeserializedSection::Plaintext(p)))
                }
                IntermediateSection::Ciphertext(ciphertext_section) => {
                    let identity_resolution_contents =
                        ciphertext_section.contents().compute_identity_resolution_contents::<C>();
                    let resolvable_ciphertext_section = ResolvableCiphertextSection {
                        identity_resolution_contents,
                        ciphertext_section,
                    };
                    encrypted_sections.push((idx, resolvable_ciphertext_section));
                }
            }
        }
        Ok(Self { deserialized_sections, encrypted_sections, malformed_sections_count: 0 })
    }

    /// Returns true iff we have resolved all sections to identities.
    fn resolved_all_identities(&self) -> bool {
        self.encrypted_sections.is_empty()
    }

    /// Runs through all of the encrypted sections in processing, and attempts
    /// to use the given credential to decrypt them. Suitable for situations
    /// where iterating over credentials is relatively slow compared to
    /// the cost of iterating over sections-in-memory.
    fn try_decrypt_with_credential<C: V1DiscoveryCryptoMaterial, P: CryptoProvider>(
        &mut self,
        arena: &mut DeserializationArenaAllocator<'adv>,
        crypto_material: C,
        match_data: M,
    ) -> Result<(), ArenaOutOfSpace> {
        let mut i = 0;
        while i < self.encrypted_sections.len() {
            let (section_idx, section): &(usize, ResolvableCiphertextSection) =
                &self.encrypted_sections[i];
            // Fast-path: Check for an identity match, ignore if there's no identity match.
            let identity_resolution_contents = &section.identity_resolution_contents;
            let identity_resolution_material = match &section.ciphertext_section {
                CiphertextSection::MicEncryptedIdentity(_) => crypto_material
                    .unsigned_identity_resolution_material::<P>()
                    .into_raw_resolution_material(),
                CiphertextSection::SignatureEncryptedIdentity(_) => crypto_material
                    .signed_identity_resolution_material::<P>()
                    .into_raw_resolution_material(),
            };
            match identity_resolution_contents.try_match::<P>(&identity_resolution_material) {
                None => {
                    // Try again with another section
                    i += 1;
                    continue;
                }
                Some(identity_match) => {
                    // The identity matched, so now we need to more closely scrutinize
                    // the provided ciphertext. Try to decrypt and parse the section.
                    let metadata_nonce = crypto_material.metadata_nonce::<P>();
                    let deserialization_result = match &section.ciphertext_section {
                        CiphertextSection::SignatureEncryptedIdentity(c) => c
                            .try_deserialize(
                                arena,
                                identity_match,
                                &crypto_material.signed_verification_material::<P>(),
                            )
                            .map_err(SectionDeserializeError::from),
                        CiphertextSection::MicEncryptedIdentity(c) => c
                            .try_deserialize(
                                arena,
                                identity_match,
                                &crypto_material.unsigned_verification_material::<P>(),
                            )
                            .map_err(SectionDeserializeError::from),
                    };
                    match deserialization_result {
                        Ok(s) => {
                            self.deserialized_sections.push((
                                *section_idx,
                                V1DeserializedSection::Decrypted(WithMatchedCredential::new(
                                    match_data.clone(),
                                    metadata_nonce,
                                    s,
                                )),
                            ));
                        }
                        Err(e) => match e {
                            SectionDeserializeError::IncorrectCredential => {
                                // keep it around to try with another credential
                                i += 1;
                                continue;
                            }
                            SectionDeserializeError::ParseError => {
                                // the credential worked, but the section itself was bogus
                                self.malformed_sections_count += 1;
                            }
                            SectionDeserializeError::ArenaOutOfSpace => {
                                return Err(ArenaOutOfSpace)
                            }
                        },
                    }
                    // By default, if we have an identity match, assume that decrypting the section worked,
                    // or that the section was somehow invalid.
                    // We don't care about maintaining order, so use O(1) remove
                    let _ = self.encrypted_sections.swap_remove(i);
                    // don't advance i -- it now points to a new element
                }
            }
        }
        Ok(())
    }

    /// Packages the current state of the deserialization process into a
    /// `V1AdvertisementContents` representing a fully-deserialized V1 advertisement.
    ///
    /// This method should only be called after all sections were either successfully
    /// decrypted or have had all relevant credentials checked against
    /// them without obtaining a successful identity-match and/or subsequent
    /// cryptographic verification of the section contents.
    fn finished_with_decryption_attempts(mut self) -> V1AdvertisementContents<'adv, M> {
        // Invalid sections = malformed sections + number of encrypted sections
        // which we could not manage to decrypt with any of our credentials
        let invalid_sections_count = self.malformed_sections_count + self.encrypted_sections.len();

        // Put the deserialized sections back into the original ordering for
        // the returned `V1AdvertisementContents`
        // (Note: idx is unique, so unstable sort is ok)
        self.deserialized_sections.sort_unstable_by_key(|(idx, _section)| *idx);
        let ordered_sections = self.deserialized_sections.into_iter().map(|(_idx, s)| s).collect();
        V1AdvertisementContents::new(ordered_sections, invalid_sections_count)
    }
}

/// Deserialize and decrypt the contents of a v1 adv after the version header
fn deser_decrypt_v1<'adv, 'cred, B, P>(
    arena: DeserializationArena<'adv>,
    cred_book: &'cred B,
    remaining: &'adv [u8],
    header: V1Header,
) -> Result<V1AdvertisementContents<'adv, B::Matched>, AdvDeserializationError>
where
    B: CredentialBook<'cred>,
    P: CryptoProvider,
{
    let mut sections_in_processing =
        SectionsInProcessing::<'_, B::Matched>::from_advertisement_contents::<P>(
            header, remaining,
        )?;

    let mut allocator = arena.into_allocator();

    // Hot loop
    // We assume that iterating credentials is more expensive than iterating sections
    for (crypto_material, match_data) in cred_book.v1_iter() {
        sections_in_processing
            .try_decrypt_with_credential::<_, P>(&mut allocator, crypto_material, match_data)
            .expect(concat!(
                "Should not run out of space because DeserializationArenaAllocator is big ",
                "enough to hold a single advertisement, and we exit immediately upon ",
                "successful decryption",
            ));
        if sections_in_processing.resolved_all_identities() {
            // No need to consider the other credentials
            break;
        }
    }
    Ok(sections_in_processing.finished_with_decryption_attempts())
}

/// Deserialize and decrypt the contents of a v0 adv after the version header
fn deser_decrypt_v0<'adv, 'cred, B, P>(
    cred_book: &'cred B,
    remaining: &'adv [u8],
) -> Result<V0AdvertisementContents<'adv, B::Matched>, AdvDeserializationError>
where
    B: CredentialBook<'cred>,
    P: CryptoProvider,
{
    let contents = legacy::deserialize::deserialize_adv_contents::<P>(remaining)?;
    match contents {
        IntermediateAdvContents::Plaintext(p) => Ok(V0AdvertisementContents::Plaintext(p)),
        IntermediateAdvContents::Ciphertext(c) => {
            for (crypto_material, matched) in cred_book.v0_iter() {
                let ldt = crypto_material.ldt_adv_cipher::<P>();
                match c.try_decrypt(&ldt) {
                    Ok(c) => {
                        let metadata_nonce = crypto_material.metadata_nonce::<P>();
                        return Ok(V0AdvertisementContents::Decrypted(WithMatchedCredential::new(
                            matched,
                            metadata_nonce,
                            c,
                        )));
                    }
                    Err(e) => match e {
                        DecryptError::DecryptOrVerifyError => continue,
                        DecryptError::DeserializeError(e) => {
                            return Err(e.into());
                        }
                    },
                }
            }
            Ok(V0AdvertisementContents::NoMatchingCredentials)
        }
    }
}

/// Parse a NP advertisement header.
///
/// This can be used on all versions of advertisements since it's the header that determines the
/// version.
///
/// Returns a `nom::IResult` with the parsed header and the remaining bytes of the advertisement.
fn parse_adv_header(adv: &[u8]) -> nom::IResult<&[u8], AdvHeader> {
    // header bits: VVVxxxxx
    let (remaining, (header_byte, version, _low_bits)) = combinator::verify(
        // splitting a byte at a bit boundary to take lower 5 bits
        combinator::map(number::complete::u8, |byte| (byte, byte >> 5, byte & 0x1F)),
        |&(_header_byte, version, low_bits)| match version {
            // reserved bits, for any version, must be zero
            PROTOCOL_VERSION_LEGACY | PROTOCOL_VERSION_EXTENDED => low_bits == 0,
            _ => false,
        },
    )(adv)?;
    match version {
        PROTOCOL_VERSION_LEGACY => Ok((remaining, AdvHeader::V0)),
        PROTOCOL_VERSION_EXTENDED => Ok((remaining, AdvHeader::V1(V1Header { header_byte }))),
        _ => unreachable!(),
    }
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub(crate) enum AdvHeader {
    V0,
    V1(V1Header),
}

/// An NP advertisement with its header parsed.
#[allow(clippy::large_enum_variant)]
#[derive(Debug, PartialEq, Eq)]
pub enum DeserializedAdvertisement<'adv, M: MatchedCredential> {
    /// V0 header has all reserved bits, so there is no data to represent other than the version
    /// itself.
    V0(V0AdvertisementContents<'adv, M>),
    /// V1 advertisement
    V1(V1AdvertisementContents<'adv, M>),
}

impl<'adv, M: MatchedCredential> DeserializedAdvertisement<'adv, M> {
    /// Attempts to cast this deserialized advertisement into the `V0AdvertisementContents`
    /// variant. If the underlying advertisement is not V0, this will instead return `None`.
    pub fn into_v0(self) -> Option<V0AdvertisementContents<'adv, M>> {
        match self {
            Self::V0(x) => Some(x),
            _ => None,
        }
    }
    /// Attempts to cast this deserialized advertisement into the `V1AdvertisementContents`
    /// variant. If the underlying advertisement is not V1, this will instead return `None`.
    pub fn into_v1(self) -> Option<V1AdvertisementContents<'adv, M>> {
        match self {
            Self::V1(x) => Some(x),
            _ => None,
        }
    }
}

/// The contents of a deserialized and decrypted V1 advertisement.
#[derive(Debug, PartialEq, Eq)]
pub struct V1AdvertisementContents<'adv, M: MatchedCredential> {
    sections: ArrayVecOption<V1DeserializedSection<'adv, M>, NP_V1_ADV_MAX_ENCRYPTED_SECTION_COUNT>,
    invalid_sections: usize,
}

impl<'adv, M: MatchedCredential> V1AdvertisementContents<'adv, M> {
    fn new(
        sections: ArrayVecOption<
            V1DeserializedSection<'adv, M>,
            NP_V1_ADV_MAX_ENCRYPTED_SECTION_COUNT,
        >,
        invalid_sections: usize,
    ) -> Self {
        Self { sections, invalid_sections }
    }

    /// Destructures this V1 advertisement into just the sections
    /// which could be successfully deserialized and decrypted
    pub fn into_sections(
        self,
    ) -> ArrayVecOption<V1DeserializedSection<'adv, M>, NP_V1_ADV_MAX_ENCRYPTED_SECTION_COUNT> {
        self.sections
    }

    /// The sections that could be successfully deserialized and decrypted
    pub fn sections(&self) -> impl ExactSizeIterator<Item = &V1DeserializedSection<M>> {
        self.sections.iter()
    }

    /// The number of sections that could not be parsed or decrypted.
    pub fn invalid_sections_count(&self) -> usize {
        self.invalid_sections
    }
}

/// Advertisement content that was either already plaintext or has been decrypted.
#[derive(Debug, PartialEq, Eq)]
pub enum V0AdvertisementContents<'adv, M: MatchedCredential> {
    /// Contents of an originally plaintext advertisement
    Plaintext(PlaintextAdvContents<'adv>),
    /// Contents that was ciphertext in the original advertisement, and has been decrypted
    /// with the credential in the [MatchedCredential]
    Decrypted(WithMatchedCredential<M, DecryptedAdvContents>),
    /// The advertisement was encrypted, but no credentials matched
    NoMatchingCredentials,
}

/// Advertisement content that was either already plaintext or has been decrypted.
#[derive(Debug, PartialEq, Eq)]
pub enum V1DeserializedSection<'adv, M: MatchedCredential> {
    /// Section that was plaintext in the original advertisement
    Plaintext(PlaintextSection<'adv>),
    /// Section that was ciphertext in the original advertisement, and has been decrypted
    /// with the credential in the [MatchedCredential]
    Decrypted(WithMatchedCredential<M, DecryptedSection<'adv>>),
}

impl<'adv, M> Section<'adv, DataElementParseError> for V1DeserializedSection<'adv, M>
where
    M: MatchedCredential,
{
    type Iterator = DataElementParsingIterator<'adv>;

    fn iter_data_elements(&self) -> Self::Iterator {
        match self {
            V1DeserializedSection::Plaintext(p) => p.iter_data_elements(),
            V1DeserializedSection::Decrypted(d) => d.contents.iter_data_elements(),
        }
    }
}

/// 16-byte metadata keys, as employed for metadata decryption.
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq)]
pub struct MetadataKey(pub [u8; 16]);

impl AsRef<[u8]> for MetadataKey {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

/// Common trait to deserialized, decrypted V0 advs and V1 sections which
/// exposes relevant data about matched identities.
pub trait HasIdentityMatch {
    /// The protocol version for which this advertisement
    /// content has an identity-match.
    type Version: ProtocolVersion;

    /// Gets the decrypted plaintext version-specific
    /// metadata key for the associated identity.
    fn metadata_key(&self) -> <Self::Version as ProtocolVersion>::MetadataKey;
}

impl HasIdentityMatch for legacy::ShortMetadataKey {
    type Version = V0;
    fn metadata_key(&self) -> Self {
        *self
    }
}

impl HasIdentityMatch for MetadataKey {
    type Version = V1;
    fn metadata_key(&self) -> Self {
        *self
    }
}

#[cfg(any(test, feature = "alloc"))]
/// Type for errors from [`WithMatchedCredential#decrypt_metadata`]
#[derive(Debug)]
pub enum MatchedMetadataDecryptionError<M: MatchedCredential> {
    /// Retrieving the encrypted metadata failed for one reason
    /// or another, so we didn't get a chance to try decryption.
    RetrievalFailed(<M as MatchedCredential>::EncryptedMetadataFetchError),
    /// The encrypted metadata could be retrieved, but it did
    /// not successfully decrypt against the matched identity.
    /// This could be an indication of data corruption or
    /// of malformed crypto on the sender-side.
    DecryptionFailed,
}

/// Decrypted advertisement content with the [MatchedCredential] from the credential that decrypted
/// it, along with any other information which is relevant to the identity-match.
#[derive(Debug, PartialEq, Eq)]
pub struct WithMatchedCredential<M: MatchedCredential, T: HasIdentityMatch> {
    matched: M,
    /// The 12-byte metadata nonce as derived from the key-seed HKDF
    /// to be used for decrypting the encrypted metadata in the attached
    /// matched-credential.
    metadata_nonce: [u8; 12],
    contents: T,
}
impl<'a, M: MatchedCredential + Clone, T: HasIdentityMatch>
    WithMatchedCredential<ReferencedMatchedCredential<'a, M>, T>
{
    /// Clones the referenced match-data to update this container
    /// so that the match-data is owned, rather than borrowed.
    pub fn clone_match_data(self) -> WithMatchedCredential<M, T> {
        let matched = self.matched.as_ref().clone();
        let metadata_nonce = self.metadata_nonce;
        let contents = self.contents;

        WithMatchedCredential { matched, metadata_nonce, contents }
    }
}
impl<M: MatchedCredential, T: HasIdentityMatch> WithMatchedCredential<M, T> {
    fn new(matched: M, metadata_nonce: [u8; 12], contents: T) -> Self {
        Self { matched, metadata_nonce, contents }
    }
    /// Applies the given function to the wrapped contents, yielding
    /// a new instance with the same matched-credential.
    pub fn map<R: HasIdentityMatch>(
        self,
        mapping: impl FnOnce(T) -> R,
    ) -> WithMatchedCredential<M, R> {
        let contents = mapping(self.contents);
        let matched = self.matched;
        let metadata_nonce = self.metadata_nonce;
        WithMatchedCredential { matched, metadata_nonce, contents }
    }
    /// Credential data for the credential that decrypted the content.
    pub fn matched_credential(&self) -> &M {
        &self.matched
    }
    /// The decrypted advertisement content.
    pub fn contents(&self) -> &T {
        &self.contents
    }

    #[cfg(any(test, feature = "alloc"))]
    fn decrypt_metadata_from_fetch<C: CryptoProvider>(
        &self,
        encrypted_metadata: &[u8],
    ) -> Result<Vec<u8>, MatchedMetadataDecryptionError<M>> {
        let metadata_key = self.contents.metadata_key();
        <<T as HasIdentityMatch>::Version as ProtocolVersion>::decrypt_metadata::<C>(
            self.metadata_nonce,
            metadata_key,
            encrypted_metadata,
        )
        .map_err(|_| MatchedMetadataDecryptionError::DecryptionFailed)
    }

    #[cfg(any(test, feature = "alloc"))]
    /// Attempts to decrypt the encrypted metadata
    /// associated with the matched credential
    /// based on the details of the identity-match.
    pub fn decrypt_metadata<C: CryptoProvider>(
        &self,
    ) -> Result<Vec<u8>, MatchedMetadataDecryptionError<M>> {
        self.matched
            .fetch_encrypted_metadata()
            .map_err(|e| MatchedMetadataDecryptionError::RetrievalFailed(e))
            .and_then(|x| Self::decrypt_metadata_from_fetch::<C>(self, x.as_ref()))
    }
}

/// Data in a V1 advertisement header.
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub(crate) struct V1Header {
    header_byte: u8,
}

const PROTOCOL_VERSION_LEGACY: u8 = 0;
const PROTOCOL_VERSION_EXTENDED: u8 = 1;

/// Errors that can occur during advertisement deserialization.
#[derive(PartialEq)]
pub enum AdvDeserializationError {
    /// The advertisement header could not be parsed
    HeaderParseError,
    /// The advertisement content could not be parsed
    ParseError {
        /// Potentially hazardous details about deserialization errors. Read the documentation for
        /// [AdvDeserializationErrorDetailsHazmat] before using this field.
        details_hazmat: AdvDeserializationErrorDetailsHazmat,
    },
}

impl Debug for AdvDeserializationError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            AdvDeserializationError::HeaderParseError => write!(f, "HeaderParseError"),
            AdvDeserializationError::ParseError { .. } => write!(f, "ParseError"),
        }
    }
}

/// Potentially hazardous details about deserialization errors. These error information can
/// potentially expose side-channel information about the plaintext of the advertisements and/or
/// the keys used to decrypt them. For any place that you avoid exposing the keys directly
/// (e.g. across FFIs, print to log, etc), avoid exposing these error details as well.
#[derive(PartialEq)]
pub enum AdvDeserializationErrorDetailsHazmat {
    /// Parsing the overall advertisement or DE structure failed
    AdvertisementDeserializeError,
    /// Deserializing an individual DE from its DE contents failed
    V0DataElementDeserializeError(DataElementDeserializeError),
    /// Must not have any other top level data elements if there is an encrypted identity DE
    TooManyTopLevelDataElements,
    /// Must not have an identity DE inside an identity DE
    InvalidDataElementHierarchy,
    /// Must have an identity DE
    MissingIdentity,
    /// Non-identity DE contents must not be empty
    NoPublicDataElements,
}

impl From<AdvDeserializeError> for AdvDeserializationError {
    fn from(err: AdvDeserializeError) -> Self {
        match err {
            AdvDeserializeError::AdvertisementDeserializeError => {
                AdvDeserializationError::ParseError {
                    details_hazmat:
                        AdvDeserializationErrorDetailsHazmat::AdvertisementDeserializeError,
                }
            }
            AdvDeserializeError::TooManyTopLevelDataElements => {
                AdvDeserializationError::ParseError {
                    details_hazmat:
                        AdvDeserializationErrorDetailsHazmat::TooManyTopLevelDataElements,
                }
            }
            AdvDeserializeError::MissingIdentity => AdvDeserializationError::ParseError {
                details_hazmat: AdvDeserializationErrorDetailsHazmat::MissingIdentity,
            },
            AdvDeserializeError::NoPublicDataElements => AdvDeserializationError::ParseError {
                details_hazmat: AdvDeserializationErrorDetailsHazmat::NoPublicDataElements,
            },
        }
    }
}

/// DE length is out of range (e.g. > 4 bits for encoded V0, > max DE size for actual V0, >127 for
/// V1) or invalid for the relevant DE type.
#[derive(Debug, PartialEq, Eq)]
pub struct DeLengthOutOfRange;

/// The identity mode for a deserialized plaintext section or advertisement.
#[derive(PartialEq, Eq, Debug, Clone, Copy)]
pub enum PlaintextIdentityMode {
    /// A "Public Identity" DE was present in the section
    Public,
}

/// A "public identity" -- a nonspecific "empty identity".
///
/// Used when serializing V0 advertisements or V1 sections.
#[derive(Default, Debug)]
pub struct PublicIdentity;
