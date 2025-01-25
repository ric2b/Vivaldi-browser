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

#![allow(clippy::unwrap_used)]

use super::*;
use alloc::{vec, vec::Vec};

#[test]
fn parse_de_single_byte_header_length_overrun() {
    // length 7, type 0x03
    let input = [0b0111_0011, 0x01, 0x02];
    assert_eq!(
        nom::Err::Error(error::Error {
            // attempted to read DE contents
            input: &input.as_slice()[1..],
            code: error::ErrorKind::Eof,
        }),
        ProtoDataElement::parse(&input).unwrap_err()
    );
}

#[test]
fn parse_de_multi_byte_header_length_overrun() {
    let input = [0b1000_0111, 0x1F, 0x01, 0x02];
    assert_eq!(
        nom::Err::Error(error::Error {
            // attempted to read DE contents
            input: &input.as_slice()[2..],
            code: error::ErrorKind::Eof,
        }),
        ProtoDataElement::parse(&input).unwrap_err()
    );
}

#[test]
fn parse_de_with_1_byte_header() {
    let data = [0x51, 0x01, 0x02, 0x03, 0x04, 0x05, 0xFF, 0xFF];
    assert_eq!(
        Ok((
            &data[6..],
            ProtoDataElement {
                header: DeHeader {
                    de_type: 1_u8.into(),
                    header_bytes: ArrayView::try_from_slice(&[0x51]).unwrap(),
                    contents_len: 5_u8.try_into().unwrap(),
                },
                contents: &data[1..6],
            }
        )),
        ProtoDataElement::parse(&data)
    );
}

#[test]
fn parse_de_with_2_byte_header() {
    let data = [0x85, 0x10, 0x01, 0x02, 0x03, 0x04, 0x05, 0xFF, 0xFF];
    assert_eq!(
        Ok((
            &data[7..],
            ProtoDataElement {
                header: DeHeader {
                    de_type: 16_u8.into(),
                    header_bytes: ArrayView::try_from_slice(&[0x85, 0x10]).unwrap(),
                    contents_len: 5_u8.try_into().unwrap(),
                },
                contents: &data[2..7],
            }
        )),
        ProtoDataElement::parse(&data)
    );
}

#[test]
fn parse_de_with_3_byte_header() {
    let data = [0x85, 0xC1, 0x41, 0x01, 0x02, 0x03, 0x04, 0x05, 0xFF, 0xFF];
    assert_eq!(
        Ok((
            &data[8..],
            ProtoDataElement {
                header: DeHeader {
                    header_bytes: ArrayView::try_from_slice(&[0x85, 0xC1, 0x41]).unwrap(),
                    contents_len: 5_u8.try_into().unwrap(),
                    de_type: 0b0000_0000_0000_0000_0010_0000_1100_0001_u32.into(),
                },
                contents: &data[3..8],
            }
        )),
        ProtoDataElement::parse(&data)
    );
}

#[test]
fn parse_de_header_1_byte() {
    let data = [0x51, 0xFF, 0xFF];
    assert_eq!(
        Ok((
            &data[1..],
            DeHeader {
                de_type: 1_u8.into(),
                contents_len: 5_u8.try_into().unwrap(),
                header_bytes: ArrayView::try_from_slice(&[0x51]).unwrap(),
            }
        )),
        DeHeader::parse(&data)
    );
}

#[test]
fn parse_de_header_2_bytes() {
    let data = [0x88, 0x01];
    assert_eq!(
        Ok((
            &data[2..],
            DeHeader {
                de_type: 1_u8.into(),
                contents_len: 8_u8.try_into().unwrap(),
                header_bytes: ArrayView::try_from_slice(&[0x88, 0x01]).unwrap(),
            }
        )),
        DeHeader::parse(&data)
    );
}

#[test]
fn parse_de_header_3_bytes() {
    let data = [0x83, 0xC1, 0x41, 0xFF, 0xFF];
    assert_eq!(
        Ok((
            &data[3..],
            DeHeader {
                de_type: 0b0000_0000_0000_0000_0010_0000_1100_0001_u32.into(),
                contents_len: 3_u8.try_into().unwrap(),
                header_bytes: ArrayView::try_from_slice(&[0x83, 0xC1, 0x41]).unwrap(),
            }
        )),
        DeHeader::parse(&data)
    );
}

#[test]
fn parse_de_header_4_bytes() {
    let data = [0x83, 0xC1, 0xC1, 0x41, 0xFF, 0xFF];
    assert_eq!(
        Ok((
            &data[4..],
            DeHeader {
                de_type: 0b0000_0000_0001_0000_0110_0000_1100_0001_u32.into(),
                contents_len: 3_u8.try_into().unwrap(),
                header_bytes: ArrayView::try_from_slice(&[0x83, 0xC1, 0xC1, 0x41]).unwrap(),
            }
        )),
        DeHeader::parse(&data)
    );
}

#[test]
fn parse_de_header_max_length_extension() {
    // 1 byte length + 5 bytes of type is the max possible length of an extended DE header.
    // The first 3 bits of type will be discarded by the left shift, so if an extended DE header
    // does make it to 5 bytes, we will only read the least significant 32 bits out of a possible 35.
    // The contents of the most significant 3 bits of type code must be 0's, otherwise the adv will
    // be discarded
    let data = [0x80, 0x8F, 0xFF, 0xFF, 0xFF, 0x7F];
    assert_eq!(
        Ok((
            &data[6..],
            DeHeader {
                de_type: u32::MAX.into(),
                contents_len: 0_u8.try_into().unwrap(),
                header_bytes: ArrayView::try_from_slice(&[0x80, 0x8F, 0xFF, 0xFF, 0xFF, 0x7F])
                    .unwrap(),
            }
        )),
        DeHeader::parse(&data)
    );
}

// In a 6 byte extended de header, if any bits are non-zero in the most 3 significant bits of type,
// the parsing will fail as this would be out of range of a valid u32 value.
#[test]
fn parse_de_6_byte_header_invalid_significant_bits() {
    assert!(DeHeader::parse(&[0x80, 0x9F, 0xFF, 0xFF, 0xFF, 0x7F]).is_err());
    assert!(DeHeader::parse(&[0x80, 0xCF, 0xFF, 0xFF, 0xFF, 0x7F]).is_err());
    assert!(DeHeader::parse(&[0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F]).is_err());
}

#[test]
fn parse_de_header_over_max_length_extension() {
    // last header byte cannot contain a leading 1
    let data = [0x80, 0x8F, 0xFF, 0xFF, 0xFF, 0xFF];
    assert_eq!(
        nom::Err::Error(error::Error {
            // failed parsing the last byte
            input: &data.as_slice()[5..],
            code: error::ErrorKind::Verify,
        }),
        DeHeader::parse(&data).unwrap_err()
    );
}

// Test edge cases which can be represented in fewer bytes
#[test]
fn parse_de_header_invalid_multi_byte_type_code() {
    // extended length and type codes which can be represented in a single byte
    assert!(DeHeader::parse(&[0b1000_0000, 0b000_0000]).is_err());
    assert!(DeHeader::parse(&[0b1000_0000, 0b000_0001]).is_err());
    assert!(DeHeader::parse(&[0b1000_0111, 0b000_1111]).is_err());

    // first byte of type doesn't have any bits in it so it contributes nothing
    assert!(DeHeader::parse(&[0b1000_0111, 0b1000_0000, 0b0100_0000]).is_err());
    // first 2 bytes of type are unnecessary
    assert!(DeHeader::parse(&[0b1000_0001, 0b1000_0000, 0b1000_0000, 0b1100_0000]).is_err());

    // extended bit set with no following byte
    assert!(DeHeader::parse(&[0b1000_0001, 0b1000_0000]).is_err());

    // needs one extra bit of length so must use extended
    assert!(DeHeader::parse(&[0b100_1111, 0b000_1111]).is_ok());

    // needs one extra bit of type so must use extended
    assert!(DeHeader::parse(&[0b100_0111, 0b001_1111]).is_ok());

    // valid to trail with all 0's for a larger type code
    assert!(DeHeader::parse(&[0b100_1111, 0b1000_0001, 0b0000_0000]).is_ok());

    // valid to trail with all 0's in middle of type code
    assert!(DeHeader::parse(&[0b100_1111, 0b1000_0001, 0b1000_0000, 0b0000_0001]).is_ok());
}

#[test]
fn de_iteration_exposes_correct_data() {
    let mut de_data = vec![];
    // de 1 byte header, type 5, len 5
    de_data.extend_from_slice(&[0x55, 0x01, 0x02, 0x03, 0x04, 0x05]);
    // de 2 byte header, type 16, len 1
    de_data.extend_from_slice(&[0x81, 0x10, 0x01]);

    let iterator = DataElementParsingIterator::new(&de_data);
    let des = iterator.collect::<Result<Vec<_>, _>>().unwrap();

    assert_eq!(
        vec![
            DataElement::new(0.into(), 5_u32.into(), &[0x01, 0x02, 0x03, 0x04, 0x05]),
            DataElement::new(1.into(), 16_u32.into(), &[0x01]),
        ],
        des
    );
}

#[test]
fn de_iteration_single_de() {
    let mut de_data = vec![];
    // de 2 byte header, type 16, len 1
    de_data.extend_from_slice(&[0x81, 0x10, 0x01]);

    let iterator = DataElementParsingIterator::new(&de_data);
    let des = iterator.collect::<Result<Vec<_>, _>>().unwrap();

    assert_eq!(vec![DataElement::new(0.into(), 16_u32.into(), &[0x01]),], des);
}

#[test]
fn de_iteration_single_de_empty_contents() {
    let mut de_data = vec![];
    // de 1 byte header, type 1, len 0
    de_data.extend_from_slice(&[0x01]);

    let iterator = DataElementParsingIterator::new(&de_data);
    let des = iterator.collect::<Result<Vec<_>, _>>().unwrap();

    assert_eq!(vec![DataElement::new(0.into(), 1_u32.into(), &[])], des);
}

#[test]
fn de_iteration_max_number_des() {
    let mut de_data = vec![];
    // de 1 byte header, type 1, len 0
    // add this 255 times which is the max amount of
    // supported DEs in a single section
    for _ in 0..255 {
        de_data.extend_from_slice(&[0x01]);
    }

    let iterator = DataElementParsingIterator::new(&de_data);
    assert!(iterator.collect::<Result<Vec<_>, _>>().is_ok());
}

#[test]
fn de_iteration_over_max_number_des() {
    let mut de_data = vec![];
    // de 1 byte header, type 1, len 0
    // add this 256 times to exceed max number of des in a section
    for _ in 0..256 {
        de_data.extend_from_slice(&[0x01]);
    }

    let iterator = DataElementParsingIterator::new(&de_data);
    assert_eq!(
        iterator.collect::<Result<Vec<_>, _>>(),
        Err(DataElementParseError::TooManyDataElements)
    );
}

#[test]
fn de_parse_error_invalid_length_header() {
    let mut de_data = vec![];
    // de 1 byte header, type 1, len 2, but only one byte left to process
    de_data.extend_from_slice(&[0x21, 0x00]);

    let iterator = DataElementParsingIterator::new(&de_data);
    assert_eq!(
        iterator.collect::<Result<Vec<_>, _>>(),
        Err(DataElementParseError::UnexpectedDataAfterEnd)
    );
}

mod coverage_gaming {
    use super::*;
    use alloc::format;

    #[allow(clippy::clone_on_copy)]
    #[test]
    fn data_element_debug_and_clone() {
        let de = DataElement::new(0.into(), 0_u32.into(), &[]);
        let _ = format!("{:?}", de);
        let _ = de.clone();
        let iterator = DataElementParsingIterator::new(&[]);
        let _ = format!("{:?}", iterator);
        let _ = format!("{:?}", DataElementParseError::TooManyDataElements);
        let (_, header) = DeHeader::parse(&[0x11, 0xFF]).unwrap();
        let _ = format!("{:?}", header);
        let _ = header.clone();
        let (_, pde) = ProtoDataElement::parse(&[0x11, 0xFF]).unwrap();
        let _ = format!("{:?}", pde);
    }
}
