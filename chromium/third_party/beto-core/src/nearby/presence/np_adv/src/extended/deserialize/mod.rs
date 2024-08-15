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
extern crate alloc;

#[cfg(any(test, feature = "alloc", feature = "devtools"))]
use alloc::vec::Vec;
use core::array::TryFromSliceError;

use core::fmt::Debug;
use nom::{branch, bytes, combinator, error, multi, number, sequence};
use strum::IntoEnumIterator as _;

use array_view::ArrayView;
use crypto_provider::CryptoProvider;
use np_hkdf::v1_salt::{self, V1Salt};

use crate::array_vec::ArrayVecOption;
#[cfg(any(feature = "devtools", test))]
use crate::credential::v1::V1DiscoveryCryptoMaterial;
use crate::credential::v1::V1;
use crate::deserialization_arena::ArenaOutOfSpace;
#[cfg(any(feature = "devtools", test))]
use crate::deserialization_arena::DeserializationArenaAllocator;
#[cfg(test)]
use crate::extended::deserialize::encrypted_section::IdentityResolutionOrDeserializationError;
use crate::extended::{NP_V1_ADV_MAX_ENCRYPTED_SECTION_COUNT, NP_V1_ADV_MAX_PUBLIC_SECTION_COUNT};
use crate::{
    de_type::{EncryptedIdentityDataElementType, IdentityDataElementType},
    extended::{
        data_elements::{MIC_ENCRYPTION_SCHEME, SIGNATURE_ENCRYPTION_SCHEME},
        de_type::{DeType, ExtendedDataElementType as _},
        deserialize::encrypted_section::{
            DeserializationError, EncryptedSectionContents, MicEncryptedSection,
            MicVerificationError, SignatureEncryptedSection, SignatureVerificationError,
        },
        DeLength, ENCRYPTION_INFO_DE_TYPE, METADATA_KEY_LEN, NP_ADV_MAX_SECTION_LEN,
    },
    HasIdentityMatch, MetadataKey, PlaintextIdentityMode, V1Header,
};

pub(crate) mod encrypted_section;

#[cfg(test)]
mod parse_tests;

#[cfg(test)]
mod section_tests;

#[cfg(test)]
mod test_stubs;

/// Parse into [IntermediateSection]s, exposing the underlying parsing errors.
/// Consumes all of `adv_body`.
pub(crate) fn parse_sections(
    adv_header: V1Header,
    adv_body: &[u8],
) -> Result<
    ArrayVecOption<IntermediateSection, NP_V1_ADV_MAX_ENCRYPTED_SECTION_COUNT>,
    nom::Err<error::Error<&[u8]>>,
> {
    combinator::all_consuming(branch::alt((
        // Public advertisement
        multi::fold_many_m_n(
            1,
            NP_V1_ADV_MAX_PUBLIC_SECTION_COUNT,
            IntermediateSection::parser_public_section(),
            ArrayVecOption::default,
            |mut acc, item| {
                acc.push(item);
                acc
            },
        ),
        // Encrypted advertisement
        multi::fold_many_m_n(
            1,
            NP_V1_ADV_MAX_ENCRYPTED_SECTION_COUNT,
            IntermediateSection::parser_encrypted_with_header(adv_header),
            ArrayVecOption::default,
            |mut acc, item| {
                acc.push(item);
                acc
            },
        ),
    )))(adv_body)
    .map(|(_rem, sections)| sections)
}

/// A partially processed section that hasn't been decrypted (if applicable) yet.
#[derive(PartialEq, Eq, Debug)]
pub(crate) enum IntermediateSection<'a> {
    /// A section that was not encrypted, e.g. a public identity or no-identity section.
    Plaintext(PlaintextSection<'a>),
    /// A section whose contents were encrypted, e.g. a private identity section.
    Ciphertext(CiphertextSection<'a>),
}

impl<'a> IntermediateSection<'a> {
    fn parser_public_section(
    ) -> impl Fn(&'a [u8]) -> nom::IResult<&'a [u8], IntermediateSection<'a>> {
        move |adv_body| {
            let (remaining, section_contents_len) =
                combinator::verify(number::complete::u8, |sec_len| {
                    *sec_len as usize <= NP_ADV_MAX_SECTION_LEN && *sec_len as usize > 0
                })(adv_body)?;

            // Section structure possibilities:
            // - Public Identity DE, all other DEs
            let parse_public_identity = combinator::map(
                // 1 starting offset because of public identity before it
                sequence::tuple((PublicIdentity::parse, nom::combinator::rest)),
                // move closure to copy section_len into scope
                move |(_identity, rest)| {
                    IntermediateSection::Plaintext(PlaintextSection::new(
                        PlaintextIdentityMode::Public,
                        SectionContents::new(
                            /* section_header */ section_contents_len,
                            rest,
                            1,
                        ),
                    ))
                },
            );
            combinator::map_parser(
                bytes::complete::take(section_contents_len),
                // Guarantee we consume all of the section (not all of the adv body)
                // Note: `all_consuming` should never fail, since we used the `rest` parser above.
                combinator::all_consuming(parse_public_identity),
            )(remaining)
        }
    }

    fn parser_encrypted_with_header(
        adv_header: V1Header,
    ) -> impl Fn(&'a [u8]) -> nom::IResult<&[u8], IntermediateSection> {
        move |adv_body| {
            let (remaining, section_contents_len) =
                combinator::verify(number::complete::u8, |sec_len| {
                    *sec_len as usize <= NP_ADV_MAX_SECTION_LEN && *sec_len as usize > 0
                })(adv_body)?;

            // Section structure possibilities:
            // - Encryption information, non-public Identity header, ciphertext
            // - Encryption information, non-public Identity header, ciphertext, MIC

            let parse_encrypted_identity = combinator::map(
                sequence::tuple((
                    EncryptionInfo::parse_signature,
                    combinator::verify(
                        combinator::consumed(sequence::tuple((
                            EncryptedIdentityMetadata::parser_at_offset(
                                v1_salt::DataElementOffset::from(1),
                            ),
                            combinator::rest,
                        ))),
                        // should be trivially true since section length was checked above,
                        // but this is an invariant for EncryptedSection, so we double check
                        |(identity_and_ciphertext, _tuple)| {
                            (METADATA_KEY_LEN..=NP_ADV_MAX_SECTION_LEN)
                                .contains(&identity_and_ciphertext.len())
                        },
                    ),
                )),
                move |(encryption_info, (identity_and_ciphertext, (identity, _des_ciphertext)))| {
                    // skip identity de header -- rest of that de is ciphertext
                    let to_skip = identity.header_bytes.len();
                    IntermediateSection::Ciphertext(CiphertextSection::SignatureEncryptedIdentity(
                        SignatureEncryptedSection {
                            contents: EncryptedSectionContents::new(
                                adv_header,
                                section_contents_len,
                                encryption_info,
                                identity,
                                &identity_and_ciphertext[to_skip..],
                            ),
                        },
                    ))
                },
            );

            let parse_mic_encrypted_identity = combinator::map(
                sequence::tuple((
                    EncryptionInfo::parse_mic,
                    combinator::verify(
                        combinator::consumed(sequence::tuple((
                            EncryptedIdentityMetadata::parser_at_offset(
                                v1_salt::DataElementOffset::from(1),
                            ),
                            combinator::rest,
                        ))),
                        // Should be trivially true since section length was checked above,
                        // but this is an invariant for MicEncryptedSection, so we double check.
                        // Also verify that there is enough space at the end to contain a valid-length MIC.
                        |(identity_ciphertext_and_mic, _tuple)| {
                            (METADATA_KEY_LEN + SectionMic::CONTENTS_LEN..=NP_ADV_MAX_SECTION_LEN)
                                .contains(&identity_ciphertext_and_mic.len())
                        },
                    ),
                )),
                move |(
                    encryption_info,
                    (identity_ciphertext_and_mic, (identity, _ciphertext_and_mic)),
                )| {
                    // should not panic since we have already ensured a valid length
                    let (identity_and_ciphertext, mic) = identity_ciphertext_and_mic
                        .split_at(identity_ciphertext_and_mic.len() - SectionMic::CONTENTS_LEN);
                    // skip identity de header -- rest of that de is ciphertext
                    let to_skip = identity.header_bytes.len();
                    IntermediateSection::Ciphertext(CiphertextSection::MicEncryptedIdentity(
                        MicEncryptedSection {
                            contents: EncryptedSectionContents::new(
                                adv_header,
                                section_contents_len,
                                encryption_info,
                                identity,
                                &identity_and_ciphertext[to_skip..],
                            ),
                            mic: mic.try_into().unwrap_or_else(|_| {
                                panic!("{} is a valid length", SectionMic::CONTENTS_LEN)
                            }),
                        },
                    ))
                },
            );

            combinator::map_parser(
                bytes::complete::take(section_contents_len),
                // guarantee we consume all of the section (not all of the adv body)
                combinator::all_consuming(branch::alt((
                    parse_mic_encrypted_identity,
                    parse_encrypted_identity,
                ))),
            )(remaining)
        }
    }
}

#[derive(PartialEq, Eq, Debug)]
struct SectionContents<'adv> {
    section_header: u8,
    de_region_excl_identity: &'adv [u8],
    data_element_start_offset: u8,
}

impl<'adv> SectionContents<'adv> {
    fn new(
        section_header: u8,
        de_region_excl_identity: &'adv [u8],
        data_element_start_offset: u8,
    ) -> Self {
        Self { section_header, de_region_excl_identity, data_element_start_offset }
    }

    fn iter_data_elements(&self) -> DataElementParsingIterator<'adv> {
        DataElementParsingIterator {
            input: self.de_region_excl_identity,
            offset: self.data_element_start_offset,
        }
    }
}

/// A section deserialized from a V1 advertisement.
pub trait Section<'adv, E: Debug> {
    /// The iterator type used to iterate over data elements
    type Iterator: Iterator<Item = Result<DataElement<'adv>, E>>;

    /// Iterator over the data elements in a section, except for any DEs related to resolving the
    /// identity or otherwise validating the payload (e.g. MIC, Signature, any identity DEs like
    /// Private Identity).
    fn iter_data_elements(&self) -> Self::Iterator;

    /// Collects the data elements into a vector, eagerly catching and resolving any errors during
    /// parsing.
    #[cfg(any(test, feature = "alloc"))]
    fn collect_data_elements(&self) -> Result<Vec<DataElement<'adv>>, E>
    where
        Self: Sized,
    {
        self.iter_data_elements().collect()
    }
}

/// A plaintext section deserialized from a V1 advertisement.
#[derive(PartialEq, Eq, Debug)]
pub struct PlaintextSection<'adv> {
    identity: PlaintextIdentityMode,
    contents: SectionContents<'adv>,
}

impl<'adv> PlaintextSection<'adv> {
    fn new(identity: PlaintextIdentityMode, contents: SectionContents<'adv>) -> Self {
        Self { identity, contents }
    }

    /// The identity mode for the section.
    ///
    /// Since plaintext sections do not use encryption, they cannot be matched to a single identity,
    /// and only have a mode (no identity or public).
    pub fn identity(&self) -> PlaintextIdentityMode {
        self.identity
    }
}

impl<'adv> Section<'adv, DataElementParseError> for PlaintextSection<'adv> {
    type Iterator = DataElementParsingIterator<'adv>;

    fn iter_data_elements(&self) -> Self::Iterator {
        self.contents.iter_data_elements()
    }
}

/// A byte buffer the size of a V1 salt.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct RawV1Salt([u8; 16]);

impl RawV1Salt {
    /// Returns the raw salt bytes as a vec.
    #[cfg(feature = "devtools")]
    pub fn to_vec(&self) -> Vec<u8> {
        self.0.to_vec()
    }
}

impl<C: CryptoProvider> From<RawV1Salt> for V1Salt<C> {
    fn from(raw_salt: RawV1Salt) -> Self {
        raw_salt.0.into()
    }
}

impl<C: CryptoProvider> From<V1Salt<C>> for RawV1Salt {
    fn from(salt: V1Salt<C>) -> Self {
        RawV1Salt(salt.into_array())
    }
}

/// Fully-parsed and verified decrypted contents from an encrypted section.
#[derive(Debug, PartialEq, Eq)]
pub struct DecryptedSection<'adv> {
    identity_type: EncryptedIdentityDataElementType,
    verification_mode: VerificationMode,
    metadata_key: MetadataKey,
    salt: RawV1Salt,
    contents: SectionContents<'adv>,
}

impl<'adv> DecryptedSection<'adv> {
    fn new(
        identity_type: EncryptedIdentityDataElementType,
        verification_mode: VerificationMode,
        metadata_key: MetadataKey,
        salt: RawV1Salt,
        contents: SectionContents<'adv>,
    ) -> Self {
        Self { identity_type, verification_mode, metadata_key, salt, contents }
    }

    /// The type of identity DE used in the section.
    pub fn identity_type(&self) -> EncryptedIdentityDataElementType {
        self.identity_type
    }

    /// The verification mode used in the section.
    pub fn verification_mode(&self) -> VerificationMode {
        self.verification_mode
    }

    /// The salt used for decryption of this advertisement.
    pub fn salt(&self) -> RawV1Salt {
        self.salt
    }
}

impl<'adv> HasIdentityMatch for DecryptedSection<'adv> {
    type Version = V1;
    fn metadata_key(&self) -> MetadataKey {
        self.metadata_key
    }
}

impl<'adv> Section<'adv, DataElementParseError> for DecryptedSection<'adv> {
    type Iterator = DataElementParsingIterator<'adv>;

    fn iter_data_elements(&self) -> Self::Iterator {
        self.contents.iter_data_elements()
    }
}

#[derive(PartialEq, Eq, Debug)]
pub(crate) enum CiphertextSection<'a> {
    SignatureEncryptedIdentity(SignatureEncryptedSection<'a>),
    MicEncryptedIdentity(MicEncryptedSection<'a>),
}

impl<'a> CiphertextSection<'a> {
    /// Tries to match this section's identity using the given crypto-material,
    /// and if successful, tries to decrypt this section.
    #[cfg(test)]
    pub(crate) fn try_resolve_identity_and_deserialize<C, P>(
        &'a self,
        allocator: &mut DeserializationArenaAllocator<'a>,
        crypto_material: &C,
    ) -> Result<DecryptedSection<'a>, SectionDeserializeError>
    where
        C: V1DiscoveryCryptoMaterial,
        P: CryptoProvider,
    {
        match self {
            CiphertextSection::SignatureEncryptedIdentity(contents) => {
                let identity_resolution_material =
                    crypto_material.signed_identity_resolution_material::<P>();
                let verification_material = crypto_material.signed_verification_material::<P>();

                contents
                    .try_resolve_identity_and_deserialize::<P>(
                        allocator,
                        &identity_resolution_material,
                        &verification_material,
                    )
                    .map_err(|e| e.into())
            }
            CiphertextSection::MicEncryptedIdentity(contents) => {
                let identity_resolution_material =
                    crypto_material.unsigned_identity_resolution_material::<P>();
                let verification_material = crypto_material.unsigned_verification_material::<P>();

                contents
                    .try_resolve_identity_and_deserialize::<P>(
                        allocator,
                        &identity_resolution_material,
                        &verification_material,
                    )
                    .map_err(|e| e.into())
            }
        }
    }

    /// Try decrypting into some raw bytes given some raw unsigned crypto-material.
    #[cfg(feature = "devtools")]
    pub(crate) fn try_resolve_identity_and_decrypt<
        C: V1DiscoveryCryptoMaterial,
        P: CryptoProvider,
    >(
        &self,
        allocator: &mut DeserializationArenaAllocator<'a>,
        crypto_material: &C,
    ) -> Option<Result<ArrayView<u8, NP_ADV_MAX_SECTION_LEN>, ArenaOutOfSpace>> {
        match self {
            CiphertextSection::SignatureEncryptedIdentity(x) => {
                let identity_resolution_material =
                    crypto_material.signed_identity_resolution_material::<P>();
                x.try_resolve_identity_and_decrypt::<P>(allocator, &identity_resolution_material)
            }
            CiphertextSection::MicEncryptedIdentity(x) => {
                let identity_resolution_material =
                    crypto_material.unsigned_identity_resolution_material::<P>();
                x.try_resolve_identity_and_decrypt::<P>(allocator, &identity_resolution_material)
            }
        }
    }

    pub(crate) fn contents(&self) -> &EncryptedSectionContents<'a> {
        match self {
            CiphertextSection::SignatureEncryptedIdentity(x) => &x.contents,
            CiphertextSection::MicEncryptedIdentity(x) => &x.contents,
        }
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

#[cfg(test)]
impl From<IdentityResolutionOrDeserializationError<MicVerificationError>>
    for SectionDeserializeError
{
    fn from(error: IdentityResolutionOrDeserializationError<MicVerificationError>) -> Self {
        match error {
            IdentityResolutionOrDeserializationError::IdentityMatchingError => {
                Self::IncorrectCredential
            }
            IdentityResolutionOrDeserializationError::DeserializationError(e) => e.into(),
        }
    }
}

#[cfg(test)]
impl From<IdentityResolutionOrDeserializationError<SignatureVerificationError>>
    for SectionDeserializeError
{
    fn from(error: IdentityResolutionOrDeserializationError<SignatureVerificationError>) -> Self {
        match error {
            IdentityResolutionOrDeserializationError::IdentityMatchingError => {
                Self::IncorrectCredential
            }
            IdentityResolutionOrDeserializationError::DeserializationError(e) => e.into(),
        }
    }
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

/// An iterator that parses the given data elements iteratively. In environments where memory is
/// not severely constrained, it is usually safer to collect this into `Result<Vec<DataElement>>`
/// so the validity of the whole advertisement can be checked before proceeding with further
/// processing.
#[derive(Debug)]
pub struct DataElementParsingIterator<'adv> {
    input: &'adv [u8],
    // The index of the data element this is currently at
    offset: u8,
}

/// The error that may arise while parsing data elements.
#[derive(Debug, PartialEq, Eq)]
pub enum DataElementParseError {
    /// Only one identity data element is allowed in an advertisement, but a duplicate is found
    /// while parsing.
    DuplicateIdentityDataElement,
    /// Unexpected data found after the end of the data elements portion. This means either the
    /// parser was fed with additional data (it should only be given the bytes within a section,
    /// not the whole advertisement), or the length field in the header of the data element is
    /// malformed.
    UnexpectedDataAfterEnd,
    /// There are too many data elements in the advertisement. The maximum number supported by the
    /// current parsing logic is 255.
    TooManyDataElements,
    /// A parse error is returned during nom.
    NomError(nom::error::ErrorKind),
}

impl<'adv> Iterator for DataElementParsingIterator<'adv> {
    type Item = Result<DataElement<'adv>, DataElementParseError>;

    fn next(&mut self) -> Option<Self::Item> {
        match ProtoDataElement::parse(self.input) {
            Ok((rem, pde)) => {
                if !IdentityDataElementType::iter().all(|t| t.type_code() != pde.header.de_type) {
                    return Some(Err(DataElementParseError::DuplicateIdentityDataElement));
                }
                self.input = rem;
                let current_offset = self.offset;
                self.offset = if let Some(offset) = self.offset.checked_add(1) {
                    offset
                } else {
                    return Some(Err(DataElementParseError::TooManyDataElements));
                };
                Some(Ok(pde.into_data_element(v1_salt::DataElementOffset::from(current_offset))))
            }
            Err(nom::Err::Failure(e)) => Some(Err(DataElementParseError::NomError(e.code))),
            Err(nom::Err::Incomplete(_)) => {
                panic!("Should always complete since we are parsing using the `nom::complete` APIs")
            }
            Err(nom::Err::Error(_)) => {
                // nom `Error` is recoverable, it usually means we should move on the parsing the
                // next section. There is nothing after data elements within a section, so we just
                // check that there is no remaining data.
                if !self.input.is_empty() {
                    return Some(Err(DataElementParseError::UnexpectedDataAfterEnd));
                }
                None
            }
        }
    }
}

/// Deserialize-specific version of a DE header that incorporates the header length.
/// This is needed for encrypted identities that need to construct a slice of everything in the
/// section following the identity DE header.
#[derive(Debug, PartialEq, Eq, Clone)]
pub(crate) struct DeHeader {
    /// The original bytes of the header, at most 6 bytes long (1 byte len, 5 bytes type)
    pub(crate) header_bytes: ArrayView<u8, 6>,
    pub(crate) de_type: DeType,
    pub(crate) contents_len: DeLength,
}

impl DeHeader {
    pub(crate) fn parse(input: &[u8]) -> nom::IResult<&[u8], DeHeader> {
        // 1-byte header: 0b0LLLTTTT
        let parse_single_byte_de_header =
            combinator::map_opt::<&[u8], _, DeHeader, error::Error<&[u8]>, _, _>(
                combinator::consumed(combinator::map_res(
                    combinator::verify(number::complete::u8, |&b| !hi_bit_set(b)),
                    |b| {
                        // L bits
                        let len = (b >> 4) & 0x07;
                        // T bits
                        let de_type = ((b & 0x0F) as u32).into();

                        len.try_into().map(|l| (l, de_type))
                    },
                )),
                |(header_bytes, (len, de_type))| {
                    ArrayView::try_from_slice(header_bytes).map(|header_bytes| DeHeader {
                        header_bytes,
                        contents_len: len,
                        de_type,
                    })
                },
            );

        // multi-byte headers: 0b1LLLLLLL (0b1TTTTTTT)* 0b0TTTTTTT
        // leading 1 in first byte = multibyte format
        // leading 1 in subsequent bytes = there is at least 1 more type bytes
        // leading 0 = this is the last header byte
        // 127-bit length, effectively infinite type bit length

        // It's conceivable to have non-canonical extended type sequences where 1 or more leading
        // bytes don't have any bits set (other than the marker hi bit), thereby contributing nothing
        // to the final value.
        // To prevent that, we require that either there be only 1 type byte, or that the first of the
        // multiple type bytes must have a value bit set. It's OK to have no value bits in subsequent
        // type bytes.

        let parse_ext_de_header = combinator::map_opt(
            combinator::consumed(sequence::pair(
                // length byte w/ leading 1
                combinator::map_res(
                    combinator::verify(number::complete::u8::<&[u8], _>, |&b| hi_bit_set(b)),
                    // snag the lower 7 bits
                    |b| (b & 0x7F).try_into(),
                ),
                branch::alt((
                    // 1 type byte case
                    combinator::recognize(
                        // 0-hi-bit type code byte
                        combinator::verify(number::complete::u8, |&b| !hi_bit_set(b)),
                    ),
                    // multiple type byte case: leading type byte must have at least 1 value bit
                    combinator::recognize(sequence::tuple((
                        // hi bit and at least 1 value bit, otherwise it would be non-canonical
                        combinator::verify(number::complete::u8, |&b| {
                            hi_bit_set(b) && (b & 0x7F != 0)
                        }),
                        // 0-3 1-hi-bit type code bytes with any bit pattern. Max is 3 since two 7
                        // bit type chunks are processed before and after this, for a total of 5,
                        // and that's as many 7-bit chunks as are needed to support a 32-bit type.
                        bytes::complete::take_while_m_n(0, 3, hi_bit_set),
                        // final 0-hi-bit type code byte
                        combinator::verify(number::complete::u8, |&b| !hi_bit_set(b)),
                    ))),
                )),
            )),
            |(header_bytes, (len, type_bytes))| {
                // snag the low 7 bits of each type byte and accumulate

                type_bytes
                    .iter()
                    .try_fold(0_u32, |accum, b| {
                        accum.checked_shl(7).map(|n| n + ((b & 0x7F) as u32))
                    })
                    .and_then(|type_code| {
                        ArrayView::try_from_slice(header_bytes).map(|header_bytes| DeHeader {
                            header_bytes,
                            contents_len: len,
                            de_type: type_code.into(),
                        })
                    })
            },
        );

        branch::alt((parse_single_byte_de_header, parse_ext_de_header))(input)
    }
}

/// An intermediate stage in parsing a [DataElement] that lacks `offset`.
#[derive(Debug, PartialEq, Eq)]
struct ProtoDataElement<'d> {
    header: DeHeader,
    /// `len()` must equal `header.contents_len`
    contents: &'d [u8],
}

impl<'d> ProtoDataElement<'d> {
    fn parse(input: &[u8]) -> nom::IResult<&[u8], ProtoDataElement> {
        let (remaining, header) = DeHeader::parse(input)?;
        let len = header.contents_len;
        combinator::map(bytes::complete::take(len.as_u8()), move |slice| {
            let header_clone = header.clone();
            ProtoDataElement { header: header_clone, contents: slice }
        })(remaining)
    }

    fn into_data_element(self, offset: v1_salt::DataElementOffset) -> DataElement<'d> {
        DataElement { offset, de_type: self.header.de_type, contents: self.contents }
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

/// The identity used to successfully decrypt and validate an encrypted section
#[derive(Clone, PartialEq, Eq, Debug)]
pub struct EncryptedSectionIdentity {
    identity_type: EncryptedIdentityDataElementType,
    validation_mode: VerificationMode,
    metadata_key: MetadataKey,
}

impl EncryptedSectionIdentity {
    /// The type of identity DE used in the section
    pub fn identity_type(&self) -> EncryptedIdentityDataElementType {
        self.identity_type
    }
    /// The validation mode used when decrypting and verifying the section
    pub fn verification_mode(&self) -> VerificationMode {
        self.validation_mode
    }
    /// The decrypted metadata key from the section's identity DE
    pub fn metadata_key(&self) -> MetadataKey {
        self.metadata_key
    }
}

/// A deserialized data element in a section.
///
/// The DE has been processed to the point of exposing a DE type and its contents as a `&[u8]`, but
/// no DE-type-specific processing has been performed.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DataElement<'adv> {
    offset: v1_salt::DataElementOffset,
    de_type: DeType,
    contents: &'adv [u8],
}

impl<'adv> DataElement<'adv> {
    /// The offset of the DE in its containing Section.
    ///
    /// Used with the section salt to derive per-DE salt.
    pub fn offset(&self) -> v1_salt::DataElementOffset {
        self.offset
    }
    /// The type of the DE
    pub fn de_type(&self) -> DeType {
        self.de_type
    }
    /// The contents of the DE
    pub fn contents(&self) -> &'adv [u8] {
        self.contents
    }
}

#[derive(PartialEq, Eq, Debug, Clone)]
pub(crate) struct EncryptionInfo {
    pub bytes: [u8; 19],
}

impl EncryptionInfo {
    // 2-byte header, 1-byte encryption scheme, 16-byte salt
    pub(crate) const TOTAL_DE_LEN: usize = 19;
    const CONTENTS_LEN: usize = 17;
    const ENCRYPTION_INFO_SCHEME_MASK: u8 = 0b01111000;

    fn parse_signature(input: &[u8]) -> nom::IResult<&[u8], EncryptionInfo> {
        Self::parser_for_scheme(SIGNATURE_ENCRYPTION_SCHEME)(input)
    }

    fn parse_mic(input: &[u8]) -> nom::IResult<&[u8], EncryptionInfo> {
        Self::parser_for_scheme(MIC_ENCRYPTION_SCHEME)(input)
    }

    fn parser_for_scheme(
        expected_scheme: u8,
    ) -> impl Fn(&[u8]) -> nom::IResult<&[u8], EncryptionInfo> {
        move |input| {
            combinator::map_res(
                combinator::consumed(combinator::map_parser(
                    combinator::map(
                        combinator::verify(ProtoDataElement::parse, |de| {
                            de.header.de_type == ENCRYPTION_INFO_DE_TYPE
                                && de.contents.len() == Self::CONTENTS_LEN
                        }),
                        |de| de.contents,
                    ),
                    sequence::tuple((
                        combinator::verify(number::complete::be_u8, |scheme: &u8| {
                            expected_scheme == (Self::ENCRYPTION_INFO_SCHEME_MASK & scheme)
                        }),
                        bytes::complete::take(16_usize),
                    )),
                )),
                |(bytes, _contents)| bytes.try_into(),
            )(input)
        }
    }

    fn salt(&self) -> RawV1Salt {
        RawV1Salt(
            self.bytes[Self::TOTAL_DE_LEN - 16..]
                .try_into()
                .expect("a 16 byte slice will always fit into a 16 byte array"),
        )
    }
}

impl TryFrom<&[u8]> for EncryptionInfo {
    type Error = TryFromSliceError;

    fn try_from(value: &[u8]) -> Result<Self, Self::Error> {
        value.try_into().map(|fixed_bytes: [u8; 19]| Ok(Self { bytes: fixed_bytes }))?
    }
}

#[derive(PartialEq, Eq, Debug)]
pub(crate) struct SectionMic {
    mic: [u8; 16],
}

impl SectionMic {
    // 16-byte metadata key
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

#[derive(PartialEq, Eq, Debug)]
struct PublicIdentity;

impl PublicIdentity {
    fn parse(input: &[u8]) -> nom::IResult<&[u8], PublicIdentity> {
        combinator::map(
            combinator::verify(DeHeader::parse, |deh| {
                deh.de_type == IdentityDataElementType::Public.type_code()
                    && deh.contents_len.as_u8() == 0
            }),
            |_| PublicIdentity,
        )(input)
    }
}

/// Parsed form of an encrypted identity DE before its contents are decrypted.
/// Metadata key is stored in the enclosing section.
#[derive(PartialEq, Eq, Debug)]
pub(crate) struct EncryptedIdentityMetadata {
    pub(crate) offset: v1_salt::DataElementOffset,
    /// The original DE header from the advertisement.
    /// Encrypted identity should always be a len=2 header.
    pub(crate) header_bytes: [u8; 2],
    pub(crate) identity_type: EncryptedIdentityDataElementType,
}

impl EncryptedIdentityMetadata {
    // 2-byte header, 16-byte metadata key
    pub(crate) const TOTAL_DE_LEN: usize = 18;

    /// Returns a parser function that parses an [`EncryptedIdentityMetadata`] using the provided DE `offset`.
    fn parser_at_offset(
        offset: v1_salt::DataElementOffset,
    ) -> impl Fn(&[u8]) -> nom::IResult<&[u8], EncryptedIdentityMetadata> {
        move |input| {
            combinator::map_opt(ProtoDataElement::parse, |de| {
                EncryptedIdentityDataElementType::from_type_code(de.header.de_type).and_then(
                    |identity_type| {
                        de.header.header_bytes.as_slice().try_into().ok().and_then(|header_bytes| {
                            de.contents.try_into().ok().map(|_metadata_key_ciphertext: [u8; 16]| {
                                // ensure the ciphertext is the right size, then discard
                                EncryptedIdentityMetadata { header_bytes, offset, identity_type }
                            })
                        })
                    },
                )
            })(input)
        }
    }
}

fn hi_bit_set(b: u8) -> bool {
    b & 0x80 > 0
}
