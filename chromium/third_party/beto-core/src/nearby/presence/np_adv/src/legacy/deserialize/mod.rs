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

//! V0 data element deserialization.
//!
//! This module only deals with the _contents_ of an advertisement, not the advertisement header.

use core::marker::PhantomData;

use crate::{
    credential::v0::V0,
    de_type::EncryptedIdentityDataElementType,
    legacy::{
        actions,
        data_elements::{DataElement, *},
        de_type::{DataElementType, DeEncodedLength, DeTypeCode, PlainDataElementType},
        Ciphertext, PacketFlavor, Plaintext, ShortMetadataKey, NP_MAX_DE_CONTENT_LEN,
    },
    HasIdentityMatch, PlaintextIdentityMode,
};
use array_view::ArrayView;
use crypto_provider::CryptoProvider;
use ldt_np_adv::{LegacySalt, NP_LEGACY_METADATA_KEY_LEN};
use nom::{bytes, combinator, number, sequence};

use super::BLE_ADV_SVC_CONTENT_LEN;

#[cfg(test)]
mod tests;

/// Deserialize an advertisement into data elements (if plaintext) or an identity type and
/// ciphertext.
pub(crate) fn deserialize_adv_contents<C: CryptoProvider>(
    input: &[u8],
) -> Result<IntermediateAdvContents<'_>, AdvDeserializeError> {
    parse_raw_adv_contents::<C>(input).and_then(|raw_adv| match raw_adv {
        RawAdvertisement::Plaintext(adv_contents) => {
            if adv_contents.data_elements().next().is_none() {
                return Err(AdvDeserializeError::NoPublicDataElements);
            }

            Ok(IntermediateAdvContents::Plaintext(adv_contents))
        }
        RawAdvertisement::Ciphertext(eac) => Ok(IntermediateAdvContents::Ciphertext(eac)),
    })
}

/// Parse an advertisement's contents to the level of raw data elements (i.e no decryption,
/// no per-type deserialization, etc).
///
/// Consumes the entire input.
fn parse_raw_adv_contents<C: CryptoProvider>(
    input: &[u8],
) -> Result<RawAdvertisement, AdvDeserializeError> {
    if input.is_empty() {
        return Err(AdvDeserializeError::MissingIdentity);
    }
    match parse_de(input) {
        Ok((rem, identity_de)) => {
            if let Some(identity_de_type) = identity_de.de_type.try_as_identity_de_type() {
                match identity_de_type.as_encrypted_identity_de_type() {
                    Some(encrypted_de_type) => {
                        if matches!(parse_de(rem), Err(nom::Err::Error(..))) {
                            match encrypted_de_type {
                                // TODO handle length=0 provisioned identity DEs
                                EncryptedIdentityDataElementType::Private
                                | EncryptedIdentityDataElementType::Trusted
                                | EncryptedIdentityDataElementType::Provisioned => combinator::map(
                                    parse_encrypted_identity_de_contents,
                                    |(salt, payload)| {
                                        RawAdvertisement::Ciphertext(EncryptedAdvContents {
                                            identity_type: encrypted_de_type,
                                            salt_padder: ldt_np_adv::salt_padder::<16, C>(salt),
                                            salt,
                                            ciphertext: payload,
                                        })
                                    },
                                )(
                                    identity_de.contents,
                                )
                                .map(|(_rem, contents)| contents)
                                .map_err(|_e| AdvDeserializeError::AdvertisementDeserializeError),
                            }
                        } else {
                            Err(AdvDeserializeError::TooManyTopLevelDataElements)
                        }
                    }
                    // It's an identity de, but not encrypted, so it must be public, and the rest
                    // must be plain
                    None => Ok(RawAdvertisement::Plaintext(PlaintextAdvContents {
                        identity_type: PlaintextIdentityMode::Public,
                        data: rem,
                    })),
                }
            } else {
                Err(AdvDeserializeError::MissingIdentity)
            }
        }
        Err(nom::Err::Error(_)) | Err(nom::Err::Failure(_)) => {
            Err(AdvDeserializeError::AdvertisementDeserializeError)
        }
        Err(nom::Err::Incomplete(_)) => {
            panic!("Should not hit Incomplete when using nom::complete parsers")
        }
    }
}

/// Legacy advertisement parsing errors
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum AdvDeserializeError {
    /// Parsing the overall advertisement or DE structure failed
    AdvertisementDeserializeError,
    /// Must not have any other top level data elements if there is an encrypted identity DE
    TooManyTopLevelDataElements,
    /// Missing identity DE
    MissingIdentity,
    /// Non-identity DE contents must not be empty
    NoPublicDataElements,
}

/// Parse an individual DE into its header and contents.
fn parse_de(input: &[u8]) -> nom::IResult<&[u8], RawDataElement, DataElementDeserializeError> {
    let (remaining, (de_type, actual_len)) =
        combinator::map_opt(number::complete::u8, |de_header| {
            // header: LLLLTTTT
            let len = de_header >> 4;
            let de_type = de_header & 0x0F;
            DeTypeCode::try_from(de_type).ok().and_then(DataElementType::from_type_code).and_then(
                |de_type| {
                    len.try_into()
                        .ok()
                        .and_then(|len: DeEncodedLength| {
                            de_type.actual_len_for_encoded_len(len).ok()
                        })
                        .map(|len| (de_type, len))
                },
            )
        })(input)?;

    combinator::map(bytes::complete::take(actual_len.as_usize()), move |contents| RawDataElement {
        de_type,
        contents,
    })(remaining)
}

/// Parse legacy encrypted identity DEs (private, trusted, provisioned) into salt and ciphertext
/// (encrypted metadata key and at least 2 bytes of DEs).
///
/// Consumes the entire input.
fn parse_encrypted_identity_de_contents(
    de_contents: &[u8],
) -> nom::IResult<&[u8], (ldt_np_adv::LegacySalt, &[u8])> {
    combinator::all_consuming(sequence::tuple((
        // 2-byte salt
        combinator::map(
            combinator::map_res(bytes::complete::take(2_usize), |slice: &[u8]| slice.try_into()),
            |arr: [u8; 2]| arr.into(),
        ),
        // 14-byte encrypted metadata key plus encrypted DEs, which must together be at least 16
        // bytes (AES block size), and at most a full DE minus the size of the salt.
        bytes::complete::take_while_m_n(16_usize, NP_MAX_DE_CONTENT_LEN - 2, |_| true),
    )))(de_contents)
}

/// A data element with content length determined and validated per its type's length rules, but
/// no further decoding performed.
#[derive(Debug, PartialEq, Eq)]
struct RawDataElement<'d> {
    de_type: DataElementType,
    /// Byte array payload of the data element, without the DE header.
    contents: &'d [u8],
}

/// An advertisement broken down into data elements, but before decryption or mapping to higher
/// level DE representations.
#[derive(Debug, PartialEq, Eq)]
enum RawAdvertisement<'d> {
    Plaintext(PlaintextAdvContents<'d>),
    Ciphertext(EncryptedAdvContents<'d>),
}

/// An iterator that parses the given data elements iteratively. In environments
/// where memory is not severely constrained, it is usually safer to collect
/// this into `Result<Vec<PlainDataElement>>` so the validity of the whole
/// advertisement can be checked before proceeding with further processing.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PlainDeIterator<'d, F>
where
    F: PacketFlavor,
    actions::ActionsDataElement<F>: DataElement,
{
    /// Data to be parsed, containing a sequence of data elements in serialized
    /// form. This should not contain the identity data elements.
    data: &'d [u8],
    _marker: PhantomData<F>,
}

impl<'d, F> PlainDeIterator<'d, F>
where
    F: PacketFlavor,
    actions::ActionsDataElement<F>: DataElement,
{
    fn raw_de_to_plain_de(
        raw_de: RawDataElement<'d>,
    ) -> Result<PlainDataElement<F>, DataElementDeserializeError> {
        let de_type = raw_de
            .de_type
            .try_as_plain_de_type()
            .ok_or(DataElementDeserializeError::DuplicateIdentityDataElement)?;
        (RawPlainDataElement { de_type, contents: raw_de.contents }).try_deserialize()
    }
}

impl<'d, F> Iterator for PlainDeIterator<'d, F>
where
    F: PacketFlavor,
    actions::ActionsDataElement<F>: DataElement,
{
    type Item = Result<PlainDataElement<F>, DataElementDeserializeError>;

    fn next(&mut self) -> Option<Self::Item> {
        let parse_result = nom::combinator::cut(nom::combinator::map_res(
            parse_de,
            Self::raw_de_to_plain_de,
        ))(self.data);
        match parse_result {
            Ok((rem, de)) => {
                self.data = rem;
                Some(Ok(de))
            }
            Err(nom::Err::Error(_)) => {
                panic!("All Errors are turned into Failures with `cut` above");
            }
            Err(nom::Err::Failure(DataElementDeserializeError::NomError(
                nom::error::ErrorKind::Eof,
            ))) => {
                if self.data.is_empty() {
                    None
                } else {
                    Some(Err(DataElementDeserializeError::UnexpectedDataRemaining))
                }
            }
            Err(nom::Err::Failure(e)) => Some(Err(e)),
            Err(nom::Err::Incomplete(_)) => {
                panic!("Incomplete unexpected when using nom::complete APIs")
            }
        }
    }
}

/// A "plain" data element (not a container) without parsing the content.
#[derive(Debug, PartialEq, Eq)]
pub(crate) struct RawPlainDataElement<'d> {
    de_type: PlainDataElementType,
    /// Byte array payload of the data element, without the DE header.
    contents: &'d [u8],
}

impl<'d> RawPlainDataElement<'d> {
    /// Deserialize into a [PlainDataElement] to expose DE-type-specific data representations.
    ///
    /// Returns `None` if the contents of the raw DE can't be deserialized into the corresponding
    /// DE's representation.
    fn try_deserialize<F>(&self) -> Result<PlainDataElement<F>, DataElementDeserializeError>
    where
        F: PacketFlavor,
        actions::ActionsDataElement<F>: DataElement,
    {
        match self.de_type {
            PlainDataElementType::Actions => {
                actions::ActionsDataElement::deserialize::<F>(self.contents)
                    .map(PlainDataElement::Actions)
            }
            PlainDataElementType::TxPower => {
                TxPowerDataElement::deserialize::<F>(self.contents).map(PlainDataElement::TxPower)
            }
        }
    }
}

/// Contents of an encrypted advertisement before decryption.
#[derive(Debug, PartialEq, Eq)]
pub(crate) struct EncryptedAdvContents<'d> {
    identity_type: EncryptedIdentityDataElementType,
    /// Salt from the advertisement, converted into a padder.
    /// Pre-calculated so it's only derived once across multiple decrypt attempts.
    salt_padder: ldt::XorPadder<{ crypto_provider::aes::BLOCK_SIZE }>,
    /// The salt instance used for encryption of this advertisement.
    salt: LegacySalt,
    /// Ciphertext containing the metadata key and any data elements
    ciphertext: &'d [u8],
}

impl<'d> EncryptedAdvContents<'d> {
    /// Try decrypting with an identity's LDT cipher and deserializing the resulting data elements.
    ///
    /// Returns the decrypted data if decryption and verification succeeded and the resulting DEs could be parsed
    /// successfully, otherwise `Err`.
    pub(crate) fn try_decrypt<C: CryptoProvider>(
        &self,
        cipher: &ldt_np_adv::LdtNpAdvDecrypterXtsAes128<C>,
    ) -> Result<DecryptedAdvContents, DecryptError> {
        let plaintext = cipher
            .decrypt_and_verify(self.ciphertext, &self.salt_padder)
            .map_err(|_e| DecryptError::DecryptOrVerifyError)?;

        // plaintext starts with 14 bytes of metadata key, then DEs.
        let (remaining, metadata_key) = combinator::map_res(
            bytes::complete::take(NP_LEGACY_METADATA_KEY_LEN),
            |slice: &[u8]| slice.try_into(),
        )(plaintext.as_slice())
        .map_err(|_e: nom::Err<nom::error::Error<&[u8]>>| {
            DecryptError::DeserializeError(AdvDeserializeError::AdvertisementDeserializeError)
        })?;

        let remaining_arr = ArrayView::try_from_slice(remaining)
            .expect("Max remaining = 31 - 14 = 17 bytes < BLE_ADV_SVC_CONTENT_LEN");

        Ok(DecryptedAdvContents::new(
            self.identity_type,
            ShortMetadataKey(metadata_key),
            self.salt,
            remaining_arr,
        ))
    }
}

/// Errors that can occur decrypting encrypted advertisements.
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub(crate) enum DecryptError {
    /// Decrypting or verifying the advertisement ciphertext failed
    DecryptOrVerifyError,
    /// Decrypting succeeded, but deserializing from the plaintext failed
    DeserializeError(AdvDeserializeError),
}

impl From<AdvDeserializeError> for DecryptError {
    fn from(e: AdvDeserializeError) -> Self {
        DecryptError::DeserializeError(e)
    }
}

/// All v0 normal DE types with deserialized contents.
#[derive(Debug, PartialEq, Eq)]
#[allow(missing_docs)]
pub enum PlainDataElement<F: PacketFlavor> {
    Actions(actions::ActionsDataElement<F>),
    TxPower(TxPowerDataElement),
}

impl<F: PacketFlavor> PlainDataElement<F> {
    /// Returns the DE type as a u8
    #[cfg(feature = "devtools")]
    pub fn de_type_code(&self) -> u8 {
        match self {
            PlainDataElement::Actions(_) => DataElementType::Actions.type_code().as_u8(),
            PlainDataElement::TxPower(_) => DataElementType::TxPower.type_code().as_u8(),
        }
    }

    /// Returns the serialized contents of the DE
    #[cfg(feature = "devtools")]
    pub fn de_contents(&self) -> alloc::vec::Vec<u8> {
        use crate::legacy::serialize::{DataElementBundle, ToDataElementBundle};
        match self {
            PlainDataElement::Actions(a) => {
                let bundle: DataElementBundle<F> = a.to_de_bundle();
                bundle.contents_as_slice().to_vec()
            }
            PlainDataElement::TxPower(t) => {
                let bundle: DataElementBundle<F> = t.to_de_bundle();
                bundle.contents_as_slice().to_vec()
            }
        }
    }
}

/// The contents of a plaintext advertisement after deserializing DE contents
#[derive(Debug, PartialEq, Eq)]
pub struct PlaintextAdvContents<'d> {
    identity_type: PlaintextIdentityMode,
    /// Contents of the advertisement excluding the identity DE
    data: &'d [u8],
}

impl<'d> PlaintextAdvContents<'d> {
    /// Returns the identity type used for the advertisement
    pub fn identity(&self) -> PlaintextIdentityMode {
        self.identity_type
    }

    /// Returns an iterator over the v0 data elements
    pub fn data_elements(&self) -> PlainDeIterator<'d, Plaintext> {
        PlainDeIterator { data: self.data, _marker: PhantomData }
    }
}

/// Contents of an encrypted advertisement after decryption and deserializing DEs.
#[derive(Debug, PartialEq, Eq)]
pub struct DecryptedAdvContents {
    identity_type: EncryptedIdentityDataElementType,
    metadata_key: ShortMetadataKey,
    salt: LegacySalt,
    /// The decrypted data in this advertisement. This should be a sequence of
    /// serialized data elements, excluding the identity DE.
    data: ArrayView<u8, { BLE_ADV_SVC_CONTENT_LEN }>,
}

impl DecryptedAdvContents {
    /// Returns a new DecryptedAdvContents with the provided contents.
    fn new(
        identity_type: EncryptedIdentityDataElementType,
        metadata_key: ShortMetadataKey,
        salt: LegacySalt,
        data: ArrayView<u8, { BLE_ADV_SVC_CONTENT_LEN }>,
    ) -> Self {
        Self { identity_type, metadata_key, salt, data }
    }

    /// The type of identity DE used in the advertisement.
    pub fn identity_type(&self) -> EncryptedIdentityDataElementType {
        self.identity_type
    }

    /// Iterator over the data elements in an advertisement, except for any DEs related to resolving
    /// the identity or otherwise validating the payload (e.g. any identity DEs like Private
    /// Identity).
    pub fn data_elements(&self) -> PlainDeIterator<Ciphertext> {
        PlainDeIterator { data: self.data.as_slice(), _marker: PhantomData }
    }

    /// The salt used for decryption of this advertisement.
    pub fn salt(&self) -> LegacySalt {
        self.salt
    }
}

impl HasIdentityMatch for DecryptedAdvContents {
    type Version = V0;
    fn metadata_key(&self) -> ShortMetadataKey {
        self.metadata_key
    }
}

/// The contents of an advertisement after plaintext DEs, if any, have been deserialized, but
/// before any decryption is done.
#[derive(Debug, PartialEq, Eq)]
pub(crate) enum IntermediateAdvContents<'d> {
    /// Plaintext advertisements
    Plaintext(PlaintextAdvContents<'d>),
    /// Ciphertext advertisements
    Ciphertext(EncryptedAdvContents<'d>),
}
