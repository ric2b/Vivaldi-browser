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
//! use crypto_provider_default::CryptoProviderImpl;
//! use np_adv::{
//!     extended::{data_elements::*, serialize::*, de_type::DeType, V1_ENCODING_UNENCRYPTED},
//!     shared_data::TxPower
//! };
//!
//! // no section identities or DEs need salt in this example
//! let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
//! let mut section_builder = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();
//!
//! section_builder.add_de(|_salt| TxPowerDataElement::from(TxPower::try_from(3).unwrap())).unwrap();
//!
//! // add some other DE with type = 1000
//! section_builder.add_de_res(|_salt|
//!     GenericDataElement::try_from( DeType::from(1000_u32), &[10, 11, 12, 13])
//! ).unwrap();
//!
//! section_builder.add_to_advertisement::<CryptoProviderImpl>();
//!
//! assert_eq!(
//!     &[
//!         0x20, // version header
//!         V1_ENCODING_UNENCRYPTED, //section format
//!         0x09, // section length
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
//!     credential::{ v1::{V1, V1BroadcastCredential}},
//!     extended::{data_elements::*, serialize::*, de_type::DeType, V1IdentityToken },
//! };
//! use rand::{Rng as _, SeedableRng as _};
//! use crypto_provider::{CryptoProvider, CryptoRng, ed25519};
//! use crypto_provider_default::CryptoProviderImpl;
//! use np_adv::shared_data::TxPower;
//!
//! let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
//!
//! // these would come from the credential
//!
//! let mut rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();
//! let identity_token = rng.gen();
//! let key_seed: [u8; 32] = rng.gen();
//! // use your preferred crypto impl
//! let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
//!
//! let broadcast_cm = V1BroadcastCredential::new(
//!     key_seed,
//!     identity_token,
//!     ed25519::PrivateKey::generate::<<CryptoProviderImpl as CryptoProvider>::Ed25519>(),
//! );
//!
//! let mut section_builder = adv_builder.section_builder(MicEncryptedSectionEncoder::<_>::new_random_salt::<CryptoProviderImpl>(
//!     &mut rng,
//!     &broadcast_cm,
//! )).unwrap();
//!
//! section_builder.add_de(|_salt| TxPowerDataElement::from(TxPower::try_from(3).unwrap())).unwrap();
//!
//! // add some other DE with type = 1000
//! section_builder.add_de_res(|salt|
//!     GenericDataElement::try_from(
//!         DeType::from(1000_u32),
//!         &do_fancy_crypto(salt.derive::<16, CryptoProviderImpl>().expect("16 is a valid HKDF length")))
//! ).unwrap();
//!
//! section_builder.add_to_advertisement::<CryptoProviderImpl>();
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

#[cfg(feature = "std")]
extern crate std;

use core::fmt::{self, Display};

use array_view::ArrayView;
use crypto_provider::CryptoProvider;
use np_hkdf::v1_salt::{DataElementOffset, ExtendedV1Salt};
use sink::Sink;

use crate::extended::{
    de_requires_extended_bit, de_type::DeType, serialize::section::EncodedSection, to_array_view,
    DeLength, BLE_5_ADV_SVC_MAX_CONTENT_LEN, NP_ADV_MAX_SECTION_LEN, NP_V1_ADV_MAX_SECTION_COUNT,
};

mod section;

use crate::header::VERSION_HEADER_V1;
pub use section::{
    encoder::{
        MicEncryptedSectionEncoder, SectionEncoder, SignedEncryptedSectionEncoder,
        UnencryptedSectionEncoder,
    },
    AddDataElementError, SectionBuilder,
};

#[cfg(test)]
use crate::header::V1AdvHeader;

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
    // TODO make this configurable, and test making sections whose length is not restricted by BLE limitations
    /// Contains the adv header byte
    adv: tinyvec::ArrayVec<[u8; BLE_5_ADV_SVC_MAX_CONTENT_LEN]>,
    /// To track the number of sections that are in the advertisement
    section_count: usize,
    /// Advertisement type: Public or Encrypted
    advertisement_type: AdvertisementType,
}

impl AsMut<AdvBuilder> for AdvBuilder {
    fn as_mut(&mut self) -> &mut AdvBuilder {
        self
    }
}

impl AdvBuilder {
    /// Build an [AdvBuilder].
    pub fn new(advertisement_type: AdvertisementType) -> Self {
        let mut adv = tinyvec::ArrayVec::new();
        adv.push(VERSION_HEADER_V1);
        Self { adv, section_count: 0, advertisement_type }
    }

    /// Create a section builder whose contents may be added to this advertisement.
    ///
    /// The builder will not accept more DEs than can fit given the space already used in the
    /// advertisement by previous sections, if any.
    ///
    /// Once the builder is populated, add it to the originating advertisement with
    /// [SectionBuilder.add_to_advertisement].
    pub fn section_builder<SE: SectionEncoder>(
        &mut self,
        section_encoder: SE,
    ) -> Result<SectionBuilder<&mut AdvBuilder, SE>, AddSectionError> {
        let (header_len, contents) = self.prepare_section_builder_buffer(&section_encoder)?;
        Ok(SectionBuilder::new(header_len, contents, section_encoder, self))
    }

    /// Create a section builder which actually takes ownership of this advertisement builder.
    ///
    /// This is unlike `AdvertisementBuilder#section_builder` in that the returned section
    /// builder will take ownership of this advertisement builder, if the operation was
    /// successful. Otherwise, this advertisement builder will be returned back to the
    /// caller unaltered as part of the `Err` arm.
    #[allow(clippy::result_large_err)]
    pub fn into_section_builder<SE: SectionEncoder>(
        self,
        section_encoder: SE,
    ) -> Result<SectionBuilder<AdvBuilder, SE>, (AdvBuilder, AddSectionError)> {
        match self.prepare_section_builder_buffer::<SE>(&section_encoder) {
            Ok((header_len, section)) => {
                Ok(SectionBuilder::new(header_len, section, section_encoder, self))
            }
            Err(err) => Err((self, err)),
        }
    }

    /// Convert the builder into an encoded advertisement.
    pub fn into_advertisement(self) -> EncodedAdvertisement {
        EncodedAdvertisement { adv: to_array_view(self.adv) }
    }

    /// Gets the current number of sections added to this advertisement
    /// builder, not counting any outstanding SectionBuilders.
    pub fn section_count(&self) -> usize {
        self.section_count
    }

    /// Returns the length of the header (excluding the leading length byte),
    /// and a buffer already populated with a placeholder section length byte and the rest
    /// of the header.
    fn prepare_section_builder_buffer<SE: SectionEncoder>(
        &self,
        section_encoder: &SE,
    ) -> Result<(usize, CapacityLimitedVec<u8, NP_ADV_MAX_SECTION_LEN>), AddSectionError> {
        if self.section_count >= NP_V1_ADV_MAX_SECTION_COUNT {
            return Err(AddSectionError::MaxSectionCountExceeded);
        }
        if self.advertisement_type != SE::ADVERTISEMENT_TYPE {
            return Err(AddSectionError::IncompatibleSectionType);
        }

        // The header contains all the header bytes except for the final length byte.
        let header = section_encoder.header();
        let header_slice = header.as_slice();

        // the max overall len available to the section
        let available_len = self.adv.capacity() - self.adv.len();

        let mut prefix = available_len
            .checked_sub(SE::SUFFIX_LEN)
            .and_then(CapacityLimitedVec::new)
            .ok_or(AddSectionError::InsufficientAdvSpace)?;
        prefix.try_extend_from_slice(header_slice).ok_or(AddSectionError::InsufficientAdvSpace)?;
        // Placeholder for section length, which we do not know yet
        prefix.try_push(0).ok_or(AddSectionError::InsufficientAdvSpace)?;
        Ok((header_slice.len(), prefix))
    }

    /// Add the section, which must have come from a SectionBuilder generated from this, into this
    /// advertisement.
    fn add_section(&mut self, section: EncodedSection) {
        self.adv
            .try_extend_from_slice(section.as_slice())
            .expect("section capacity enforced in the section builder");
        self.section_count += 1;
    }

    #[cfg(test)]
    fn adv_header(&self) -> V1AdvHeader {
        V1AdvHeader::new(self.adv[0])
    }
}

/// Errors that can occur when adding a section to an advertisement
#[derive(Debug, PartialEq, Eq)]
pub enum AddSectionError {
    /// The advertisement doesn't have enough space to hold the minimum size of the section
    InsufficientAdvSpace,
    /// The advertisement can only hold a maximum of NP_V1_ADV_MAX_SECTION_COUNT number of sections
    MaxSectionCountExceeded,
    /// An incompatible section trying to be added
    IncompatibleSectionType,
}

impl Display for AddSectionError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            AddSectionError::InsufficientAdvSpace => {
                write!(f, "The advertisement (max {BLE_5_ADV_SVC_MAX_CONTENT_LEN} bytes) doesn't have enough remaining space to hold the section")
            }
            AddSectionError::MaxSectionCountExceeded => {
                write!(f, "The advertisement can only hold a maximum of {NP_V1_ADV_MAX_SECTION_COUNT} number of sections")
            }
            AddSectionError::IncompatibleSectionType => {
                write!(f, "Public and Encrypted sections cannot be mixed in the same advertisement")
            }
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for AddSectionError {}

/// An encoded NP V1 advertisement, starting with the NP advertisement header byte.
#[derive(Debug, PartialEq, Eq)]
pub struct EncodedAdvertisement {
    adv: ArrayView<u8, BLE_5_ADV_SVC_MAX_CONTENT_LEN>,
}

impl EncodedAdvertisement {
    /// Returns the advertisement as a slice.
    pub fn as_slice(&self) -> &[u8] {
        self.adv.as_slice()
    }
    /// Converts this encoded advertisement into
    /// a raw byte-array.
    pub fn into_array_view(self) -> ArrayView<u8, BLE_5_ADV_SVC_MAX_CONTENT_LEN> {
        self.adv
    }
}

/// The advertisement type, which dictates what sections can exist
#[derive(Debug, PartialEq, Eq)]
pub enum AdvertisementType {
    /// Plaintext advertisement with only plaintext sections
    Plaintext,
    /// Encrypted advertisement with only encrypted sections
    Encrypted,
}

/// Derived salt for an individual data element.
pub struct DeSalt {
    salt: ExtendedV1Salt,
    de_offset: DataElementOffset,
}

impl DeSalt {
    /// Derive salt of the requested length.
    ///
    /// The length must be a valid HKDF-SHA256 length.
    pub fn derive<const N: usize, C: CryptoProvider>(&self) -> Option<[u8; N]> {
        self.salt.derive::<N, C>(Some(self.de_offset))
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
        if !de_requires_extended_bit(de_type, len) {
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
    /// Invariant: `vec.len() <= capacity` and `vec.capacity() >= capacity`.
    capacity: usize,
    vec: tinyvec::ArrayVec<[T; N]>,
}

impl<T, const N: usize> CapacityLimitedVec<T, N>
where
    T: fmt::Debug + Clone,
    [T; N]: tinyvec::Array + fmt::Debug,
    <[T; N] as tinyvec::Array>::Item: fmt::Debug + Clone,
{
    /// Returns `None` if `capacity > N`
    pub(crate) fn new(capacity: usize) -> Option<Self> {
        if capacity <= N {
            Some(Self { capacity, vec: tinyvec::ArrayVec::new() })
        } else {
            None
        }
    }

    pub(crate) fn len(&self) -> usize {
        self.vec.len()
    }

    fn capacity(&self) -> usize {
        self.capacity
    }

    fn truncate(&mut self, len: usize) {
        self.vec.truncate(len);
    }

    pub(crate) fn into_inner(self) -> tinyvec::ArrayVec<[T; N]> {
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
