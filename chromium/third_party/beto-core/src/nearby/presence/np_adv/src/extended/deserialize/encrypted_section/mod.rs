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

use crate::{
    credential::v1::*,
    extended::{
        deserialize::{DecryptedSection, SectionMic, VerificationMode},
        section_signature_payload::*,
        V1IdentityToken, NP_ADV_MAX_SECTION_LEN, V1_IDENTITY_TOKEN_LEN,
    },
    NP_SVC_UUID,
};

use crate::deserialization_arena::DeserializationArenaAllocator;

#[cfg(any(feature = "devtools", test))]
extern crate alloc;

use crate::{
    extended::{
        deserialize::section::header::CiphertextExtendedIdentityToken,
        salt::{MultiSalt, V1Salt},
    },
    header::V1AdvHeader,
};
#[cfg(any(feature = "devtools", test))]
use alloc::vec::Vec;
#[cfg(feature = "devtools")]
use array_view::ArrayView;
use core::fmt::Debug;
use crypto_provider::{
    aes::ctr::{AesCtr, AesCtrNonce, NonceAndCounter},
    hmac::Hmac,
    CryptoProvider,
};
use np_hkdf::v1_salt::ExtendedV1Salt;

#[cfg(test)]
use crate::extended::deserialize::encrypted_section::tests::IdentityResolutionOrDeserializationError;

use super::ArenaOutOfSpace;

#[cfg(test)]
mod tests;

/// Represents the contents of an encrypted section
/// which are directly employed in identity resolution.
/// This does not incorporate any information about credentials.
///
/// Should be re-used for multiple identity resolution attempts, if applicable, to amortize the
/// cost of calculating this data.
#[derive(PartialEq, Eq, Debug)]
pub(crate) struct SectionIdentityResolutionContents {
    /// The ciphertext for the identity token
    pub(crate) identity_token: CiphertextExtendedIdentityToken,
    /// The 12-byte cryptographic nonce which is derived from the salt for a
    /// particular section.
    pub(crate) nonce: AesCtrNonce,
}

impl SectionIdentityResolutionContents {
    /// Decrypt the contained metadata-key ciphertext buffer whose bytes of plaintext are, maybe, the
    /// metadata key for an NP identity, as specified via pre-calculated cryptographic materials
    /// stored in some [`SectionIdentityResolutionMaterial`].
    ///
    /// This method does not decrypt an entire section's ciphertext aside from the metadata key,
    /// and so verification (MIC or signature) needs to be done elsewhere.
    ///
    /// Returns `Some` if decrypting the metadata-key ciphertext produces plaintext whose HMAC
    /// matches the expected MAC. Otherwise, returns `None`.
    pub(crate) fn try_match<C: CryptoProvider>(
        &self,
        identity_resolution_material: &SectionIdentityResolutionMaterial,
    ) -> Option<IdentityMatch<C>> {
        let mut decrypt_buf = self.identity_token.0;
        let mut cipher = C::AesCtr128::new(
            &identity_resolution_material.aes_key,
            NonceAndCounter::from_nonce(self.nonce),
        );
        cipher.apply_keystream(&mut decrypt_buf[..]);

        let identity_token_hmac_key: np_hkdf::NpHmacSha256Key =
            identity_resolution_material.identity_token_hmac_key.into();
        identity_token_hmac_key
            .verify_hmac::<C>(
                &decrypt_buf[..],
                identity_resolution_material.expected_identity_token_hmac,
            )
            .ok()
            .map(move |_| IdentityMatch {
                cipher,
                identity_token: V1IdentityToken(decrypt_buf),
                nonce: self.nonce,
            })
    }
}

/// Carries data about an identity "match" for a particular section
/// against some particular V1 identity-resolution crypto-materials.
pub(crate) struct IdentityMatch<C: CryptoProvider> {
    /// Decrypted identity token
    identity_token: V1IdentityToken,
    /// The AES-Ctr nonce to be used in section decryption and verification
    nonce: AesCtrNonce,
    /// The state of the AES-Ctr cipher after successfully decrypting
    /// the metadata key ciphertext. May be used to decrypt the remainder
    /// of the section ciphertext.
    cipher: C::AesCtr128,
}

/// Maximum length of a section's contents, after the metadata-key.
#[allow(unused)]
const MAX_SECTION_CONTENTS_LEN: usize = NP_ADV_MAX_SECTION_LEN - V1_IDENTITY_TOKEN_LEN;

/// Bare, decrypted contents from an encrypted section,
/// including the decrypted metadata key and the decrypted section ciphertext.
/// At this point, verification of the plaintext contents has not yet been performed.
pub(crate) struct RawDecryptedSection<'a> {
    // Only used with feature = "devtools"
    #[allow(unused)]
    pub(crate) identity_token: V1IdentityToken,
    pub(crate) nonce: AesCtrNonce,
    pub(crate) plaintext_contents: &'a [u8],
}

#[cfg(feature = "devtools")]
impl<'a> RawDecryptedSection<'a> {
    pub(crate) fn to_raw_bytes(&self) -> ArrayView<u8, NP_ADV_MAX_SECTION_LEN> {
        let mut result = Vec::new();
        result.extend_from_slice(self.identity_token.as_slice());
        result.extend_from_slice(self.plaintext_contents);
        ArrayView::try_from_slice(&result).expect("Won't panic because of the involved lengths")
    }
}

/// Represents the contents of an encrypted section,
/// independent of the encryption type.
#[derive(PartialEq, Eq, Debug)]
pub(crate) struct EncryptedSectionContents<'adv, S> {
    adv_header: V1AdvHeader,
    format_bytes: &'adv [u8],
    pub(crate) salt: S,
    /// Ciphertext of identity token (part of section header)
    identity_token: CiphertextExtendedIdentityToken,
    /// The portion of the ciphertext that has been encrypted.
    /// Length must be in `[0, NP_ADV_MAX_SECTION_LEN]`.
    section_contents: &'adv [u8],
    // The length byte exactly as it appears in the adv. This is the length of the encrypted
    // contents plus any additional bytes of suffix
    total_section_contents_len: u8,
}

impl<'adv, S: V1Salt> EncryptedSectionContents<'adv, S> {
    /// Constructs a representation of the contents of an encrypted V1 section
    /// from the advertisement header, the section header, information about
    /// the encryption used for identity verification, identity metadata,
    /// and the entire section contents as an undecrypted ciphertext.
    ///
    /// # Panics
    /// If `all_ciphertext` is greater than `NP_ADV_MAX_SECTION_LEN` bytes,
    /// or less than `IDENTITY_TOKEN_LEN` bytes.
    pub(crate) fn new(
        adv_header: V1AdvHeader,
        format_bytes: &'adv [u8],
        salt: S,
        identity_token: CiphertextExtendedIdentityToken,
        section_contents_len: u8,
        section_contents: &'adv [u8],
    ) -> Self {
        assert!(section_contents.len() <= NP_ADV_MAX_SECTION_LEN - V1_IDENTITY_TOKEN_LEN);
        Self {
            adv_header,
            format_bytes,
            salt,
            identity_token,
            total_section_contents_len: section_contents_len,
            section_contents,
        }
    }

    /// Gets the salt for this encrypted section
    pub(crate) fn salt(&self) -> MultiSalt {
        self.salt.into()
    }

    /// Constructs some cryptographic contents for section identity-resolution
    /// out of the entire contents of this encrypted section.
    pub(crate) fn compute_identity_resolution_contents<C: CryptoProvider>(
        &self,
    ) -> SectionIdentityResolutionContents {
        let nonce = self.salt.compute_nonce::<C>();
        SectionIdentityResolutionContents { nonce, identity_token: self.identity_token }
    }

    /// Given an identity-match, decrypts the ciphertext in this encrypted section
    /// and returns the raw bytes of the decrypted plaintext.
    pub(crate) fn decrypt_ciphertext<C: CryptoProvider>(
        &self,
        arena: &mut DeserializationArenaAllocator<'adv>,
        mut identity_match: IdentityMatch<C>,
    ) -> Result<RawDecryptedSection<'adv>, ArenaOutOfSpace> {
        // Fill decrypt_buf with the ciphertext after the section length
        let decrypt_buf =
            arena
                .allocate(u8::try_from(self.section_contents.len()).expect(
                    "section_contents.len() must be in [0, NP_ADV_MAX_SECTION_CONTENTS_LEN - EXTENDED_IDENTITY_TOKEN_LEN]",
                ))?;
        decrypt_buf.copy_from_slice(self.section_contents);

        // Decrypt everything after the metadata key
        identity_match.cipher.apply_keystream(decrypt_buf);

        Ok(RawDecryptedSection {
            identity_token: identity_match.identity_token,
            nonce: identity_match.nonce,
            plaintext_contents: decrypt_buf,
        })
    }

    /// Try decrypting into some raw bytes given some raw identity-resolution material.
    #[cfg(feature = "devtools")]
    pub(crate) fn try_resolve_identity_and_decrypt<P: CryptoProvider>(
        &self,
        allocator: &mut DeserializationArenaAllocator<'adv>,
        identity_resolution_material: &SectionIdentityResolutionMaterial,
    ) -> Option<Result<ArrayView<u8, NP_ADV_MAX_SECTION_LEN>, ArenaOutOfSpace>> {
        self.compute_identity_resolution_contents::<P>()
            .try_match(identity_resolution_material)
            .map(|identity_match| {
                Ok(self.decrypt_ciphertext::<P>(allocator, identity_match)?.to_raw_bytes())
            })
    }
}

/// An encrypted section which is verified using a ed25519 signature
#[derive(PartialEq, Eq, Debug)]
pub(crate) struct SignatureEncryptedSection<'a> {
    pub(crate) contents: EncryptedSectionContents<'a, ExtendedV1Salt>,
}

impl<'a> SignatureEncryptedSection<'a> {
    /// Try deserializing into a [`DecryptedSection`] given an identity-match
    /// with some paired verification material for the matched identity.
    pub(crate) fn try_deserialize<P>(
        &self,
        arena: &mut DeserializationArenaAllocator<'a>,
        identity_match: IdentityMatch<P>,
        verification_material: &SignedSectionVerificationMaterial,
    ) -> Result<DecryptedSection<'a>, DeserializationError<SignatureVerificationError>>
    where
        P: CryptoProvider,
    {
        let identity_token = identity_match.identity_token;
        let raw_decrypted = self.contents.decrypt_ciphertext(arena, identity_match)?;
        let nonce = raw_decrypted.nonce;
        let remaining = raw_decrypted.plaintext_contents;

        let (plaintext_des, sig) = remaining
            .split_last_chunk::<{ crypto_provider::ed25519::SIGNATURE_LENGTH }>()
            .ok_or(SignatureVerificationError::SignatureMissing)?;

        let expected_signature = crypto_provider::ed25519::Signature::from(*sig);

        let section_signature_payload = SectionSignaturePayload::new(
            self.contents.format_bytes,
            self.contents.salt.bytes(),
            &nonce,
            identity_token.as_slice(),
            self.contents.total_section_contents_len,
            plaintext_des,
        );

        let public_key = verification_material.signature_verification_public_key();

        section_signature_payload.verify::<P::Ed25519>(expected_signature, &public_key).map_err(
            |e| {
                // Length of the payload should fit in the signature verification buffer.
                debug_assert!(e != np_ed25519::SignatureVerificationError::PayloadTooBig);
                SignatureVerificationError::SignatureMismatch
            },
        )?;

        Ok(DecryptedSection::new(
            VerificationMode::Signature,
            self.contents.salt(),
            identity_token,
            plaintext_des,
        ))
    }

    /// Try decrypting into some raw bytes given some raw signed crypto-material.
    #[cfg(feature = "devtools")]
    pub(crate) fn try_resolve_identity_and_decrypt<P: CryptoProvider>(
        &self,
        allocator: &mut DeserializationArenaAllocator<'a>,
        identity_resolution_material: &SignedSectionIdentityResolutionMaterial,
    ) -> Option<Result<ArrayView<u8, NP_ADV_MAX_SECTION_LEN>, ArenaOutOfSpace>> {
        self.contents.try_resolve_identity_and_decrypt::<P>(
            allocator,
            identity_resolution_material.as_raw_resolution_material(),
        )
    }

    /// Try deserializing into a [Section] given some raw signed crypto-material.
    ///
    /// A less-efficient, one-shot way of getting
    /// [EncryptedSectionContents::compute_identity_resolution_contents] and then attempting
    /// deserialization.
    ///
    /// Normally, id resolution contents would be calculated for a bunch of sections, and then have
    /// many identities tried on them. This just works for one identity.
    #[cfg(test)]
    pub(crate) fn try_resolve_identity_and_deserialize<P: CryptoProvider>(
        &self,
        allocator: &mut DeserializationArenaAllocator<'a>,
        identity_resolution_material: &SignedSectionIdentityResolutionMaterial,
        verification_material: &SignedSectionVerificationMaterial,
    ) -> Result<
        DecryptedSection,
        IdentityResolutionOrDeserializationError<SignatureVerificationError>,
    > {
        match self
            .contents
            .compute_identity_resolution_contents::<P>()
            .try_match::<P>(identity_resolution_material.as_raw_resolution_material())
        {
            Some(identity_match) => self
                .try_deserialize(allocator, identity_match, verification_material)
                .map_err(|e| e.into()),
            None => Err(IdentityResolutionOrDeserializationError::IdentityMatchingError),
        }
    }
}

/// An error when attempting to deserialize an encrypted advertisement,
/// assuming that we already have an identity-match.
///
/// This should not be exposed publicly, since it's too
/// detailed.
#[derive(Debug, PartialEq, Eq)]
pub(crate) enum DeserializationError<V: VerificationError> {
    /// Verification failed
    VerificationError(V),
    /// The given arena ran out of space
    ArenaOutOfSpace,
}

impl<V: VerificationError> From<ArenaOutOfSpace> for DeserializationError<V> {
    fn from(_: ArenaOutOfSpace) -> Self {
        Self::ArenaOutOfSpace
    }
}

impl<V: VerificationError> From<V> for DeserializationError<V> {
    fn from(verification_error: V) -> Self {
        Self::VerificationError(verification_error)
    }
}

/// Common trait bound for errors which arise during
/// verification of encrypted section contents.
///
/// Implementors should not be exposed publicly, since it's too
/// detailed.
pub(crate) trait VerificationError: Debug + PartialEq + Eq {}

/// An error when attempting to verify a signature
#[derive(Debug, PartialEq, Eq)]
pub(crate) enum SignatureVerificationError {
    /// The provided signature did not match the calculated signature
    SignatureMismatch,
    /// The provided signature is missing
    SignatureMissing,
}

impl VerificationError for SignatureVerificationError {}

/// An encrypted section whose contents are verified to match a message integrity code (MIC)
#[derive(PartialEq, Eq, Debug)]
pub(crate) struct MicEncryptedSection<'a> {
    pub(crate) contents: EncryptedSectionContents<'a, MultiSalt>,
    pub(crate) mic: SectionMic,
}

impl<'a> MicEncryptedSection<'a> {
    /// Try deserializing into a [`DecryptedSection`].
    ///
    /// Returns an error if the credential is incorrect or if the section data is malformed.
    pub(crate) fn try_deserialize<P>(
        &self,
        allocator: &mut DeserializationArenaAllocator<'a>,
        identity_match: IdentityMatch<P>,
        crypto_material: &impl V1DiscoveryCryptoMaterial,
    ) -> Result<DecryptedSection<'a>, DeserializationError<MicVerificationError>>
    where
        P: CryptoProvider,
    {
        let hmac_key = match self.contents.salt {
            MultiSalt::Short(_) => {
                crypto_material.mic_short_salt_verification_material::<P>().mic_hmac_key()
            }
            MultiSalt::Extended(_) => {
                crypto_material.mic_extended_salt_verification_material::<P>().mic_hmac_key()
            }
        };

        let mut mic_hmac = hmac_key.build_hmac::<P>();
        // if mic is ok, the section was generated by someone holding at least the shared credential
        mic_hmac.update(&NP_SVC_UUID);
        mic_hmac.update(&[self.contents.adv_header.contents()]);
        // section format
        mic_hmac.update(self.contents.format_bytes);
        // salt bytes
        mic_hmac.update(self.contents.salt.as_slice());
        // nonce
        mic_hmac.update(identity_match.nonce.as_slice());
        // ciphertext identity token
        mic_hmac.update(self.contents.identity_token.0.as_slice());
        // section payload len
        mic_hmac.update(&[self.contents.total_section_contents_len]);
        // rest of encrypted contents
        mic_hmac.update(self.contents.section_contents);
        mic_hmac
            // adv only contains first 16 bytes of HMAC
            .verify_truncated_left(&self.mic.mic)
            .map_err(|_e| MicVerificationError::MicMismatch)?;

        // plaintext identity token, already decrypted during identity match
        let identity_token = identity_match.identity_token;
        let raw_decrypted = self.contents.decrypt_ciphertext(allocator, identity_match)?;
        Ok(DecryptedSection::new(
            VerificationMode::Mic,
            self.contents.salt(),
            identity_token,
            raw_decrypted.plaintext_contents,
        ))
    }

    /// Try decrypting into some raw bytes given some raw unsigned crypto-material.
    #[cfg(feature = "devtools")]
    pub(crate) fn try_resolve_short_salt_identity_and_decrypt<P: CryptoProvider>(
        &self,
        allocator: &mut DeserializationArenaAllocator<'a>,
        identity_resolution_material: &MicShortSaltSectionIdentityResolutionMaterial,
    ) -> Option<Result<ArrayView<u8, NP_ADV_MAX_SECTION_LEN>, ArenaOutOfSpace>> {
        self.contents.try_resolve_identity_and_decrypt::<P>(
            allocator,
            identity_resolution_material.as_raw_resolution_material(),
        )
    }

    /// Try decrypting into some raw bytes given some raw unsigned crypto-material.
    #[cfg(feature = "devtools")]
    pub(crate) fn try_resolve_extended_salt_identity_and_decrypt<P: CryptoProvider>(
        &self,
        allocator: &mut DeserializationArenaAllocator<'a>,
        identity_resolution_material: &MicExtendedSaltSectionIdentityResolutionMaterial,
    ) -> Option<Result<ArrayView<u8, NP_ADV_MAX_SECTION_LEN>, ArenaOutOfSpace>> {
        self.contents.try_resolve_identity_and_decrypt::<P>(
            allocator,
            identity_resolution_material.as_raw_resolution_material(),
        )
    }

    /// Try deserializing into a [Section] given some raw unsigned crypto-material.
    #[cfg(test)]
    pub(crate) fn try_resolve_identity_and_deserialize<P: CryptoProvider>(
        &self,
        allocator: &mut DeserializationArenaAllocator<'a>,
        crypto_material: &impl V1DiscoveryCryptoMaterial,
    ) -> Result<DecryptedSection, IdentityResolutionOrDeserializationError<MicVerificationError>>
    {
        let section_identity_resolution_contents =
            self.contents.compute_identity_resolution_contents::<P>();

        let identity_match = match self.contents.salt {
            MultiSalt::Short(_) => section_identity_resolution_contents.try_match::<P>(
                crypto_material
                    .mic_short_salt_identity_resolution_material::<P>()
                    .as_raw_resolution_material(),
            ),
            MultiSalt::Extended(_) => section_identity_resolution_contents.try_match::<P>(
                crypto_material
                    .mic_extended_salt_identity_resolution_material::<P>()
                    .as_raw_resolution_material(),
            ),
        }
        .ok_or(IdentityResolutionOrDeserializationError::IdentityMatchingError)?;

        self.try_deserialize(allocator, identity_match, crypto_material).map_err(|e| e.into())
    }
}

#[derive(Debug, PartialEq, Eq)]
pub(crate) enum MicVerificationError {
    /// Calculated MIC did not match MIC from section
    MicMismatch,
}

impl VerificationError for MicVerificationError {}
