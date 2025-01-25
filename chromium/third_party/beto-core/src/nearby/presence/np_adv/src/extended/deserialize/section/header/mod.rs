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

//! High-level, early-stage parsed structures for a section: the header, then everything else.

use crate::extended::{
    deserialize, salt::ShortV1Salt, V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN,
    V1_ENCODING_ENCRYPTED_MIC_WITH_SHORT_SALT_AND_TOKEN,
    V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN, V1_ENCODING_UNENCRYPTED,
    V1_IDENTITY_TOKEN_LEN,
};
use crate::helpers::parse_byte_array;
use nom::{combinator, error, number, sequence};
use np_hkdf::v1_salt::{ExtendedV1Salt, EXTENDED_SALT_LEN};

#[cfg(test)]
mod tests;

#[derive(PartialEq, Eq, Debug)]
pub(crate) enum SectionHeader {
    Unencrypted,
    Encrypted(EncryptedSectionHeader),
}

impl SectionHeader {
    /// Returns the parsed header and the remaining length of the section contents
    ///
    /// This structure makes it easy to get a slice of the entire header with
    /// [combinator::consumed] for later inclusion in signatures etc, but also
    /// to [nom::bytes::complete::take] the rest of the section with a minimum of
    /// error-prone length calculations.
    pub(crate) fn parse(input: &[u8]) -> nom::IResult<&[u8], (&[u8], Self, u8)> {
        // Consume first 1-2 bytes of format
        // 0bERRRSSSS first header byte
        let (input, (format_bytes, (first_header_byte, maybe_second_byte))) =
            combinator::consumed(nom::branch::alt((
                // Extended bit not set, verify all reserved bits are 0
                combinator::map(
                    combinator::verify(number::complete::u8, |b| {
                        !deserialize::hi_bit_set(*b) && (*b & 0x70) == 0
                    }),
                    |b| (b, None),
                ),
                // Extended bit is set, take another byte and verify all reserved bits are 0
                combinator::map(
                    nom::sequence::pair(
                        combinator::verify(number::complete::u8, |b| {
                            deserialize::hi_bit_set(*b) && (*b & 0x70) == 0
                        }),
                        combinator::verify(number::complete::u8, |second_byte| *second_byte == 0u8),
                    ),
                    |(b1, b2)| (b1, Some(b2)),
                ),
            )))(input)?;

        let encoding = first_header_byte & 0x0F;

        let (input, section_header) = match (encoding, maybe_second_byte) {
            (V1_ENCODING_UNENCRYPTED, None) => Ok((input, (SectionHeader::Unencrypted))),
            (V1_ENCODING_ENCRYPTED_MIC_WITH_SHORT_SALT_AND_TOKEN, None) => combinator::map(
                sequence::tuple((ShortV1Salt::parse, CiphertextExtendedIdentityToken::parse)),
                |(salt, token)| {
                    SectionHeader::Encrypted(EncryptedSectionHeader::MicShortSalt { salt, token })
                },
            )(input),
            (V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN, None) => combinator::map(
                sequence::tuple((parse_v1_extended_salt, CiphertextExtendedIdentityToken::parse)),
                |(salt, token)| {
                    SectionHeader::Encrypted(EncryptedSectionHeader::MicExtendedSalt {
                        salt,
                        token,
                    })
                },
            )(input),
            (V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN, None) => {
                combinator::map(
                    sequence::tuple((
                        parse_v1_extended_salt,
                        CiphertextExtendedIdentityToken::parse,
                    )),
                    |(salt, token)| {
                        SectionHeader::Encrypted(EncryptedSectionHeader::SigExtendedSalt {
                            salt,
                            token,
                        })
                    },
                )(input)
            }
            _ => Err(nom::Err::Error(error::Error::new(input, error::ErrorKind::Alt))),
        }?;

        //finally parse the section payload length, this is the same regardless of encoding scheme
        let (input, section_len) = combinator::verify(number::complete::u8, |b| {
            // length cannot be 0 for an unencrypted section as this is meaningless
            !(encoding == V1_ENCODING_UNENCRYPTED && *b == 0u8)
        })(input)?;

        Ok((input, (format_bytes, section_header, section_len)))
    }
}

fn parse_v1_extended_salt(input: &[u8]) -> nom::IResult<&[u8], ExtendedV1Salt> {
    combinator::map(parse_byte_array::<{ EXTENDED_SALT_LEN }>, ExtendedV1Salt::from)(input)
}

#[allow(clippy::enum_variant_names)]
#[derive(PartialEq, Eq, Debug)]
pub(crate) enum EncryptedSectionHeader {
    MicShortSalt { salt: ShortV1Salt, token: CiphertextExtendedIdentityToken },
    MicExtendedSalt { salt: ExtendedV1Salt, token: CiphertextExtendedIdentityToken },
    SigExtendedSalt { salt: ExtendedV1Salt, token: CiphertextExtendedIdentityToken },
}

/// 16-byte identity token, straight out of the section.
///
/// If identity resolution succeeds, decrypted to an [ExtendedIdentityToken](crate::extended::V1IdentityToken).
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub(crate) struct CiphertextExtendedIdentityToken(pub(crate) [u8; V1_IDENTITY_TOKEN_LEN]);

impl CiphertextExtendedIdentityToken {
    pub(crate) fn parse(input: &[u8]) -> nom::IResult<&[u8], Self> {
        combinator::map(parse_byte_array::<V1_IDENTITY_TOKEN_LEN>, Self)(input)
    }
}

impl From<[u8; V1_IDENTITY_TOKEN_LEN]> for CiphertextExtendedIdentityToken {
    fn from(value: [u8; V1_IDENTITY_TOKEN_LEN]) -> Self {
        Self(value)
    }
}
