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

//! Deserialization for V1 advertisement contents
#[cfg(any(test, feature = "alloc"))]
use alloc::vec::Vec;

use core::{array::TryFromSliceError, fmt::Debug};

use crate::{
    array_vec::ArrayVecOption,
    credential::{
        book::CredentialBook,
        matched::{HasIdentityMatch, MatchedCredential, WithMatchedCredential},
        v1::{V1DiscoveryCryptoMaterial, V1},
    },
    deserialization_arena::{ArenaOutOfSpace, DeserializationArena, DeserializationArenaAllocator},
    extended::{
        deserialize::{
            data_element::{DataElement, DataElementParseError, DataElementParsingIterator},
            encrypted_section::{
                DeserializationError, MicVerificationError, SectionIdentityResolutionContents,
                SignatureVerificationError,
            },
            section::intermediate::{
                parse_sections, CiphertextSection, IntermediateSection, PlaintextSection,
            },
        },
        salt::MultiSalt,
        V1IdentityToken, NP_V1_ADV_MAX_SECTION_COUNT,
    },
    header::V1AdvHeader,
    AdvDeserializationError, AdvDeserializationErrorDetailsHazmat,
};
use crypto_provider::CryptoProvider;

#[cfg(test)]
mod tests;

pub mod data_element;
pub(crate) mod encrypted_section;
pub(crate) mod section;

/// Provides deserialization APIs which expose more of the internals, suitable for use only in
/// dev tools.
#[cfg(feature = "devtools")]
pub mod dev_tools;

/// Deserialize and decrypt the contents of a v1 adv after the version header
pub(crate) fn deser_decrypt_v1<'adv, 'cred, B, P>(
    arena: DeserializationArena<'adv>,
    cred_book: &'cred B,
    remaining: &'adv [u8],
    header: V1AdvHeader,
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

/// A section deserialized from a V1 advertisement.
pub trait Section<'adv> {
    /// The iterator type used to iterate over data elements
    type Iterator: Iterator<Item = Result<DataElement<'adv>, DataElementParseError>>;

    /// Iterator over the data elements in a section, except for any DEs related to resolving the
    /// identity or otherwise validating the payload (e.g. MIC, Signature, any identity DEs like
    /// Private Identity).
    fn iter_data_elements(&self) -> Self::Iterator;

    /// Collects the data elements into a vector, eagerly catching and resolving any errors during
    /// parsing.
    #[cfg(any(test, feature = "alloc"))]
    fn collect_data_elements(&self) -> Result<Vec<DataElement<'adv>>, DataElementParseError>
    where
        Self: Sized,
    {
        self.iter_data_elements().collect()
    }
}

/// Fully-parsed and verified decrypted contents from an encrypted section.
#[derive(Debug, PartialEq, Eq)]
pub struct DecryptedSection<'adv> {
    verification_mode: VerificationMode,
    identity_token: V1IdentityToken,
    salt: MultiSalt,
    /// Decrypted DE data, excluding any encoder suffix
    plaintext: &'adv [u8],
}

impl<'adv> DecryptedSection<'adv> {
    fn new(
        verification_mode: VerificationMode,
        salt: MultiSalt,
        identity_token: V1IdentityToken,
        plaintext: &'adv [u8],
    ) -> Self {
        Self { verification_mode, identity_token, salt, plaintext }
    }

    /// The verification mode used in the section.
    pub fn verification_mode(&self) -> VerificationMode {
        self.verification_mode
    }

    /// The identity token extracted from the section
    pub fn identity_token(&self) -> &V1IdentityToken {
        &self.identity_token
    }

    /// The salt used for decryption of this section.
    pub fn salt(&self) -> &MultiSalt {
        &self.salt
    }

    #[cfg(test)]
    pub(crate) fn plaintext(&self) -> &'adv [u8] {
        self.plaintext
    }
}

impl<'adv> HasIdentityMatch for DecryptedSection<'adv> {
    type Version = V1;
    fn identity_token(&self) -> V1IdentityToken {
        self.identity_token
    }
}

impl<'adv> Section<'adv> for DecryptedSection<'adv> {
    type Iterator = DataElementParsingIterator<'adv>;

    fn iter_data_elements(&self) -> Self::Iterator {
        DataElementParsingIterator::new(self.plaintext)
    }
}

/// Errors that can occur when deserializing a section
/// of a V1 advertisement.
#[derive(Debug, PartialEq, Eq)]
pub enum SectionDeserializeError {
    /// The credential supplied did not match the section's identity data
    IncorrectCredential,
    /// Section data is malformed
    ParseError,
    /// The given arena is not large enough to hold the decrypted contents
    ArenaOutOfSpace,
}

impl From<DeserializationError<MicVerificationError>> for SectionDeserializeError {
    fn from(mic_deserialization_error: DeserializationError<MicVerificationError>) -> Self {
        match mic_deserialization_error {
            DeserializationError::VerificationError(MicVerificationError::MicMismatch) => {
                Self::IncorrectCredential
            }
            DeserializationError::ArenaOutOfSpace => Self::ArenaOutOfSpace,
        }
    }
}

impl From<DeserializationError<SignatureVerificationError>> for SectionDeserializeError {
    fn from(
        signature_deserialization_error: DeserializationError<SignatureVerificationError>,
    ) -> Self {
        match signature_deserialization_error {
            DeserializationError::VerificationError(
                SignatureVerificationError::SignatureMissing,
            ) => Self::ParseError,
            DeserializationError::VerificationError(
                SignatureVerificationError::SignatureMismatch,
            ) => Self::IncorrectCredential,
            DeserializationError::ArenaOutOfSpace => Self::ArenaOutOfSpace,
        }
    }
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
    deserialized_sections:
        ArrayVecOption<(usize, V1DeserializedSection<'adv, M>), { NP_V1_ADV_MAX_SECTION_COUNT }>,
    encrypted_sections:
        ArrayVecOption<(usize, ResolvableCiphertextSection<'adv>), { NP_V1_ADV_MAX_SECTION_COUNT }>,
    malformed_sections_count: usize,
}

impl<'adv, M: MatchedCredential> SectionsInProcessing<'adv, M> {
    /// Attempts to parse a V1 advertisement's contents after the version header
    /// into a collection of not-yet-fully-deserialized sections which may
    /// require credentials to be decrypted.
    fn from_advertisement_contents<C: CryptoProvider>(
        header: V1AdvHeader,
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
                        ciphertext_section.identity_resolution_contents::<C>();
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
                CiphertextSection::MicEncrypted(m) => match m.contents.salt {
                    MultiSalt::Short(_) => crypto_material
                        .mic_short_salt_identity_resolution_material::<P>()
                        .into_raw_resolution_material(),
                    MultiSalt::Extended(_) => crypto_material
                        .mic_extended_salt_identity_resolution_material::<P>()
                        .into_raw_resolution_material(),
                },

                CiphertextSection::SignatureEncrypted(_) => crypto_material
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
                    let deserialization_result = match &section.ciphertext_section {
                        CiphertextSection::SignatureEncrypted(c) => c
                            .try_deserialize(
                                arena,
                                identity_match,
                                &crypto_material.signed_verification_material::<P>(),
                            )
                            .map_err(SectionDeserializeError::from),
                        CiphertextSection::MicEncrypted(c) => c
                            .try_deserialize(arena, identity_match, &crypto_material)
                            .map_err(SectionDeserializeError::from),
                    };
                    match deserialization_result {
                        Ok(s) => {
                            self.deserialized_sections.push((
                                *section_idx,
                                V1DeserializedSection::Decrypted(WithMatchedCredential::new(
                                    match_data.clone(),
                                    crypto_material.metadata_nonce::<P>(),
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
                                return Err(ArenaOutOfSpace);
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

/// The contents of a deserialized and decrypted V1 advertisement.
#[derive(Debug, PartialEq, Eq)]
pub struct V1AdvertisementContents<'adv, M: MatchedCredential> {
    sections: ArrayVecOption<V1DeserializedSection<'adv, M>, NP_V1_ADV_MAX_SECTION_COUNT>,
    invalid_sections: usize,
}

impl<'adv, M: MatchedCredential> V1AdvertisementContents<'adv, M> {
    fn new(
        sections: ArrayVecOption<V1DeserializedSection<'adv, M>, NP_V1_ADV_MAX_SECTION_COUNT>,
        invalid_sections: usize,
    ) -> Self {
        Self { sections, invalid_sections }
    }

    /// Destructures this V1 advertisement into just the sections
    /// which could be successfully deserialized and decrypted
    pub fn into_sections(
        self,
    ) -> ArrayVecOption<V1DeserializedSection<'adv, M>, NP_V1_ADV_MAX_SECTION_COUNT> {
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
pub enum V1DeserializedSection<'adv, M: MatchedCredential> {
    /// Section that was plaintext in the original advertisement
    Plaintext(PlaintextSection<'adv>),
    /// Section that was ciphertext in the original advertisement, and has been decrypted
    /// with the credential in the [MatchedCredential]
    Decrypted(WithMatchedCredential<M, DecryptedSection<'adv>>),
}

// Allow easy DE iteration if the user doesn't care which kind of section it is
impl<'adv, M: MatchedCredential> Section<'adv> for V1DeserializedSection<'adv, M> {
    type Iterator = DataElementParsingIterator<'adv>;

    fn iter_data_elements(&self) -> Self::Iterator {
        match self {
            V1DeserializedSection::Plaintext(p) => p.iter_data_elements(),
            V1DeserializedSection::Decrypted(d) => d.contents().iter_data_elements(),
        }
    }
}

/// The level of integrity protection in an encrypted section
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum VerificationMode {
    /// A symmetric MIC (message integrity code aka message authentication code) was verified.
    ///
    /// Since this is a symmetric operation, holders of the key material needed to verify a MIC
    /// can also forge MICs.
    Mic,
    /// An asymmetric signature was verified.
    ///
    /// Since this is an asymmetric operation, only the holder of the private key can generate
    /// signatures, so it offers a stronger level of authenticity protection than [Self::Mic].
    Signature,
}

#[derive(PartialEq, Eq, Debug)]
pub(crate) struct SectionMic {
    mic: [u8; Self::CONTENTS_LEN],
}

impl SectionMic {
    /// 16-byte MIC
    pub(crate) const CONTENTS_LEN: usize = 16;
}

impl From<[u8; 16]> for SectionMic {
    fn from(value: [u8; 16]) -> Self {
        SectionMic { mic: value }
    }
}

impl TryFrom<&[u8]> for SectionMic {
    type Error = TryFromSliceError;

    fn try_from(value: &[u8]) -> Result<Self, Self::Error> {
        let fixed_bytes: [u8; SectionMic::CONTENTS_LEN] = value.try_into()?;
        Ok(Self { mic: fixed_bytes })
    }
}

fn hi_bit_set(b: u8) -> bool {
    b & 0x80 > 0
}
