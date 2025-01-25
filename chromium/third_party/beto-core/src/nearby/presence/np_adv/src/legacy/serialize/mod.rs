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

//! Serialization for V0 advertisements.
//!
//! # Examples
//!
//! Serializing a plaintext advertisement:
//!
//! ```
//! use np_adv::{legacy::{data_elements::tx_power::TxPowerDataElement, serialize::*}, shared_data::*};
//!
//! let mut builder = AdvBuilder::new(UnencryptedEncoder);
//! builder
//!     .add_data_element(TxPowerDataElement::from(TxPower::try_from(3).expect("3 is a valid TxPower value")))
//!     .unwrap();
//! let packet = builder.into_advertisement().unwrap();
//! assert_eq!(
//!     &[
//!         0x00, // Adv Header
//!         0x15, 0x03, // tx power de
//!     ],
//!     packet.as_slice()
//! );
//! ```
//!
//! Serializing an encrypted advertisement:
//!
//! ```
//! use np_adv::{shared_data::*, legacy::{data_elements::de_type::*, data_elements::tx_power::TxPowerDataElement, serialize::*, *}};
//! use np_adv::credential::{v0::{V0, V0BroadcastCredential}};
//! use crypto_provider::CryptoProvider;
//! use crypto_provider_default::CryptoProviderImpl;
//! use ldt_np_adv::{V0Salt, V0IdentityToken};
//!
//! // Generate these from proper CSPRNGs -- using fixed data here
//! let metadata_key = V0IdentityToken::from([0x33; 14]);
//! let salt = V0Salt::from([0x01, 0x02]);
//! let key_seed = [0x44; 32];
//!
//! let broadcast_cred = V0BroadcastCredential::new(
//!     key_seed,
//!     metadata_key,
//! );
//!
//! let mut builder = AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(
//!     salt,
//!     &broadcast_cred,
//! ));
//!
//! builder
//!     .add_data_element(TxPowerDataElement::from(TxPower::try_from(3).expect("3 is a valid TxPower value")))
//!     .unwrap();
//!
//! let packet = builder.into_advertisement().unwrap();
//! ```
use core::fmt;

use array_view::ArrayView;
use crypto_provider::CryptoProvider;
use ldt::LdtCipher;
use ldt_np_adv::{V0IdentityToken, V0_IDENTITY_TOKEN_LEN};
use sink::Sink;

use crate::credential::v0::V0BroadcastCredential;
use crate::{
    extended::to_array_view,
    header::{VERSION_HEADER_V0_LDT, VERSION_HEADER_V0_UNENCRYPTED},
    legacy::{
        data_elements::{
            de_type::{DeActualLength, DeEncodedLength, DeTypeCode},
            DataElementSerializationBuffer, DataElementSerializeError, SerializeDataElement,
        },
        Ciphertext, PacketFlavor, Plaintext, BLE_4_ADV_SVC_MAX_CONTENT_LEN, NP_MAX_ADV_CONTENT_LEN,
        NP_MIN_ADV_CONTENT_LEN,
    },
};

mod header;

#[cfg(test)]
pub(crate) mod tests;

/// An encoder used in serializing an advertisement.
pub trait AdvEncoder: fmt::Debug {
    /// The flavor of packet this identity produces
    type Flavor: PacketFlavor;
    /// The error returned if postprocessing fails
    type Error: fmt::Debug;

    /// The version header to put at the start of the advertisement.
    const VERSION_HEADER: u8;

    /// The V0-specific header to be written at the start of the advertisement
    /// immediately after the version header.
    fn header(&self) -> header::V0Header;

    /// Perform identity-specific manipulation to the serialized DEs to produce the final
    /// advertisement format.
    ///
    /// `buf` has the contents of [Self::header] written at the start, followed
    /// by all of the DEs added to a packet. It does not include the NP top level header.
    ///
    /// The first `header_len` bytes of `buf` are the bytes produced by the call to [Self::header].
    ///
    /// Returns `Ok` if postprocessing was successful and the packet is finished, or `Err` if
    /// postprocessing failed and the packet should be discarded.
    fn postprocess(&self, header_len: usize, buf: &mut [u8]) -> Result<(), Self::Error>;
}

/// An unencrypted encoder with no associated identity.
#[derive(Debug)]
pub struct UnencryptedEncoder;

impl AdvEncoder for UnencryptedEncoder {
    type Flavor = Plaintext;
    type Error = UnencryptedEncodeError;
    const VERSION_HEADER: u8 = VERSION_HEADER_V0_UNENCRYPTED;

    fn header(&self) -> header::V0Header {
        header::V0Header::unencrypted()
    }

    fn postprocess(&self, _header_len: usize, buf: &mut [u8]) -> Result<(), Self::Error> {
        if buf.len() < NP_MIN_ADV_CONTENT_LEN {
            Err(UnencryptedEncodeError::InvalidLength)
        } else {
            Ok(())
        }
    }
}

/// Unencrypted encoding errors
#[derive(Debug, PartialEq, Eq)]
pub enum UnencryptedEncodeError {
    /// The advertisement content was outside the valid range
    InvalidLength,
}

/// Encoder used for encrypted packets (private, trusted, provisioned) that encrypts other DEs
/// (as well as the metadata key).
pub struct LdtEncoder<C: CryptoProvider> {
    salt: ldt_np_adv::V0Salt,
    identity_token: V0IdentityToken,
    ldt_enc: ldt_np_adv::NpLdtEncryptCipher<C>,
}

// Exclude sensitive members
impl<C: CryptoProvider> fmt::Debug for LdtEncoder<C> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "LdtEncoder {{ salt: {:X?} }}", self.salt)
    }
}

impl<C: CryptoProvider> LdtEncoder<C> {
    /// Build an `LdtIdentity` for the provided identity type, salt, and
    /// broadcast crypto-materials.
    pub fn new(salt: ldt_np_adv::V0Salt, broadcast_cred: &V0BroadcastCredential) -> Self {
        let identity_token = broadcast_cred.identity_token();
        let key_seed = broadcast_cred.key_seed();
        let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&key_seed);
        let ldt_key = key_seed_hkdf.v0_ldt_key();
        let ldt_enc = ldt_np_adv::NpLdtEncryptCipher::<C>::new(&ldt_key);

        Self { salt, identity_token, ldt_enc }
    }
}

impl<C: CryptoProvider> AdvEncoder for LdtEncoder<C> {
    type Flavor = Ciphertext;
    type Error = LdtEncodeError;
    const VERSION_HEADER: u8 = VERSION_HEADER_V0_LDT;

    fn header(&self) -> header::V0Header {
        header::V0Header::ldt_short_salt(self.salt, self.identity_token)
    }

    fn postprocess(&self, header_len: usize, buf: &mut [u8]) -> Result<(), LdtEncodeError> {
        // encrypt everything after v0 format and salt
        self.ldt_enc
            .encrypt(
                &mut buf[header_len - V0_IDENTITY_TOKEN_LEN..],
                &ldt_np_adv::salt_padder::<C>(self.salt),
            )
            .map_err(|e| match e {
                // too short, not enough DEs
                ldt::LdtError::InvalidLength(_) => LdtEncodeError::InvalidLength,
            })
    }
}

/// LDT encoding errors
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum LdtEncodeError {
    /// The minimum size (2 bytes) or maximum size (7 bytes for BLE 4.2
    /// advertisements) of DE data was not met
    InvalidLength,
}

/// The serialized form of a V0 adv, suitable for setting as the value for the NP UUID svc data.
pub type SerializedAdv = ArrayView<u8, BLE_4_ADV_SVC_MAX_CONTENT_LEN>;

/// Accumulates DEs, then massages the serialized DEs with the configured identity to produce
/// the final advertisement.
#[derive(Debug)]
pub struct AdvBuilder<E: AdvEncoder> {
    /// The first byte is for the adv header, then the next I::OVERHEAD_LEN bytes are set aside for
    /// use by [AdvEncoder::postprocess].
    buffer: [u8; BLE_4_ADV_SVC_MAX_CONTENT_LEN],
    /// How much of the buffer is consumed.
    /// Always <= buffer length, and at least 2
    len: usize,
    /// How long the header was
    header_len: usize,
    encoder: E,
}

impl<E: AdvEncoder> AdvBuilder<E> {
    /// Create an empty AdvBuilder with the provided identity.
    pub fn new(encoder: E) -> AdvBuilder<E> {
        let mut buffer = [0; BLE_4_ADV_SVC_MAX_CONTENT_LEN];
        buffer[0] = E::VERSION_HEADER;

        // encode the rest of the V0 header
        let header = encoder.header();
        let header_len = header.as_slice().len();
        // len will be at least 1, important for max_de_len safety in add_data_element
        let len = 1 + header_len;
        // check for broken identities
        debug_assert!(len < buffer.len());
        buffer[1..=header_len].copy_from_slice(header.as_slice());

        AdvBuilder {
            // conveniently the first byte is already 0, which is the correct header for v0.
            // 3 bit version (000 since this is version 0), 5 bit reserved (also all 0)
            buffer,
            len,
            header_len,
            encoder,
        }
    }

    /// Add the data element to the packet buffer, if there is space.
    pub fn add_data_element<D: SerializeDataElement<E::Flavor>>(
        &mut self,
        data_element: D,
    ) -> Result<(), AddDataElementError> {
        // invariant: self.len <= buffer length
        debug_assert!(self.len <= self.buffer.len());
        let dest = &mut self.buffer[self.len..];
        // because self.len is at least 1, the dest will be no more than
        // `[BLE_ADV_SVC_CONTENT_LEN] - 1 = [NP_MAX_ADV_CONTENT_LEN]`
        let de_buf = serialize_de(&data_element, dest.len())?;
        dest[..de_buf.len()].copy_from_slice(de_buf.as_slice());
        // invariant: de_buf fit in the remaining space, so adding its length is safe
        self.len += de_buf.len();
        debug_assert!(self.len <= self.buffer.len());

        Ok(())
    }

    /// Return the finished advertisement (version header || V0 header || DEs),
    /// or `None` if the adv could not be built.
    pub fn into_advertisement(mut self) -> Result<SerializedAdv, E::Error> {
        // encrypt, if applicable
        self.encoder
            // skip NP version header for postprocessing
            .postprocess(self.header_len, &mut self.buffer[1..self.len])
            .map(|_| ArrayView::try_from_array(self.buffer, self.len).expect("len is always valid"))
    }
}

/// Errors that can occur for [AdvBuilder.add_data_element].
#[derive(Debug, PartialEq, Eq)]
pub enum AddDataElementError {
    /// The provided DE can't fit in the remaining space
    InsufficientAdvSpace,
}

impl From<DataElementSerializeError> for AddDataElementError {
    fn from(value: DataElementSerializeError) -> Self {
        match value {
            DataElementSerializeError::InsufficientSpace => Self::InsufficientAdvSpace,
        }
    }
}

/// Encode a DE type and length into a DE header byte.
pub(crate) fn encode_de_header(code: DeTypeCode, header_len: DeEncodedLength) -> u8 {
    // 4 high bits are length, 4 low bits are type
    (header_len.as_u8() << 4) | code.as_u8()
}

/// Encode a DE into a buffer.
///
/// The buffer will contain the DE header and DE contents, if any, and will
/// not exceed `max_de_len`.
///
/// # Panics
/// `max_de_len` must be no larger than [NP_MAX_ADV_CONTENT_LEN].
fn serialize_de<F: PacketFlavor, D: SerializeDataElement<F>>(
    data_element: &D,
    max_de_len: usize,
) -> Result<SerializedDataElement, AddDataElementError> {
    let mut de_buf = DataElementSerializationBuffer::new(max_de_len)
        .expect("max_de_len must not exceed NP_MAX_DE_CONTENT_LEN");

    // placeholder for header
    de_buf.try_push(0).ok_or(AddDataElementError::InsufficientAdvSpace)?;
    data_element.serialize_contents(&mut de_buf)?;

    let encoded_len = data_element.map_actual_len_to_encoded_len(
        DeActualLength::try_from(de_buf.len() - 1)
            .expect("DE fit in buffer after header, so it should be a valid size"),
    );

    let mut vec = de_buf.into_inner().into_inner();
    vec[0] = encode_de_header(data_element.de_type_code(), encoded_len);

    debug_assert!(vec.len() <= max_de_len);

    Ok(SerializedDataElement::new(to_array_view(vec)))
}

/// The serialized form of a data element, including its header.
pub(crate) struct SerializedDataElement {
    data: ArrayView<u8, NP_MAX_ADV_CONTENT_LEN>,
}

impl SerializedDataElement {
    fn new(data: ArrayView<u8, NP_MAX_ADV_CONTENT_LEN>) -> Self {
        Self { data }
    }

    /// The serialized DE, starting with the DE header byte
    pub(crate) fn as_slice(&self) -> &[u8] {
        self.data.as_slice()
    }

    pub(crate) fn len(&self) -> usize {
        self.data.len()
    }
}
