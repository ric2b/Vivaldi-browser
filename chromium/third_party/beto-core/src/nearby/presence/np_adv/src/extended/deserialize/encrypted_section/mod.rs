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
    deserialization_arena::DeserializationArenaAllocator,
    extended::{
        deserialize::{
            DecryptedSection, EncryptedIdentityMetadata, EncryptionInfo, SectionContents,
            SectionMic, VerificationMode,
        },
        section_signature_payload::*,
        METADATA_KEY_LEN, NP_ADV_MAX_SECTION_LEN,
    },
    MetadataKey, V1Header, NP_SVC_UUID,
};

#[cfg(any(feature = "devtools", test))]
extern crate alloc;
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
use np_hkdf::v1_salt::V1Salt;

use super::ArenaOutOfSpace;

#[cfg(test)]
mod mic_decrypt_tests;
#[cfg(test)]
mod signature_decrypt_tests;

/// Represents the contents of an encrypted section
/// which are directly employed in identity resolution.
/// This does not incorporate any information about credentials.
#[derive(PartialEq, Eq, Debug)]
pub(crate) struct SectionIdentityResolutionContents {
    /// The ciphertext for the metadata key
    pub(crate) metadata_key_ciphertext: [u8; METADATA_KEY_LEN],
    /// The 12-byte cryptographic nonce which is derived from the encryption info
    /// and the identity metadata for a particular section.
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
        let mut decrypt_buf = self.metadata_key_ciphertext;
        let aes_key = &identity_resolution_material.aes_key;
        let mut cipher = C::AesCtr128::new(aes_key, NonceAndCounter::from_nonce(self.nonce));
        cipher.apply_keystream(&mut decrypt_buf[..]);

        let metadata_key_hmac_key: np_hkdf::NpHmacSha256Key<C> =
            identity_resolution_material.metadata_key_hmac_key.into();
        let expected_metadata_key_hmac = identity_resolution_material.expected_metadata_key_hmac;
        metadata_key_hmac_key.verify_hmac(&decrypt_buf[..], expected_metadata_key_hmac).ok().map(
            move |_| IdentityMatch {
                cipher,
                metadata_key_plaintext: MetadataKey(decrypt_buf),
                nonce: self.nonce,
            },
        )
    }
}

/// Carries data about an identity "match" for a particular section
/// against some particular V1 identity-resolution crypto-materials.
pub(crate) struct IdentityMatch<C: CryptoProvider> {
    /// Decrypted metadata key ciphertext
    metadata_key_plaintext: MetadataKey,
    /// The AES-Ctr nonce to be used in section decryption and verification
    nonce: AesCtrNonce,
    /// The state of the AES-Ctr cipher after successfully decrypting
    /// the metadata key ciphertext. May be used to decrypt the remainder
    /// of the section ciphertext.
    cipher: C::AesCtr128,
}

/// Maximum length of a section's contents, after the metadata-key.
#[allow(unused)]
const MAX_SECTION_CONTENTS_LEN: usize = NP_ADV_MAX_SECTION_LEN - METADATA_KEY_LEN;

/// Bare, decrypted contents from an encrypted section,
/// including the decrypted metadata key and the decrypted section ciphertext.
/// At this point, verification of the plaintext contents has not yet been performed.
pub(crate) struct RawDecryptedSection<'adv> {
    pub(crate) metadata_key_plaintext: MetadataKey,
    pub(crate) nonce: AesCtrNonce,
    pub(crate) plaintext_contents: &'adv [u8],
}

#[cfg(feature = "devtools")]
impl<'adv> RawDecryptedSection<'adv> {
    pub(crate) fn to_raw_bytes(&self) -> ArrayView<u8, NP_ADV_MAX_SECTION_LEN> {
        let mut result = Vec::new();
        result.extend_from_slice(&self.metadata_key_plaintext.0);
        result.extend_from_slice(self.plaintext_contents);
        ArrayView::try_from_slice(&result).expect("Won't panic because of the involved lengths")
    }
}

/// Represents the contents of an encrypted section,
/// independent of the encryption type.
#[derive(PartialEq, Eq, Debug)]
pub(crate) struct EncryptedSectionContents<'a> {
    pub(crate) section_header: u8,
    pub(crate) adv_header: V1Header,
    pub(crate) encryption_info: EncryptionInfo,
    pub(crate) identity: EncryptedIdentityMetadata,
    /// All ciphertext (Contents of identity DE + all DEs)
    /// Length must be in `[METADATA_KEY_LEN, NP_ADV_MAX_SECTION_LEN]`.
    pub(crate) all_ciphertext: &'a [u8],
}

impl<'a> EncryptedSectionContents<'a> {
    /// Constructs a representation of the contents of an encrypted V1 section
    /// from the advertisement header, the section header, information about
    /// the encryption used for identity verification, identity metadata,
    /// and the entire section contents as an undecrypted ciphertext.
    ///
    /// # Panics
    /// If `all_ciphertext` is greater than `NP_ADV_MAX_SECTION_LEN` bytes,
    /// or less than `METADATA_KEY_LEN` bytes.
    pub(crate) fn new(
        adv_header: V1Header,
        section_header: u8,
        encryption_info: EncryptionInfo,
        identity: EncryptedIdentityMetadata,
        all_ciphertext: &'a [u8],
    ) -> Self {
        assert!(all_ciphertext.len() >= METADATA_KEY_LEN);
        assert!(all_ciphertext.len() <= NP_ADV_MAX_SECTION_LEN);
        Self { adv_header, section_header, encryption_info, identity, all_ciphertext }
    }

    /// Gets the salt for this encrypted section
    pub(crate) fn salt<C: CryptoProvider>(&self) -> V1Salt<C> {
        self.encryption_info.salt().into()
    }

    /// Constructs a cryptographic nonce for this encrypted section
    /// based on the contained salt.
    pub(crate) fn compute_nonce<C: CryptoProvider>(&self) -> AesCtrNonce {
        self.salt::<C>()
            .derive(Some(self.identity.offset))
            .expect("AES-CTR nonce is a valid HKDF size")
    }

    /// Constructs some cryptographic contents for section identity-resolution
    /// out of the entire contents of this encrypted section.
    pub(crate) fn compute_identity_resolution_contents<C: CryptoProvider>(
        &self,
    ) -> SectionIdentityResolutionContents {
        let nonce = self.compute_nonce::<C>();
        let metadata_key_ciphertext: [u8; METADATA_KEY_LEN] = self.all_ciphertext
            [..METADATA_KEY_LEN]
            .try_into()
            .expect("slice will always fit into same size array");

        SectionIdentityResolutionContents { nonce, metadata_key_ciphertext }
    }

    /// Given an identity-match, decrypts the ciphertext in this encrypted section
    /// and returns the raw bytes of the decrypted plaintext.
    pub(crate) fn decrypt_ciphertext<C: CryptoProvider>(
        &self,
        arena: &mut DeserializationArenaAllocator<'a>,
        mut identity_match: IdentityMatch<C>,
    ) -> Result<RawDecryptedSection<'a>, ArenaOutOfSpace> {
        // Fill decrypt_buf with the ciphertext after the metadata key
        let decrypt_buf =
            arena.allocate(u8::try_from(self.all_ciphertext.len() - METADATA_KEY_LEN).expect(
                "all_ciphertext.len() must be in [METADATA_KEY_LEN, NP_ADV_MAX_SECTION_LEN]",
            ))?;
        decrypt_buf.copy_from_slice(&self.all_ciphertext[METADATA_KEY_LEN..]);

        // Decrypt everything after the metadata key
        identity_match.cipher.apply_keystream(decrypt_buf);

        Ok(RawDecryptedSection {
            metadata_key_plaintext: identity_match.metadata_key_plaintext,
            nonce: identity_match.nonce,
            plaintext_contents: decrypt_buf,
        })
    }

    /// Try decrypting into some raw bytes given some raw identity-resolution material.
    #[cfg(feature = "devtools")]
    pub(crate) fn try_resolve_identity_and_decrypt<P: CryptoProvider>(
        &self,
        allocator: &mut DeserializationArenaAllocator<'a>,
        identity_resolution_material: &SectionIdentityResolutionMaterial,
    ) -> Option<Result<ArrayView<u8, NP_ADV_MAX_SECTION_LEN>, ArenaOutOfSpace>> {
        let identity_resolution_contents = self.compute_identity_resolution_contents::<P>();
        identity_resolution_contents.try_match(identity_resolution_material).map(|identity_match| {
            let decrypted_section = self.decrypt_ciphertext::<P>(allocator, identity_match)?;
            Ok(decrypted_section.to_raw_bytes())
        })
    }
}

/// An encrypted section which is verified using a ed25519 signature
#[derive(PartialEq, Eq, Debug)]
pub(crate) struct SignatureEncryptedSection<'a> {
    pub(crate) contents: EncryptedSectionContents<'a>,
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
        let raw_decrypted = self.contents.decrypt_ciphertext(arena, identity_match)?;
        let metadata_key = raw_decrypted.metadata_key_plaintext;
        let nonce = raw_decrypted.nonce;
        let remaining = raw_decrypted.plaintext_contents;

        if remaining.len() < crypto_provider::ed25519::SIGNATURE_LENGTH {
            return Err(SignatureVerificationError::SignatureMissing.into());
        }

        // should not panic due to above check
        let (non_identity_des, sig) =
            remaining.split_at(remaining.len() - crypto_provider::ed25519::SIGNATURE_LENGTH);

        // All implementations only check for 64 bytes, and this will always result in a 64 byte signature.
        let expected_signature =
            np_ed25519::Signature::<P>::try_from(sig).expect("Signature is always 64 bytes.");

        let section_signature_payload = SectionSignaturePayload::from_deserialized_parts(
            self.contents.adv_header.header_byte,
            self.contents.section_header,
            &self.contents.encryption_info.bytes,
            &nonce,
            self.contents.identity.header_bytes,
            metadata_key,
            non_identity_des,
        );

        let public_key = verification_material.signature_verification_public_key();

        section_signature_payload.verify(&expected_signature, &public_key).map_err(|e| {
            // Length of the payload should fit in the signature verification buffer.
            debug_assert!(e != np_ed25519::SignatureVerificationError::PayloadTooBig);

            SignatureVerificationError::SignatureMismatch
        })?;

        let salt = self.contents.salt::<P>();

        Ok(DecryptedSection::new(
            self.contents.identity.identity_type,
            VerificationMode::Signature,
            metadata_key,
            salt.into(),
            // de offset 2 because of leading encryption info and identity DEs
            SectionContents::new(self.contents.section_header, non_identity_des, 2),
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
        let section_identity_resolution_contents =
            self.contents.compute_identity_resolution_contents::<P>();
        match section_identity_resolution_contents
            .try_match::<P>(identity_resolution_material.as_raw_resolution_material())
        {
            Some(identity_match) => self
                .try_deserialize(allocator, identity_match, verification_material)
                .map_err(|e| e.into()),
            None => Err(IdentityResolutionOrDeserializationError::IdentityMatchingError),
        }
    }
}

/// An error when attempting to resolve an identity and then
/// attempt to deserialize an encrypted advertisement.
///
/// This should not be exposed publicly, since it's too
/// detailed.
#[cfg(test)]
#[derive(Debug, PartialEq, Eq)]
pub(crate) enum IdentityResolutionOrDeserializationError<V: VerificationError> {
    /// Failed to match the encrypted adv to an identity
    IdentityMatchingError,
    /// Failed to deserialize the encrypted adv after matching the identity
    DeserializationError(DeserializationError<V>),
}

#[cfg(test)]
impl<V: VerificationError> From<DeserializationError<V>>
    for IdentityResolutionOrDeserializationError<V>
{
    fn from(deserialization_error: DeserializationError<V>) -> Self {
        Self::DeserializationError(deserialization_error)
    }
}

#[cfg(test)]
impl<V: VerificationError> From<V> for IdentityResolutionOrDeserializationError<V> {
    fn from(verification_error: V) -> Self {
        Self::DeserializationError(DeserializationError::VerificationError(verification_error))
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
    pub(crate) contents: EncryptedSectionContents<'a>,
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
        verification_material: &UnsignedSectionVerificationMaterial,
    ) -> Result<DecryptedSection<'a>, DeserializationError<MicVerificationError>>
    where
        P: CryptoProvider,
    {
        let raw_decrypted = self.contents.decrypt_ciphertext(allocator, identity_match)?;
        let metadata_key = raw_decrypted.metadata_key_plaintext;
        let nonce = raw_decrypted.nonce;
        let remaining_des = raw_decrypted.plaintext_contents;

        // if mic is ok, the section was generated by someone holding at least the shared credential
        let mut mic_hmac = verification_material.mic_hmac_key::<P>().build_hmac();
        mic_hmac.update(&NP_SVC_UUID);
        mic_hmac.update(&[self.contents.adv_header.header_byte]);
        mic_hmac.update(&[self.contents.section_header]);
        mic_hmac.update(&self.contents.encryption_info.bytes);
        mic_hmac.update(&nonce);
        mic_hmac.update(&self.contents.identity.header_bytes);
        mic_hmac.update(self.contents.all_ciphertext);
        mic_hmac
            // adv only contains first 16 bytes of HMAC
            .verify_truncated_left(&self.mic.mic)
            .map_err(|_e| MicVerificationError::MicMismatch)?;

        let salt = self.contents.salt::<P>();

        Ok(DecryptedSection::new(
            self.contents.identity.identity_type,
            VerificationMode::Mic,
            metadata_key,
            salt.into(),
            // offset 2 for encryption info and identity DEs
            SectionContents::new(self.contents.section_header, remaining_des, 2),
        ))
    }

    /// Try decrypting into some raw bytes given some raw unsigned crypto-material.
    #[cfg(feature = "devtools")]
    pub(crate) fn try_resolve_identity_and_decrypt<P: CryptoProvider>(
        &self,
        allocator: &mut DeserializationArenaAllocator<'a>,
        identity_resolution_material: &UnsignedSectionIdentityResolutionMaterial,
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
        identity_resolution_material: &UnsignedSectionIdentityResolutionMaterial,
        verification_material: &UnsignedSectionVerificationMaterial,
    ) -> Result<DecryptedSection, IdentityResolutionOrDeserializationError<MicVerificationError>>
    {
        let section_identity_resolution_contents =
            self.contents.compute_identity_resolution_contents::<P>();
        match section_identity_resolution_contents
            .try_match::<P>(identity_resolution_material.as_raw_resolution_material())
        {
            Some(identity_match) => self
                .try_deserialize(allocator, identity_match, verification_material)
                .map_err(|e| e.into()),
            None => Err(IdentityResolutionOrDeserializationError::IdentityMatchingError),
        }
    }
}

#[derive(Debug, PartialEq, Eq)]
pub(crate) enum MicVerificationError {
    /// Calculated MIC did not match MIC from section
    MicMismatch,
}

impl VerificationError for MicVerificationError {}
