// Copyright 2024 Google LLC
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

//! Parsing logic for V1 data elements, header + contents

use crate::extended::{de_requires_extended_bit, de_type::DeType, deserialize, DeLength};
use array_view::ArrayView;
use core::fmt;
use nom::{branch, bytes, combinator, error, number, sequence};
use np_hkdf::v1_salt;

#[cfg(test)]
mod tests;

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

    pub(crate) fn new(
        offset: v1_salt::DataElementOffset,
        de_type: DeType,
        contents: &'adv [u8],
    ) -> Self {
        Self { offset, de_type, contents }
    }

    /// Exposes the ability to create a DE for testing purposes, real clients should only obtain
    /// one by deserializing an advertisement
    #[cfg(feature = "testing")]
    pub fn new_for_testing(
        offset: v1_salt::DataElementOffset,
        de_type: DeType,
        contents: &'adv [u8],
    ) -> Self {
        Self { offset, de_type, contents }
    }
}

impl DeHeader {
    pub(crate) fn parse(input: &[u8]) -> nom::IResult<&[u8], DeHeader> {
        // 1-byte header: 0b0LLLTTTT
        let parse_single_byte_de_header =
            combinator::map_opt::<&[u8], _, DeHeader, error::Error<&[u8]>, _, _>(
                combinator::consumed(combinator::map_res(
                    combinator::verify(number::complete::u8, |&b| !deserialize::hi_bit_set(b)),
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

        let parse_ext_de_header = combinator::verify(
            combinator::map_opt(
                combinator::consumed(sequence::pair(
                    // length byte w/ leading 1
                    combinator::map_res(
                        combinator::verify(number::complete::u8::<&[u8], _>, |&b| {
                            deserialize::hi_bit_set(b)
                        }),
                        // snag the lower 7 bits
                        |b| (b & 0x7F).try_into(),
                    ),
                    branch::alt((
                        // 1 type byte case
                        combinator::recognize(
                            // 0-hi-bit type code byte
                            combinator::verify(number::complete::u8, |&b| {
                                !deserialize::hi_bit_set(b)
                            }),
                        ),
                        // multiple type byte case: leading type byte must have at least 1 value bit
                        combinator::recognize(sequence::tuple((
                            // hi bit and at least 1 value bit, otherwise it would be non-canonical
                            combinator::verify(number::complete::u8, |&b| {
                                deserialize::hi_bit_set(b) && (b & 0x7F != 0)
                            }),
                            // 0-3 1-hi-bit type code bytes with any bit pattern. Max is 3 since two 7
                            // bit type chunks are processed before and after this, for a total of 5,
                            // and that's as many 7-bit chunks as are needed to support a 32-bit type.
                            bytes::complete::take_while_m_n(0, 3, deserialize::hi_bit_set),
                            // final 0-hi-bit type code byte
                            combinator::verify(number::complete::u8, |&b| {
                                !deserialize::hi_bit_set(b)
                            }),
                        ))),
                    )),
                )),
                |(header_bytes, (len, type_bytes))| {
                    // snag the low 7 bits of each type byte and accumulate
                    type_bytes
                        .iter()
                        .try_fold(0_u64, |accum, b| {
                            accum.checked_shl(7).map(|n| n + ((b & 0x7F) as u64))
                        })
                        .and_then(|type_code| u32::try_from(type_code).ok())
                        .and_then(|type_code| {
                            ArrayView::try_from_slice(header_bytes).map(|header_bytes| DeHeader {
                                header_bytes,
                                contents_len: len,
                                de_type: type_code.into(),
                            })
                        })
                },
            ),
            |header| {
                // verify that the length and type code actually require use of the extended bit
                de_requires_extended_bit(header.de_type.as_u32(), header.contents_len.len)
            },
        );

        branch::alt((parse_single_byte_de_header, parse_ext_de_header))(input)
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

impl<'adv> DataElementParsingIterator<'adv> {
    pub(crate) fn new(input: &'adv [u8]) -> Self {
        Self { input, offset: 0 }
    }
}

impl<'adv> Iterator for DataElementParsingIterator<'adv> {
    type Item = Result<DataElement<'adv>, DataElementParseError>;

    fn next(&mut self) -> Option<Self::Item> {
        match ProtoDataElement::parse(self.input) {
            Ok((rem, pde)) => {
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

/// The error that may arise while parsing data elements.
#[derive(Debug, PartialEq, Eq)]
pub enum DataElementParseError {
    /// Unexpected data found after the end of the data elements portion. This means either the
    /// parser was fed with additional data (it should only be given the bytes within a section,
    /// not the whole advertisement), or the length field in the header of the data element is
    /// malformed.
    UnexpectedDataAfterEnd,
    /// There are too many data elements in the advertisement. The maximum number supported by the
    /// current parsing logic is 255.
    TooManyDataElements,
    /// A parse error is returned during nom.
    NomError(error::ErrorKind),
}

impl fmt::Display for DataElementParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DataElementParseError::UnexpectedDataAfterEnd => write!(f, "Unexpected data after end"),
            DataElementParseError::TooManyDataElements => write!(f, "Too many data elements"),
            DataElementParseError::NomError(_) => write!(f, "Nom error"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for DataElementParseError {}

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

/// An intermediate stage in parsing a [DataElement] that lacks `offset`.
#[derive(Debug, PartialEq, Eq)]
pub struct ProtoDataElement<'d> {
    header: DeHeader,
    /// `len()` must equal `header.contents_len`
    contents: &'d [u8],
}

impl<'d> ProtoDataElement<'d> {
    pub(crate) fn parse(input: &[u8]) -> nom::IResult<&[u8], ProtoDataElement> {
        let (remaining, header) = DeHeader::parse(input)?;
        let len = header.contents_len;
        combinator::map(bytes::complete::take(len.as_u8()), move |slice| {
            let header_clone = header.clone();
            ProtoDataElement { header: header_clone, contents: slice }
        })(remaining)
    }

    fn into_data_element(self, offset: v1_salt::DataElementOffset) -> DataElement<'d> {
        DataElement::new(offset, self.header.de_type, self.contents)
    }
}
