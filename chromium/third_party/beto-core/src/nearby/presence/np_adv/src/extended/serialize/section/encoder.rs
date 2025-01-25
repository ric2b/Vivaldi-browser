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

use crate::credential::v1::V1BroadcastCredential;
use crate::extended::salt::{MultiSalt, ShortV1Salt, V1Salt};
use crate::header::VERSION_HEADER_V1;
use crate::{
    extended::{
        deserialize::SectionMic,
        section_signature_payload::SectionSignaturePayload,
        serialize::{section::header::SectionHeader, AdvertisementType, DeSalt},
        V1IdentityToken, V1_IDENTITY_TOKEN_LEN,
    },
    NP_SVC_UUID,
};
use crypto_provider::{
    aes,
    aes::ctr::{AesCtr as _, AesCtrNonce, NonceAndCounter},
    ed25519,
    hmac::Hmac,
    CryptoProvider, CryptoRng as _,
};
use np_hkdf::v1_salt::EXTENDED_SALT_LEN;
use np_hkdf::{
    v1_salt::{DataElementOffset, ExtendedV1Salt},
    DerivedSectionKeys,
};

/// Everything needed to properly encode a section
pub trait SectionEncoder {
    /// How much space needs to be reserved after the DEs
    const SUFFIX_LEN: usize;

    /// The advertisement type that can support this section
    const ADVERTISEMENT_TYPE: AdvertisementType;

    /// The type of derived salt produced for a DE sharing a section with this identity.
    type DerivedSalt;

    /// Header to write at the start of the section contents
    fn header(&self) -> SectionHeader;

    /// Postprocess the contents of the section (the data after the section header byte), which will
    /// start with the contents of [Self::header()], and similarly end with
    /// [Self::SUFFIX_LEN] bytes, with DEs (if any) in the middle.
    ///
    /// `section_header_without_length` is the bytes of the section header up until but not
    /// including the length byte
    /// `section_len` is the length of the contents that come after the length byte which includes
    /// the de contents + the suffix. This is the length of `remaining_content_bytes` stored as an u8
    /// `remaining_content_bytes` are the bytes of the rest of the contents that come after section length
    fn postprocess<C: CryptoProvider>(
        &mut self,
        section_header_without_length: &mut [u8],
        section_len: u8,
        remaining_content_bytes: &mut [u8],
    );

    /// Produce a `Self::Output` salt for a DE.
    fn de_salt(&self, de_offset: DataElementOffset) -> Self::DerivedSalt;
}

/// Encoder for plaintext data elements
#[derive(Debug)]
pub struct UnencryptedSectionEncoder;

impl UnencryptedSectionEncoder {}

impl SectionEncoder for UnencryptedSectionEncoder {
    const SUFFIX_LEN: usize = 0;
    const ADVERTISEMENT_TYPE: AdvertisementType = AdvertisementType::Plaintext;

    type DerivedSalt = ();

    fn header(&self) -> SectionHeader {
        SectionHeader::unencrypted()
    }
    fn postprocess<C: CryptoProvider>(
        &mut self,
        _section_header_without_length: &mut [u8],
        _section_len: u8,
        _remaining_content_bytes: &mut [u8],
    ) {
        // no op
    }

    fn de_salt(&self, _de_offset: DataElementOffset) -> Self::DerivedSalt {}
}

/// Encrypts the data elements and protects integrity with an np_ed25519 signature
/// using key material derived from an NP identity.
pub struct SignedEncryptedSectionEncoder {
    pub(crate) salt: ExtendedV1Salt,
    identity_token: V1IdentityToken,
    private_key: ed25519::PrivateKey,
    aes_key: aes::Aes128Key,
}

impl SignedEncryptedSectionEncoder {
    /// Build a [SignedEncryptedSectionEncoder] from an identity type,
    /// some broadcast crypto-material, and with a random salt.
    pub fn new_random_salt<C: CryptoProvider>(
        rng: &mut C::CryptoRng,
        crypto_material: &V1BroadcastCredential,
    ) -> Self {
        let salt: ExtendedV1Salt = rng.gen::<[u8; 16]>().into();
        Self::new::<C>(salt, crypto_material)
    }

    /// Build a [SignedEncryptedSectionEncoder] from an identity type,
    /// a provided salt, and some broadcast crypto-material.
    pub(crate) fn new<C: CryptoProvider>(
        salt: ExtendedV1Salt,
        crypto_material: &V1BroadcastCredential,
    ) -> Self {
        let identity_token = crypto_material.identity_token();
        let key_seed = crypto_material.key_seed();
        let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&key_seed);
        let private_key = crypto_material.signing_key();
        let aes_key = key_seed_hkdf.v1_signature_keys().aes_key();
        Self { salt, identity_token, private_key, aes_key }
    }
}

impl SectionEncoder for SignedEncryptedSectionEncoder {
    /// Ed25519 signature
    const SUFFIX_LEN: usize = crypto_provider::ed25519::SIGNATURE_LENGTH;
    const ADVERTISEMENT_TYPE: AdvertisementType = AdvertisementType::Encrypted;

    type DerivedSalt = DeSalt;

    fn header(&self) -> SectionHeader {
        SectionHeader::encrypted_signature_extended_salt(&self.salt, &self.identity_token)
    }

    fn postprocess<C: CryptoProvider>(
        &mut self,
        section_header_without_length: &mut [u8],
        section_len: u8,
        remaining_contents: &mut [u8],
    ) {
        let nonce: AesCtrNonce = self.salt.compute_nonce::<C>();
        let mut cipher = C::AesCtr128::new(&self.aes_key, NonceAndCounter::from_nonce(nonce));

        let start_of_identity_token = section_header_without_length.len() - V1_IDENTITY_TOKEN_LEN;
        let (format_and_salt_bytes, identity_token) =
            section_header_without_length.split_at_mut(start_of_identity_token);
        debug_assert!(identity_token.len() == V1_IDENTITY_TOKEN_LEN);

        let start_of_salt = format_and_salt_bytes.len() - EXTENDED_SALT_LEN;
        let (format_bytes, salt_bytes) = format_and_salt_bytes.split_at_mut(start_of_salt);
        debug_assert!(salt_bytes.len() == EXTENDED_SALT_LEN);
        debug_assert!((1..=2).contains(&format_bytes.len()));

        let start_of_signature = remaining_contents.len() - Self::SUFFIX_LEN;
        let (plaintext_data_elements, sig) = remaining_contents.split_at_mut(start_of_signature);
        debug_assert!(sig.len() == Self::SUFFIX_LEN);

        let section_signature_payload = SectionSignaturePayload::new(
            format_bytes,
            salt_bytes,
            &nonce,
            identity_token,
            section_len,
            plaintext_data_elements,
        );

        sig.copy_from_slice(
            &section_signature_payload.sign::<C::Ed25519>(&self.private_key).to_bytes(),
        );

        cipher.apply_keystream(identity_token);
        cipher.apply_keystream(plaintext_data_elements);
        cipher.apply_keystream(sig);
    }

    fn de_salt(&self, de_offset: DataElementOffset) -> Self::DerivedSalt {
        DeSalt { salt: self.salt, de_offset }
    }
}

/// Encrypts the data elements and protects integrity with a MIC using key material derived from
/// an NP identity.
pub struct MicEncryptedSectionEncoder<S> {
    pub(crate) salt: S,
    identity_token: V1IdentityToken,
    aes_key: aes::Aes128Key,
    mic_hmac_key: np_hkdf::NpHmacSha256Key,
}

impl<S: MicSectionEncoderSalt> MicEncryptedSectionEncoder<S> {
    /// Build a [MicEncryptedSectionEncoder] from the provided identity info.
    pub(crate) fn new<C: CryptoProvider>(
        salt: S,
        broadcast_credential: &V1BroadcastCredential,
    ) -> Self {
        let identity_token = broadcast_credential.identity_token();
        let key_seed = broadcast_credential.key_seed();
        let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&key_seed);
        let aes_key = salt.derive_aes_key(&key_seed_hkdf);
        let mic_hmac_key = salt.derive_mic_hmac_key(&key_seed_hkdf);

        Self { salt, identity_token, aes_key, mic_hmac_key }
    }

    /// Build a [MicEncryptedSectionEncoder] from the provided identity info.
    /// Exposed outside of this crate for testing purposes only, since this
    /// does not handle the generation of random salts.
    #[cfg(any(test, feature = "testing", feature = "devtools"))]
    pub fn new_with_salt<C: CryptoProvider>(
        salt: S,
        broadcast_credential: &V1BroadcastCredential,
    ) -> Self {
        Self::new::<C>(salt, broadcast_credential)
    }
}

impl MicEncryptedSectionEncoder<ExtendedV1Salt> {
    /// Build a [MicEncryptedSectionEncoder] from the provided identity
    /// info with a random extended salt.
    pub fn new_random_salt<C: CryptoProvider>(
        rng: &mut C::CryptoRng,
        broadcast_credential: &V1BroadcastCredential,
    ) -> Self {
        Self::new::<C>(rng.gen(), broadcast_credential)
    }
}

impl MicEncryptedSectionEncoder<MultiSalt> {
    /// Build a [MicEncryptedSectionEncoder] from the provided identity
    /// info with a random extended salt wrapped in [MultiSalt].
    ///
    /// Prefer [Self::new_random_salt] unless there is a need for the type
    /// parameter to be [MultiSalt].
    pub fn new_wrapped_salt<C: CryptoProvider>(
        rng: &mut C::CryptoRng,
        broadcast_credential: &V1BroadcastCredential,
    ) -> Self {
        Self::new::<C>(rng.gen::<ExtendedV1Salt>().into(), broadcast_credential)
    }
}

impl<S: MicSectionEncoderSalt> SectionEncoder for MicEncryptedSectionEncoder<S> {
    /// Length of mic
    const SUFFIX_LEN: usize = SectionMic::CONTENTS_LEN;

    const ADVERTISEMENT_TYPE: AdvertisementType = AdvertisementType::Encrypted;

    type DerivedSalt = S::DerivedSalt;

    fn header(&self) -> SectionHeader {
        self.salt.header(&self.identity_token)
    }

    fn postprocess<C: CryptoProvider>(
        &mut self,
        section_header_without_length: &mut [u8],
        section_len: u8,
        remaining_contents: &mut [u8],
    ) {
        let nonce: AesCtrNonce = self.salt.compute_nonce::<C>();
        let mut cipher = C::AesCtr128::new(&self.aes_key, NonceAndCounter::from_nonce(nonce));

        let start_of_identity_token = section_header_without_length.len() - V1_IDENTITY_TOKEN_LEN;
        let (format_and_salt_bytes, identity_token) =
            section_header_without_length.split_at_mut(start_of_identity_token);
        debug_assert!(identity_token.len() == V1_IDENTITY_TOKEN_LEN);
        // format can be 1-2 bytes
        debug_assert!((1 + self.salt.into().as_slice().len()
            ..=2 + self.salt.into().as_slice().len())
            .contains(&format_and_salt_bytes.len()));

        let start_of_mic = remaining_contents.len() - Self::SUFFIX_LEN;
        let (data_element_contents, mic) = remaining_contents.split_at_mut(start_of_mic);
        debug_assert!(mic.len() == Self::SUFFIX_LEN);

        // First encrypt the identity token
        cipher.apply_keystream(identity_token);

        // Now encrypt the rest of the ciphertext bytes that come after the section length byte
        cipher.apply_keystream(data_element_contents);

        // calculate MAC per the spec
        let mut section_hmac = self.mic_hmac_key.build_hmac::<C>();
        // svc uuid
        section_hmac.update(NP_SVC_UUID.as_slice());
        // adv header
        section_hmac.update(&[VERSION_HEADER_V1]);
        // section format and salt
        section_hmac.update(format_and_salt_bytes);
        // nonce
        section_hmac.update(&nonce);
        // encrypted identity token
        section_hmac.update(identity_token);
        // section len
        section_hmac.update(&[section_len]);
        // rest of the ciphertext
        section_hmac.update(data_element_contents);

        // write truncated MIC
        mic.copy_from_slice(&section_hmac.finalize()[..SectionMic::CONTENTS_LEN]);
    }

    fn de_salt(&self, de_offset: DataElementOffset) -> Self::DerivedSalt {
        self.salt.derive_de_salt(de_offset)
    }
}

/// Behavior for salts used with MIC sections.
pub trait MicSectionEncoderSalt: V1Salt {
    /// The type of derived data produced, or `()` if no data can be derived.
    type DerivedSalt;

    /// Build the appropriate header for the type of salt used
    fn header(&self, identity_token: &V1IdentityToken) -> SectionHeader;

    /// Derive a DE salt at the specified offset.
    fn derive_de_salt(&self, de_offset: DataElementOffset) -> Self::DerivedSalt;

    /// Derive the AES key  suitable for this salt type
    fn derive_aes_key<C: CryptoProvider>(&self, hkdf: &np_hkdf::NpKeySeedHkdf<C>)
        -> aes::Aes128Key;

    /// Derive the MIC HMAC key suitable for this salt type
    fn derive_mic_hmac_key<C: CryptoProvider>(
        &self,
        hkdf: &np_hkdf::NpKeySeedHkdf<C>,
    ) -> np_hkdf::NpHmacSha256Key;
}

impl MicSectionEncoderSalt for ExtendedV1Salt {
    type DerivedSalt = DeSalt;

    fn header(&self, identity_token: &V1IdentityToken) -> SectionHeader {
        SectionHeader::encrypted_mic_extended_salt(self, identity_token)
    }

    fn derive_de_salt(&self, de_offset: DataElementOffset) -> Self::DerivedSalt {
        DeSalt { salt: *self, de_offset }
    }

    fn derive_aes_key<C: CryptoProvider>(
        &self,
        hkdf: &np_hkdf::NpKeySeedHkdf<C>,
    ) -> aes::Aes128Key {
        hkdf.v1_mic_extended_salt_keys().aes_key()
    }

    fn derive_mic_hmac_key<C: CryptoProvider>(
        &self,
        hkdf: &np_hkdf::NpKeySeedHkdf<C>,
    ) -> np_hkdf::NpHmacSha256Key {
        hkdf.v1_mic_extended_salt_keys().mic_hmac_key()
    }
}

// TODO is this impl used?
impl MicSectionEncoderSalt for ShortV1Salt {
    type DerivedSalt = ();

    fn header(&self, identity_token: &V1IdentityToken) -> SectionHeader {
        SectionHeader::encrypted_mic_short_salt(*self, identity_token)
    }

    fn derive_de_salt(&self, _de_offset: DataElementOffset) -> Self::DerivedSalt {}

    fn derive_aes_key<C: CryptoProvider>(
        &self,
        hkdf: &np_hkdf::NpKeySeedHkdf<C>,
    ) -> aes::Aes128Key {
        hkdf.v1_mic_short_salt_keys().aes_key()
    }

    fn derive_mic_hmac_key<C: CryptoProvider>(
        &self,
        hkdf: &np_hkdf::NpKeySeedHkdf<C>,
    ) -> np_hkdf::NpHmacSha256Key {
        hkdf.v1_mic_short_salt_keys().mic_hmac_key()
    }
}

impl MicSectionEncoderSalt for MultiSalt {
    type DerivedSalt = Option<DeSalt>;

    fn header(&self, identity_token: &V1IdentityToken) -> SectionHeader {
        match self {
            MultiSalt::Short(s) => SectionHeader::encrypted_mic_short_salt(*s, identity_token),
            MultiSalt::Extended(s) => SectionHeader::encrypted_mic_extended_salt(s, identity_token),
        }
    }

    fn derive_de_salt(&self, de_offset: DataElementOffset) -> Self::DerivedSalt {
        match self {
            MultiSalt::Short(_) => None,
            MultiSalt::Extended(s) => Some(DeSalt { salt: *s, de_offset }),
        }
    }

    fn derive_aes_key<C: CryptoProvider>(
        &self,
        hkdf: &np_hkdf::NpKeySeedHkdf<C>,
    ) -> aes::Aes128Key {
        match self {
            MultiSalt::Short(_) => hkdf.v1_mic_short_salt_keys().aes_key(),
            MultiSalt::Extended(_) => hkdf.v1_mic_extended_salt_keys().aes_key(),
        }
    }

    fn derive_mic_hmac_key<C: CryptoProvider>(
        &self,
        hkdf: &np_hkdf::NpKeySeedHkdf<C>,
    ) -> np_hkdf::NpHmacSha256Key {
        match self {
            MultiSalt::Short(_) => hkdf.v1_mic_short_salt_keys().mic_hmac_key(),
            MultiSalt::Extended(_) => hkdf.v1_mic_extended_salt_keys().mic_hmac_key(),
        }
    }
}
