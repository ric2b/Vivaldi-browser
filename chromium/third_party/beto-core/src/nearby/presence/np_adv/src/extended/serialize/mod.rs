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

//! Serialization support for V1 advertisements.
//!
//! # Examples
//!
//! Serialize some DEs without an adv salt:
//!
//! ```
//! use np_adv::{
//!     extended::{data_elements::*, serialize::*, de_type::DeType },
//!     PublicIdentity
//! };
//! use np_adv::shared_data::TxPower;
//!
//! // no section identities or DEs need salt in this example
//! let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
//! let mut section_builder = adv_builder.section_builder(PublicSectionEncoder::default()).unwrap();
//!
//! section_builder.add_de(|_salt| TxPowerDataElement::from(TxPower::try_from(3).unwrap())).unwrap();
//!
//! // add some other DE with type = 1000
//! section_builder.add_de_res(|_salt|
//!     GenericDataElement::try_from( DeType::from(1000_u32), &[10, 11, 12, 13])
//! ).unwrap();
//!
//! section_builder.add_to_advertisement();
//!
//! assert_eq!(
//!     &[
//!         0x20, // adv header
//!         10, // section header
//!         0x03, // public identity
//!         0x15, 3, // tx power
//!         0x84, 0x87, 0x68, 10, 11, 12, 13, // other DE
//!     ],
//!     adv_builder.into_advertisement().as_slice()
//! );
//! ```
//!
//! Serialize some DEs in an adv with an encrypted section:
//!
//! ```
//! use np_adv::{
//!     credential::{SimpleBroadcastCryptoMaterial, v1::V1},
//!     de_type::EncryptedIdentityDataElementType,
//!     extended::{data_elements::*, serialize::*, de_type::DeType },
//!     MetadataKey,
//! };
//! use rand::{Rng as _, SeedableRng as _};
//! use crypto_provider::{CryptoProvider, CryptoRng};
//! use crypto_provider_default::CryptoProviderImpl;
//! use np_adv::shared_data::TxPower;
//!
//! let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
//!
//! // these would come from the credential//!
//! let mut rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();
//! let metadata_key: [u8; 16] = rng.gen();
//! let metadata_key = MetadataKey(metadata_key);
//! let key_seed: [u8; 32] = rng.gen();
//! // use your preferred crypto impl
//! let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
//!
//! let broadcast_cm = SimpleBroadcastCryptoMaterial::<V1>::new(
//!     key_seed,
//!     metadata_key,
//! );
//!
//! let mut section_builder = adv_builder.section_builder(MicEncryptedSectionEncoder::<CryptoProviderImpl>::new_random_salt(
//!     &mut rng,
//!     EncryptedIdentityDataElementType::Private,
//!     &broadcast_cm,
//! )).unwrap();
//!
//! section_builder.add_de(|_salt| TxPowerDataElement::from(TxPower::try_from(3).unwrap())).unwrap();
//!
//! // add some other DE with type = 1000
//! section_builder.add_de_res(|salt|
//!     GenericDataElement::try_from(
//!         DeType::from(1000_u32),
//!         &do_fancy_crypto(salt.derive::<16>().expect("16 is a valid HKDF length")))
//! ).unwrap();
//!
//! section_builder.add_to_advertisement();
//!
//! // can't assert much about this since most of it is random
//! assert_eq!(
//!     0x20, // adv header
//!     adv_builder.into_advertisement().as_slice()[0]
//! );
//!
//! // A hypothetical function that uses the per-DE derived salt to do something like encrypt or
//! // otherwise scramble data
//! fn do_fancy_crypto(derived_salt: [u8; 16]) -> [u8; 16] {
//!     // flipping bits is just a nonsense example, do something real here
//!     derived_salt.iter().map(|b| !b)
//!         .collect::<Vec<_>>()
//!         .try_into().expect("array sizes match")
//! }
//! ```
use crate::extended::{NP_V1_ADV_MAX_ENCRYPTED_SECTION_COUNT, NP_V1_ADV_MAX_PUBLIC_SECTION_COUNT};
use crate::{
    credential::{
        v1::{SignedBroadcastCryptoMaterial, V1},
        BroadcastCryptoMaterial,
    },
    de_type::{EncryptedIdentityDataElementType, IdentityDataElementType},
    extended::{
        data_elements::EncryptionInfoDataElement,
        de_type::{DeType, ExtendedDataElementType},
        deserialize::{EncryptedIdentityMetadata, EncryptionInfo, SectionMic},
        section_signature_payload::*,
        to_array_view, DeLength, BLE_ADV_SVC_CONTENT_LEN, NP_ADV_MAX_SECTION_LEN,
    },
    DeLengthOutOfRange, MetadataKey, NP_SVC_UUID,
};
use array_view::ArrayView;
use core::fmt::{self, Display};
use crypto_provider::{
    aes::{
        ctr::{AesCtr, AesCtrNonce, NonceAndCounter},
        Aes128Key,
    },
    hmac::Hmac,
    CryptoProvider, CryptoRng,
};
use np_hkdf::v1_salt;
use np_hkdf::v1_salt::{DataElementOffset, V1Salt};
use sink::Sink;

#[cfg(test)]
pub(crate) mod adv_tests;
#[cfg(test)]
mod de_header_tests;
#[cfg(test)]
pub(crate) mod section_tests;
#[cfg(test)]
mod test_vectors;

/// Builder for V1 advertisements.
#[derive(Debug)]
pub struct AdvBuilder {
    /// Contains the adv header byte
    adv: tinyvec::ArrayVec<[u8; BLE_ADV_SVC_CONTENT_LEN]>,
    /// To track the number of sections that are in the advertisement
    section_count: usize,
    /// Advertisement type: Public or Encrypted
    advertisement_type: AdvertisementType,
}

impl AdvBuilder {
    /// Build an [AdvBuilder].
    pub fn new(advertisement_type: AdvertisementType) -> Self {
        let mut adv = tinyvec::ArrayVec::new();
        // version 1, 0bVVVRRRRR
        adv.push(0b00100000);
        Self { adv, section_count: 0, advertisement_type }
    }

    /// Create a section builder.
    ///
    /// The builder will not accept more DEs than can fit given the space already used in the
    /// advertisement by previous sections, if any.
    ///
    /// Once the builder is populated, add it to the originating advertisement with
    /// [SectionBuilder.add_to_advertisement].
    pub fn section_builder<SE: SectionEncoder>(
        &mut self,
        section_encoder: SE,
    ) -> Result<SectionBuilder<SE>, AddSectionError> {
        // section header and identity prefix
        let prefix_len = 1 + SE::PREFIX_LEN;
        let minimum_section_len = prefix_len + SE::SUFFIX_LEN;
        // the max overall len available to the section
        let available_len = self.adv.capacity() - self.adv.len();

        if available_len < minimum_section_len {
            return Err(AddSectionError::InsufficientAdvSpace);
        }

        if self.section_count >= self.advertisement_type.max_sections() {
            return Err(AddSectionError::MaxSectionCountExceeded);
        }

        if self.advertisement_type != SE::ADVERTISEMENT_TYPE {
            return Err(AddSectionError::IncompatibleSectionType);
        }

        let mut section: tinyvec::ArrayVec<[u8; 249]> = tinyvec::ArrayVec::new();
        // placeholder for section header and identity prefix
        section.resize(prefix_len, 0);

        Ok(SectionBuilder {
            section: CapacityLimitedVec {
                vec: section,
                // won't underflow: checked above
                capacity: available_len - SE::SUFFIX_LEN,
            },
            section_encoder,
            adv_builder: self,
            next_de_offset: SE::INITIAL_DE_OFFSET,
        })
    }

    /// Convert the builder into an encoded advertisement.
    pub fn into_advertisement(self) -> EncodedAdvertisement {
        EncodedAdvertisement { adv: to_array_view(self.adv) }
    }

    /// Add the section, which must have come from a SectionBuilder generated from this, into this
    /// advertisement.
    fn add_section(&mut self, section: EncodedSection) {
        self.adv
            .try_extend_from_slice(section.as_slice())
            .expect("section capacity enforced in the section builder");
        self.section_count += 1;
    }

    fn header_byte(&self) -> u8 {
        self.adv[0]
    }
}

/// Errors that can occur when adding a section to an advertisement
#[derive(Debug, PartialEq, Eq)]
pub enum AddSectionError {
    /// The advertisement doesn't have enough space to hold the minimum size of the section
    InsufficientAdvSpace,
    /// The advertisement can only hold a maximum of NP_V1_ADV_MAX_ENCRYPTED_SECTION_COUNT number of sections
    MaxSectionCountExceeded,
    /// An incompatible section trying to be added
    IncompatibleSectionType,
}

impl Display for AddSectionError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            AddSectionError::InsufficientAdvSpace => {
                write!(f, "The advertisement (max {BLE_ADV_SVC_CONTENT_LEN} bytes) doesn't have enough remaining space to hold the section")
            }
            AddSectionError::MaxSectionCountExceeded => {
                write!(f, "The advertisement can only hold a maximum of {NP_V1_ADV_MAX_ENCRYPTED_SECTION_COUNT} number of sections")
            }
            AddSectionError::IncompatibleSectionType => {
                write!(f, "Public and Encrypted sections cannot be mixed in the same advertisement")
            }
        }
    }
}

/// An encoded NP V1 advertisement, starting with the NP advertisement header byte.
#[derive(Debug, PartialEq, Eq)]
pub struct EncodedAdvertisement {
    adv: ArrayView<u8, BLE_ADV_SVC_CONTENT_LEN>,
}

impl EncodedAdvertisement {
    /// Returns the advertisement as a slice.
    pub fn as_slice(&self) -> &[u8] {
        self.adv.as_slice()
    }
}

/// The encoded form of an advertisement section
type EncodedSection = ArrayView<u8, NP_ADV_MAX_SECTION_LEN>;

/// Accumulates data elements and encodes them into a section.
#[derive(Debug)]
pub struct SectionBuilder<'a, SE: SectionEncoder> {
    /// Contains the section header, the identity-specified overhead, and any DEs added
    pub(crate) section: CapacityLimitedVec<u8, NP_ADV_MAX_SECTION_LEN>,
    section_encoder: SE,
    /// mut ref to enforce only one active section builder at a time
    adv_builder: &'a mut AdvBuilder,
    next_de_offset: DataElementOffset,
}

impl<'a, SE: SectionEncoder> SectionBuilder<'a, SE> {
    /// Add this builder to the advertisement that created it.
    pub fn add_to_advertisement(self) {
        let adv_builder = self.adv_builder;
        adv_builder.add_section(Self::build_section(
            self.section.into_inner(),
            self.section_encoder,
            adv_builder,
        ))
    }

    /// Add a data element to the section with a closure that returns a `Result`.
    ///
    /// The provided `build_de` closure will be invoked with the derived salt for this DE.
    pub fn add_de_res<W: WriteDataElement, E, F: FnOnce(SE::DerivedSalt) -> Result<W, E>>(
        &mut self,
        build_de: F,
    ) -> Result<(), AddDataElementError<E>> {
        let writer = build_de(self.section_encoder.de_salt(self.next_de_offset))
            .map_err(AddDataElementError::BuildDeError)?;

        let orig_len = self.section.len();
        // since we own the writer, and it's immutable, no race risk writing header w/ len then
        // the contents as long as it's not simply an incorrect impl
        let de_header = writer.de_header();
        let content_len = self
            .section
            .try_extend_from_slice(de_header.serialize().as_slice())
            .ok_or(AddDataElementError::InsufficientSectionSpace)
            .and_then(|_| {
                let after_header_len = self.section.len();
                writer
                    .write_de_contents(&mut self.section)
                    .ok_or(AddDataElementError::InsufficientSectionSpace)
                    .map(|_| self.section.len() - after_header_len)
            })
            .map_err(|e| {
                // if anything went wrong, truncate any partial writes (e.g. just the header)
                self.section.truncate(orig_len);
                e
            })?;

        if content_len != usize::from(de_header.len.as_u8()) {
            panic!(
                "Buggy WriteDataElement impl: header len {}, actual written len {}",
                de_header.len.as_u8(),
                content_len
            );
        }

        self.next_de_offset = self.next_de_offset.incremented();

        Ok(())
    }

    /// Add a data element to the section with a closure that returns the data element directly.
    ///
    /// The provided `build_de` closure will be invoked with the derived salt for this DE.
    pub fn add_de<W: WriteDataElement, F: FnOnce(SE::DerivedSalt) -> W>(
        &mut self,
        build_de: F,
    ) -> Result<(), AddDataElementError<()>> {
        self.add_de_res(|derived_salt| Ok::<_, ()>(build_de(derived_salt)))
    }

    /// Convert a section builder's contents into an encoded section.
    ///
    /// Implemented without self to avoid partial-move issues.
    fn build_section(
        mut section_contents: tinyvec::ArrayVec<[u8; NP_ADV_MAX_SECTION_LEN]>,
        mut section_encoder: SE,
        adv_builder: &AdvBuilder,
    ) -> EncodedSection {
        // there is space because the capacity for DEs was restricted to allow it
        section_contents.resize(section_contents.len() + SE::SUFFIX_LEN, 0);

        section_contents[0] = section_contents
            .len()
            .try_into()
            .ok()
            .and_then(|len: u8| len.checked_sub(1))
            .expect("section length is always <=255 and non-negative");

        section_encoder.postprocess(
            adv_builder.header_byte(),
            section_contents[0],
            &mut section_contents[1..],
        );

        to_array_view(section_contents)
    }
}

/// Errors for adding a DE to a section
#[derive(Debug, PartialEq, Eq)]
pub enum AddDataElementError<E> {
    /// An error occurred when invoking the DE builder closure.
    BuildDeError(E),
    /// Too much data to fit into the section
    InsufficientSectionSpace,
}

/// The advertisement type, which dictates what sections can exist
#[derive(Debug, PartialEq, Eq)]
pub enum AdvertisementType {
    /// Plaintext advertisement with only plaintext sections
    Plaintext,
    /// Encrypted advertisement with only encrypted sections
    Encrypted,
}

impl AdvertisementType {
    fn max_sections(&self) -> usize {
        match self {
            AdvertisementType::Plaintext => NP_V1_ADV_MAX_PUBLIC_SECTION_COUNT,
            AdvertisementType::Encrypted => NP_V1_ADV_MAX_ENCRYPTED_SECTION_COUNT,
        }
    }
}

/// Everything needed to properly encode a section
pub trait SectionEncoder {
    /// How much space needs to be reserved for this identity's prefix bytes after the section
    /// header and before other DEs
    const PREFIX_LEN: usize;

    /// How much space needs to be reserved after the DEs
    const SUFFIX_LEN: usize;

    /// The DE offset to use for any DEs added to the section
    const INITIAL_DE_OFFSET: DataElementOffset;

    /// The advertisement type that can support this section
    const ADVERTISEMENT_TYPE: AdvertisementType;

    /// Postprocess the contents of the section (the data after the section header byte), which will
    /// start with [Self::PREFIX_LEN] bytes set aside for the identity's use, and similarly end with
    /// [Self::SUFFIX_LEN] bytes, with DEs (if any) in the middle.
    fn postprocess(&mut self, adv_header_byte: u8, section_header: u8, section_contents: &mut [u8]);

    /// The type of derived salt produced for a DE sharing a section with this identity.
    type DerivedSalt;

    /// Produce a `Self::Output` salt for a DE.
    fn de_salt(&self, de_offset: DataElementOffset) -> Self::DerivedSalt;
}

/// Public section for plaintext data elements
#[derive(Default, Debug)]
pub struct PublicSectionEncoder {}
impl SectionEncoder for PublicSectionEncoder {
    /// 1 byte of public identity DE header
    const PREFIX_LEN: usize = 1;
    const SUFFIX_LEN: usize = 0;
    /// Room for the public DE
    const INITIAL_DE_OFFSET: DataElementOffset = DataElementOffset::ZERO.incremented();
    const ADVERTISEMENT_TYPE: AdvertisementType = AdvertisementType::Plaintext;
    fn postprocess(
        &mut self,
        _adv_header_byte: u8,
        _section_header: u8,
        section_contents: &mut [u8],
    ) {
        section_contents[0..1].copy_from_slice(
            DeHeader { len: DeLength::ZERO, de_type: IdentityDataElementType::Public.type_code() }
                .serialize()
                .as_slice(),
        )
    }
    type DerivedSalt = ();
    fn de_salt(&self, _de_offset: DataElementOffset) -> Self::DerivedSalt {}
}

/// Encrypts the data elements and protects integrity with an np_ed25519 signature
/// using key material derived from an NP identity.
pub struct SignedEncryptedSectionEncoder<C: CryptoProvider> {
    identity_type: EncryptedIdentityDataElementType,
    salt: V1Salt<C>,
    metadata_key: MetadataKey,
    key_pair: np_ed25519::KeyPair<C>,
    aes_key: Aes128Key,
}

impl<C: CryptoProvider> SignedEncryptedSectionEncoder<C> {
    /// Build a [SignedEncryptedSectionEncoder] from an identity type,
    /// some broadcast crypto-material, and with a random salt.
    pub fn new_random_salt<B: SignedBroadcastCryptoMaterial>(
        rng: &mut C::CryptoRng,
        identity_type: EncryptedIdentityDataElementType,
        crypto_material: &B,
    ) -> Self {
        let salt: V1Salt<C> = rng.gen::<[u8; 16]>().into();
        Self::new(identity_type, salt, crypto_material)
    }

    /// Build a [SignedEncryptedSectionEncoder] from an identity type,
    /// a provided salt, and some broadcast crypto-material.
    pub(crate) fn new<B: SignedBroadcastCryptoMaterial>(
        identity_type: EncryptedIdentityDataElementType,
        salt: V1Salt<C>,
        crypto_material: &B,
    ) -> Self {
        let metadata_key = crypto_material.metadata_key();
        let key_seed = crypto_material.key_seed();
        let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&key_seed);
        let private_key = crypto_material.signing_key();
        let key_pair = np_ed25519::KeyPair::<C>::from_private_key(&private_key);
        let aes_key = key_seed_hkdf.extended_signed_section_aes_key();
        Self { identity_type, salt, metadata_key, key_pair, aes_key }
    }
}

impl<C: CryptoProvider> SectionEncoder for SignedEncryptedSectionEncoder<C> {
    const PREFIX_LEN: usize =
        EncryptionInfo::TOTAL_DE_LEN + EncryptedIdentityMetadata::TOTAL_DE_LEN;
    /// Ed25519 signature
    const SUFFIX_LEN: usize = crypto_provider::ed25519::SIGNATURE_LENGTH;
    /// Room for the encryption info and identity DEs
    const INITIAL_DE_OFFSET: DataElementOffset =
        DataElementOffset::ZERO.incremented().incremented();
    const ADVERTISEMENT_TYPE: AdvertisementType = AdvertisementType::Encrypted;

    fn postprocess(
        &mut self,
        adv_header_byte: u8,
        section_header: u8,
        section_contents: &mut [u8],
    ) {
        let encryption_info_bytes = EncryptionInfoDataElement::signature(
            self.salt.as_slice().try_into().expect("Salt should be 16 bytes"),
        )
        .serialize();
        section_contents[0..19].copy_from_slice(&encryption_info_bytes);

        let identity_header = identity_de_header(self.identity_type, self.metadata_key);
        section_contents[19..21].copy_from_slice(identity_header.serialize().as_slice());
        section_contents[21..37].copy_from_slice(&self.metadata_key.0);

        let nonce: AesCtrNonce = self
            .de_salt(v1_salt::DataElementOffset::from(1))
            .derive()
            .expect("AES-CTR nonce is a valid HKDF length");

        let (before_sig, sig) =
            section_contents.split_at_mut(section_contents.len() - Self::SUFFIX_LEN);
        let (encryption_info, after_encryption_info) =
            before_sig.split_at(EncryptionInfo::TOTAL_DE_LEN);

        let encryption_info: &[u8; EncryptionInfo::TOTAL_DE_LEN] =
            encryption_info.try_into().expect("encryption info should always be the correct size");

        // we need to sign the 16-byte IV, which doesn't have to actually fit in the adv, but we
        // don't need a bigger buffer here since we won't be including the 66 bytes for the sig +
        // header.
        // If the stack usage ever becomes a problem, we can investigate pre hashing for the
        // signature.
        let nonce_ref = &nonce;
        let section_signature_payload = SectionSignaturePayload::from_serialized_parts(
            adv_header_byte,
            section_header,
            encryption_info,
            nonce_ref,
            after_encryption_info,
        );

        let signature = section_signature_payload.sign(&self.key_pair);

        sig[0..64].copy_from_slice(&signature.to_bytes());

        let mut cipher = C::AesCtr128::new(&self.aes_key, NonceAndCounter::from_nonce(nonce));

        // encrypt just the part that should be ciphertext: identity DE contents and subsequent DEs
        cipher.apply_keystream(&mut section_contents[21..]);
    }

    type DerivedSalt = DeSalt<C>;

    fn de_salt(&self, de_offset: DataElementOffset) -> Self::DerivedSalt {
        DeSalt { salt: V1Salt::from(*self.salt.as_array_ref()), de_offset }
    }
}

/// Encrypts the data elements and protects integrity with a MIC using key material derived from
/// an NP identity.
pub struct MicEncryptedSectionEncoder<C: CryptoProvider> {
    identity_type: EncryptedIdentityDataElementType,
    salt: V1Salt<C>,
    metadata_key: MetadataKey,
    aes_key: Aes128Key,
    mic_hmac_key: np_hkdf::NpHmacSha256Key<C>,
}

impl<C: CryptoProvider> MicEncryptedSectionEncoder<C> {
    /// Build a [MicEncryptedSectionEncoder] from the provided identity
    /// info with a random salt.
    pub fn new_random_salt<B: BroadcastCryptoMaterial<V1>>(
        rng: &mut C::CryptoRng,
        identity_type: EncryptedIdentityDataElementType,
        crypto_material: &B,
    ) -> Self {
        let salt: V1Salt<C> = rng.gen::<[u8; 16]>().into();
        Self::new(identity_type, salt, crypto_material)
    }

    /// Build a [MicEncryptedSectionEncoder] from the provided identity info.
    pub(crate) fn new<B: BroadcastCryptoMaterial<V1>>(
        identity_type: EncryptedIdentityDataElementType,
        salt: V1Salt<C>,
        crypto_material: &B,
    ) -> Self {
        let metadata_key = crypto_material.metadata_key();
        let key_seed = crypto_material.key_seed();
        let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&key_seed);
        let aes_key = np_hkdf::UnsignedSectionKeys::aes_key(&key_seed_hkdf);
        let mic_hmac_key = np_hkdf::UnsignedSectionKeys::hmac_key(&key_seed_hkdf);

        Self { identity_type, salt, metadata_key, aes_key, mic_hmac_key }
    }

    /// Build a [MicEncrypedSectionEncoder] from the provided identity info.
    /// Exposed outside of this crate for testing purposes only, since this
    /// does not handle the generation of random salts.
    #[cfg(any(test, feature = "testing"))]
    pub fn new_for_testing<B: BroadcastCryptoMaterial<V1>>(
        identity_type: EncryptedIdentityDataElementType,
        salt: V1Salt<C>,
        crypto_material: &B,
    ) -> Self {
        Self::new(identity_type, salt, crypto_material)
    }
}

impl<C: CryptoProvider> SectionEncoder for MicEncryptedSectionEncoder<C> {
    const PREFIX_LEN: usize =
        EncryptionInfo::TOTAL_DE_LEN + EncryptedIdentityMetadata::TOTAL_DE_LEN;
    /// Length of mic
    const SUFFIX_LEN: usize = SectionMic::CONTENTS_LEN;
    /// Room for the mic, encryption info, and identity DEs
    const INITIAL_DE_OFFSET: DataElementOffset =
        DataElementOffset::ZERO.incremented().incremented();

    const ADVERTISEMENT_TYPE: AdvertisementType = AdvertisementType::Encrypted;

    fn postprocess(
        &mut self,
        adv_header_byte: u8,
        section_header: u8,
        section_contents: &mut [u8],
    ) {
        // prefix byte layout:
        // 0-18: Encryption Info DE (header + scheme + salt)
        // 19-20: Identity DE header
        // 21-36: Identity DE contents (metadata key)
        // Encryption Info DE
        let encryption_info_bytes = EncryptionInfoDataElement::mic(
            self.salt.as_slice().try_into().expect("Salt should be 16 bytes"),
        )
        .serialize();
        section_contents[0..19].copy_from_slice(&encryption_info_bytes);
        // Identity DE
        let identity_header = identity_de_header(self.identity_type, self.metadata_key);
        section_contents[19..21].copy_from_slice(identity_header.serialize().as_slice());
        section_contents[21..37].copy_from_slice(&self.metadata_key.0);
        // DE offset for identity is 1: Encryption Info DE, Identity DE, then other DEs
        let nonce: AesCtrNonce = self
            .de_salt(v1_salt::DataElementOffset::from(1))
            .derive()
            .expect("AES-CTR nonce is a valid HKDF length");
        let mut cipher = C::AesCtr128::new(&self.aes_key, NonceAndCounter::from_nonce(nonce));
        let ciphertext_end = section_contents.len() - SectionMic::CONTENTS_LEN;
        // encrypt just the part that should be ciphertext: identity DE contents and subsequent DEs
        cipher.apply_keystream(&mut section_contents[21..ciphertext_end]);
        // calculate MAC per the spec
        let mut section_hmac = self.mic_hmac_key.build_hmac();
        // svc uuid
        section_hmac.update(NP_SVC_UUID.as_slice());
        // adv header
        section_hmac.update(&[adv_header_byte]);
        // section header
        section_hmac.update(&[section_header]);
        // encryption info
        section_hmac.update(&encryption_info_bytes);
        // derived salt
        section_hmac.update(&nonce);
        // identity header + ciphertext
        section_hmac.update(&section_contents[19..ciphertext_end]);
        let mic: [u8; 32] = section_hmac.finalize();
        // write truncated MIC
        section_contents[ciphertext_end..].copy_from_slice(&mic[..SectionMic::CONTENTS_LEN]);
    }
    type DerivedSalt = DeSalt<C>;
    fn de_salt(&self, de_offset: DataElementOffset) -> Self::DerivedSalt {
        DeSalt { salt: V1Salt::from(*self.salt.as_array_ref()), de_offset }
    }
}

/// Derived salt for an individual data element.
pub struct DeSalt<C: CryptoProvider> {
    salt: V1Salt<C>,
    de_offset: DataElementOffset,
}

impl<C: CryptoProvider> DeSalt<C> {
    /// Derive salt of the requested length.
    ///
    /// The length must be a valid HKDF-SHA256 length.
    pub fn derive<const N: usize>(&self) -> Option<[u8; N]> {
        self.salt.derive(Some(self.de_offset))
    }
}

/// For DE structs that only implement one DE type, rather than multi-type impls.
pub trait SingleTypeDataElement {
    /// The DE type for the DE.
    const DE_TYPE: DeType;
}

/// Writes data for a V1 DE into a provided buffer.
///
/// V1 data elements can be hundreds of bytes, so we ideally wouldn't even stack allocate a buffer
/// big enough for that, hence an abstraction that writes into an existing buffer.
pub trait WriteDataElement {
    /// Returns the DE header that will be serialized into the section.
    fn de_header(&self) -> DeHeader;
    /// Write just the contents of the DE, returning `Some` if all contents could be written and
    /// `None` otherwise.
    fn write_de_contents<S: Sink<u8>>(&self, sink: &mut S) -> Option<()>;
}

// convenience impl for &W
impl<W: WriteDataElement> WriteDataElement for &W {
    fn de_header(&self) -> DeHeader {
        (*self).de_header()
    }

    fn write_de_contents<S: Sink<u8>>(&self, sink: &mut S) -> Option<()> {
        (*self).write_de_contents(sink)
    }
}

/// Serialization-specific representation of a DE header
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct DeHeader {
    /// The length of the content of the DE
    len: DeLength,
    pub(crate) de_type: DeType,
}

impl DeHeader {
    /// Build a DeHeader from the provided type and length
    pub fn new(de_type: DeType, len: DeLength) -> Self {
        DeHeader { de_type, len }
    }

    /// Serialize the DE header as per the V1 DE header format:
    /// - 1 byte form for length <= 3 bits, type <= 4 bits: `0LLLTTTT`
    /// - multi byte form: `0b1LLLLLLL [0b1TTTTTTT ...] 0b0TTTTTTT`
    ///   - the shortest possible encoding must be used (no empty prefix type bytes)
    ///
    /// We assume that a 32-bit de type is sufficient, which would take at most 5 7-bit chunks to
    /// encode, resulting in a total length of 6 bytes with the initial length byte.
    pub(crate) fn serialize(&self) -> ArrayView<u8, 6> {
        let mut buffer = [0; 6];
        let de_type = self.de_type.as_u32();
        let hi_bit = 0x80_u8;
        let len = self.len.len;
        if len < 8 && de_type < 16 {
            buffer[0] = len << 4 | de_type as u8;
            ArrayView::try_from_array(buffer, 1).expect("1 is a valid length")
        } else {
            // length w/ extended bit
            buffer[0] = hi_bit | len;

            // expand to a u64 so we can represent all 5 7-bit chunks of a u32, shifted so that
            // it fills the top 5 * 7 = 35 bits after the high bit, which is left unset so that
            // the MSB can be interpreted as a 7-bit chunk with an unset high bit.
            let mut type64 = (de_type as u64) << (64 - 35 - 1);
            let mut remaining_chunks = 5;
            let mut chunks_written = 0;
            // write 7 bit chunks, skipping leading 0 chunks
            while remaining_chunks > 0 {
                let chunk = type64.to_be_bytes()[0];
                remaining_chunks -= 1;

                // shift 7 more bits up, leaving the high bit unset
                type64 = (type64 << 7) & (u64::MAX >> 1);

                if chunks_written == 0 && chunk == 0 {
                    // skip leading all-zero chunks
                    continue;
                }

                buffer[1 + chunks_written] = chunk;
                chunks_written += 1;
            }
            if chunks_written > 0 {
                // fill in high bits for all but the last
                for byte in buffer[1..chunks_written].iter_mut() {
                    *byte |= hi_bit;
                }

                ArrayView::try_from_array(buffer, 1 + chunks_written).expect("length is at most 6")
            } else {
                // type byte is a leading 0 bit w/ 0 type, so use the existing 0 byte
                ArrayView::try_from_array(buffer, 2).expect("2 is a valid length")
            }
        }
    }
}

fn identity_de_header(
    id_type: EncryptedIdentityDataElementType,
    metadata_key: MetadataKey,
) -> DeHeader {
    DeHeader {
        de_type: id_type.type_code(),
        len: metadata_key
            .0
            .len()
            .try_into()
            .map_err(|_e| DeLengthOutOfRange)
            .and_then(|len: u8| len.try_into())
            .expect("metadata key is a valid DE length"),
    }
}

/// A wrapper around a fixed-size tinyvec that can have its capacity further constrained to handle
/// dynamic size limits.
#[derive(Debug)]
pub(crate) struct CapacityLimitedVec<T, const N: usize>
where
    T: fmt::Debug + Clone,
    [T; N]: tinyvec::Array + fmt::Debug,
    <[T; N] as tinyvec::Array>::Item: fmt::Debug + Clone,
{
    /// constraint on the occupied space in `vec`.
    /// Invariant: `vec.len() <= constraint` and `vec.capacity() >= capacity`.
    capacity: usize,
    vec: tinyvec::ArrayVec<[T; N]>,
}

impl<T, const N: usize> CapacityLimitedVec<T, N>
where
    T: fmt::Debug + Clone,
    [T; N]: tinyvec::Array + fmt::Debug,
    <[T; N] as tinyvec::Array>::Item: fmt::Debug + Clone,
{
    pub(crate) fn len(&self) -> usize {
        self.vec.len()
    }

    fn capacity(&self) -> usize {
        self.capacity
    }

    fn truncate(&mut self, len: usize) {
        self.vec.truncate(len);
    }

    fn into_inner(self) -> tinyvec::ArrayVec<[T; N]> {
        self.vec
    }
}

impl<T, const N: usize> Sink<<[T; N] as tinyvec::Array>::Item> for CapacityLimitedVec<T, N>
where
    T: fmt::Debug + Clone,
    [T; N]: tinyvec::Array + fmt::Debug,
    <[T; N] as tinyvec::Array>::Item: fmt::Debug + Clone,
{
    fn try_extend_from_slice(&mut self, items: &[<[T; N] as tinyvec::Array>::Item]) -> Option<()> {
        if items.len() > (self.capacity() - self.len()) {
            return None;
        }
        // won't panic: just checked the length
        self.vec.extend_from_slice(items);
        Some(())
    }

    fn try_push(&mut self, item: <[T; N] as tinyvec::Array>::Item) -> Option<()> {
        if self.len() == self.capacity() {
            // already full
            None
        } else {
            self.vec.push(item);
            Some(())
        }
    }
}

impl<T, const N: usize> AsRef<[<[T; N] as tinyvec::Array>::Item]> for CapacityLimitedVec<T, N>
where
    T: fmt::Debug + Clone,
    [T; N]: tinyvec::Array + fmt::Debug,
    <[T; N] as tinyvec::Array>::Item: fmt::Debug + Clone,
{
    fn as_ref(&self) -> &[<[T; N] as tinyvec::Array>::Item] {
        self.vec.as_slice()
    }
}
impl<T, const N: usize> AsMut<[<[T; N] as tinyvec::Array>::Item]> for CapacityLimitedVec<T, N>
where
    T: fmt::Debug + Clone,
    [T; N]: tinyvec::Array + fmt::Debug,
    <[T; N] as tinyvec::Array>::Item: fmt::Debug + Clone,
{
    fn as_mut(&mut self) -> &mut [<[T; N] as tinyvec::Array>::Item] {
        self.vec.as_mut_slice()
    }
}
