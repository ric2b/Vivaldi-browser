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

use ldt_np_adv::{V0IdentityToken, V0Salt};

/// Format || salt || token
const ADV_HEADER_MAX_LEN: usize = 1 + 2 + 14;

/// Serializes a V0 header.
///
/// Does not include the overall NP version header byte that defines the adv
/// version.
pub struct V0Header {
    header_bytes: tinyvec::ArrayVec<[u8; ADV_HEADER_MAX_LEN]>,
}

impl V0Header {
    pub(crate) fn unencrypted() -> Self {
        let header_bytes = tinyvec::ArrayVec::new();
        Self { header_bytes }
    }

    pub(crate) fn ldt_short_salt(salt: V0Salt, identity_token: V0IdentityToken) -> Self {
        let mut header_bytes = tinyvec::ArrayVec::new();
        header_bytes.extend_from_slice(salt.bytes().as_slice());
        header_bytes.extend_from_slice(identity_token.as_slice());
        Self { header_bytes }
    }

    /// The returned slice must be shorter than [crate::legacy::BLE_4_ADV_SVC_MAX_CONTENT_LEN] - 1.
    pub(crate) fn as_slice(&self) -> &[u8] {
        self.header_bytes.as_slice()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ldt_np_adv::{V0_IDENTITY_TOKEN_LEN, V0_SALT_LEN};

    const SHORT_SALT_BYTES: [u8; V0_SALT_LEN] = [0x10, 0x11];
    const TOKEN_BYTES: [u8; V0_IDENTITY_TOKEN_LEN] =
        [0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D];

    #[test]
    fn unencrypted_slice() {
        assert_eq!(&[0_u8; 0], V0Header::unencrypted().as_slice());
    }

    #[rustfmt::skip]
    #[test]
    fn ldt_short_salt_slice() {
        assert_eq!(
            &[
                // salt
                0x10, 0x11,
                // token
                0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D
            ],
            V0Header::ldt_short_salt(SHORT_SALT_BYTES.into(), TOKEN_BYTES.into()).as_slice()
        );
    }
}
