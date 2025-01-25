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

use crate::extended::salt::ShortV1Salt;
use crate::extended::{
    V1IdentityToken, V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN,
    V1_ENCODING_ENCRYPTED_MIC_WITH_SHORT_SALT_AND_TOKEN,
    V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN, V1_ENCODING_UNENCRYPTED,
};
use np_hkdf::v1_salt::ExtendedV1Salt;

/// Format || salt || token
pub(crate) const SECTION_HEADER_MAX_LEN: usize = 2 + 16 + 16;

/// Assembles the header bytes for a section after the section length, before postprocessing by
/// a [SectionEncoder](crate::extended::serialize::section::SectionEncoder).
///
/// Does not include the overall NP header byte that defines the adv version.
pub struct SectionHeader {
    /// The section header
    /// Invariant: always has at least 1 byte
    header_bytes: tinyvec::ArrayVec<[u8; SECTION_HEADER_MAX_LEN]>,
}

impl SectionHeader {
    pub(crate) fn unencrypted() -> Self {
        let mut header_bytes = tinyvec::ArrayVec::new();
        header_bytes.push(V1_ENCODING_UNENCRYPTED);
        Self { header_bytes }
    }

    pub(crate) fn encrypted_mic_short_salt(salt: ShortV1Salt, token: &V1IdentityToken) -> Self {
        let mut header_bytes = tinyvec::ArrayVec::new();
        header_bytes.push(V1_ENCODING_ENCRYPTED_MIC_WITH_SHORT_SALT_AND_TOKEN);
        header_bytes.extend_from_slice(salt.bytes().as_slice());
        header_bytes.extend_from_slice(token.bytes().as_slice());
        Self { header_bytes }
    }

    pub(crate) fn encrypted_mic_extended_salt(
        salt: &ExtendedV1Salt,
        token: &V1IdentityToken,
    ) -> Self {
        let mut header_bytes = tinyvec::ArrayVec::new();
        header_bytes.push(V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN);
        header_bytes.extend_from_slice(salt.bytes().as_slice());
        header_bytes.extend_from_slice(token.bytes().as_slice());
        Self { header_bytes }
    }

    pub(crate) fn encrypted_signature_extended_salt(
        salt: &ExtendedV1Salt,
        token: &V1IdentityToken,
    ) -> Self {
        let mut header_bytes = tinyvec::ArrayVec::new();
        header_bytes.push(V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN);
        header_bytes.extend_from_slice(salt.bytes().as_slice());
        header_bytes.extend_from_slice(token.bytes().as_slice());
        Self { header_bytes }
    }

    pub(crate) fn as_slice(&self) -> &[u8] {
        self.header_bytes.as_slice()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::extended::V1_IDENTITY_TOKEN_LEN;
    use ldt_np_adv::V0_SALT_LEN;
    use np_hkdf::v1_salt::EXTENDED_SALT_LEN;

    const SHORT_SALT_BYTES: [u8; V0_SALT_LEN] = [0x10, 0x11];
    const EXTENDED_SALT_BYTES: [u8; EXTENDED_SALT_LEN] = [
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E,
        0x1F,
    ];
    const TOKEN_BYTES: [u8; V1_IDENTITY_TOKEN_LEN] = [
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E,
        0x2F,
    ];

    #[test]
    fn unencrypted_slice() {
        assert_eq!(&[V1_ENCODING_UNENCRYPTED], SectionHeader::unencrypted().as_slice());
    }

    #[rustfmt::skip]
    #[test]
    fn encrypted_mic_short_salt_slice() {
        assert_eq!(
            &[
                // format
                0x01,
                // salt
                0x10, 0x11,
                // token
                0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
                0x2C, 0x2D, 0x2E, 0x2F,
            ],
            SectionHeader::encrypted_mic_short_salt(
                SHORT_SALT_BYTES.into(),
                &TOKEN_BYTES.into()
            )
                .as_slice()
        );
    }

    #[rustfmt::skip]
    #[test]
    fn encrypted_mic_extended_salt_slice() {
        assert_eq!(
            &[
                // format
                0x02,
                // salt
                0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
                0x1C, 0x1D, 0x1E, 0x1F,
                // token
                0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
                0x2C, 0x2D, 0x2E, 0x2F,
            ],
            SectionHeader::encrypted_mic_extended_salt(
                &EXTENDED_SALT_BYTES.into(),
                &TOKEN_BYTES.into()
            )
                .as_slice()
        );
    }

    #[rustfmt::skip]
    #[test]
    fn encrypted_sig_extended_salt_slice() {
        assert_eq!(
            &[
                // format
                0x03,
                // salt
                0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
                0x1C, 0x1D, 0x1E, 0x1F,
                // token
                0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
                0x2C, 0x2D, 0x2E, 0x2F,
            ],
            SectionHeader::encrypted_signature_extended_salt(
                &EXTENDED_SALT_BYTES.into(),
                &TOKEN_BYTES.into()
            )
                .as_slice()
        );
    }
}
