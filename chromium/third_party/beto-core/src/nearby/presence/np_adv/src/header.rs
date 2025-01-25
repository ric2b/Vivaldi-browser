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

//! NP version header (the first byte) support.
//!
//! The version header byte is 3 bits of version followed by 5 reserved bits.
//!
//! For V0 (`0b000`), the first 3 of the reserved bits identify the encoding
//! scheme used, and the last 2 are still reserved.

use nom::{combinator, number};

// 3-bit versions (high 3 bits in version header)
const PROTOCOL_VERSION_LEGACY: u8 = 0;
const PROTOCOL_VERSION_EXTENDED: u8 = 1;

// 3-bit encoding ids (3 bits after version, leaving 2 reserved bits)
const V0_ENCODING_SCHEME_ID_UNENCRYPTED: u8 = 0;
const V0_ENCODING_SCHEME_ID_LDT: u8 = 1;

/// Version header byte for V1
pub const VERSION_HEADER_V1: u8 = PROTOCOL_VERSION_EXTENDED << 5;

/// Version header byte for V0 with the unencrypted encoding
pub const VERSION_HEADER_V0_UNENCRYPTED: u8 =
    (PROTOCOL_VERSION_LEGACY << 5) | (V0_ENCODING_SCHEME_ID_UNENCRYPTED << 2);

/// Version header byte for V0 with the LDT encoding
pub const VERSION_HEADER_V0_LDT: u8 =
    (PROTOCOL_VERSION_LEGACY << 5) | (V0_ENCODING_SCHEME_ID_LDT << 2);

/// The first byte in the NP svc data. It defines which version of the protocol
/// is used for the rest of the svc data.
#[derive(Debug, PartialEq, Eq, Clone)]
pub(crate) enum NpVersionHeader {
    V0(V0Encoding),
    V1(V1AdvHeader),
}

impl NpVersionHeader {
    /// Parse a NP advertisement header.
    ///
    /// This can be used on all versions of advertisements since it's the header that determines the
    /// version.
    ///
    /// Returns a `nom::IResult` with the parsed header and the remaining bytes of the advertisement.
    pub(crate) fn parse(adv: &[u8]) -> nom::IResult<&[u8], Self> {
        combinator::map_opt(number::complete::u8, |version_header| match version_header {
            VERSION_HEADER_V0_UNENCRYPTED => Some(NpVersionHeader::V0(V0Encoding::Unencrypted)),
            VERSION_HEADER_V0_LDT => Some(NpVersionHeader::V0(V0Encoding::Ldt)),
            VERSION_HEADER_V1 => Some(NpVersionHeader::V1(V1AdvHeader::new(version_header))),
            _ => None,
        })(adv)
    }
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub(crate) enum V0Encoding {
    Unencrypted,
    Ldt,
}

/// A parsed NP Version Header that indicates the V1 protocol is in use.
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct V1AdvHeader {
    header_byte: u8,
}

impl V1AdvHeader {
    pub(crate) fn new(header_byte: u8) -> Self {
        Self { header_byte }
    }

    /// The version header byte
    pub(crate) fn contents(&self) -> u8 {
        self.header_byte
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used)]
mod tests {
    use super::*;

    extern crate std;

    use nom::error;

    #[test]
    fn parse_header_v0_unencrypted() {
        let (_, header) = NpVersionHeader::parse(&[0x00]).unwrap();
        assert_eq!(NpVersionHeader::V0(V0Encoding::Unencrypted), header);
    }

    #[test]
    fn parse_header_v0_ldt() {
        let (_, header) = NpVersionHeader::parse(&[0x04]).unwrap();
        assert_eq!(NpVersionHeader::V0(V0Encoding::Ldt), header);
    }

    #[test]
    fn parse_header_v0_nonzero_invalid_encoding() {
        let input = &[0x08];
        assert_eq!(
            nom::Err::Error(error::Error {
                input: input.as_slice(),
                code: error::ErrorKind::MapOpt,
            }),
            NpVersionHeader::parse(input).unwrap_err()
        );
    }

    #[test]
    fn parse_header_v0_nonzero_reserved() {
        let input = &[0x01];
        assert_eq!(
            nom::Err::Error(error::Error {
                input: input.as_slice(),
                code: error::ErrorKind::MapOpt,
            }),
            NpVersionHeader::parse(input).unwrap_err()
        );
    }

    #[test]
    fn parse_header_v1_nonzero_reserved() {
        let input = &[0x30];
        assert_eq!(
            nom::Err::Error(error::Error {
                input: input.as_slice(),
                code: error::ErrorKind::MapOpt,
            }),
            NpVersionHeader::parse(input).unwrap_err()
        );
    }

    #[test]
    fn parse_header_bad_version() {
        let input = &[0x80];
        assert_eq!(
            nom::Err::Error(error::Error {
                input: input.as_slice(),
                code: error::ErrorKind::MapOpt,
            }),
            NpVersionHeader::parse(input).unwrap_err()
        );
    }

    #[test]
    fn parse_header_v1() {
        let (_, header) = NpVersionHeader::parse(&[0x20]).unwrap();
        assert_eq!(NpVersionHeader::V1(V1AdvHeader::new(0x20)), header);
    }
}
