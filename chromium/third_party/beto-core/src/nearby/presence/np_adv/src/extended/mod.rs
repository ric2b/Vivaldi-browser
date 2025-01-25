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

//! V1 advertisement support.
use crate::DeLengthOutOfRange;
use array_view::ArrayView;
use crypto_provider::CryptoRng;

pub mod data_elements;
pub mod de_type;
pub mod deserialize;
pub mod salt;
pub mod section_signature_payload;
pub mod serialize;

// TODO make this easy to use w/ configurable arena size
/// Maximum size of an NP advertisement, including the adv header
pub const BLE_5_ADV_SVC_MAX_CONTENT_LEN: usize = 254
    // length and type bytes for svc data TLV
    - 1 - 1
    // NP UUID
    - 2;

/// Maximum number of sections in an advertisement
pub const NP_V1_ADV_MAX_SECTION_COUNT: usize = 8;

/// Maximum size of a NP section, including its length header byte
pub const NP_ADV_MAX_SECTION_LEN: usize = NP_ADV_MAX_SECTION_CONTENTS_LEN + 1;

// TODO should this be 255 (or 256, if we +1 the length)?
/// Maximum hypothetical size of a NP section's contents, excluding its header
/// byte. This is longer than can fit in a BLE 5 extended adv, but other media
/// could fit it, like mDNS.
const NP_ADV_MAX_SECTION_CONTENTS_LEN: usize = 255;

/// Size of a V1 identity token
pub const V1_IDENTITY_TOKEN_LEN: usize = 16;

// 4-bit encoding ids
/// Encoding ID for unencrypted sections with no salt
pub const V1_ENCODING_UNENCRYPTED: u8 = 0x00;
/// Encoding ID for encrypted sections with a MIC and a short salt
pub const V1_ENCODING_ENCRYPTED_MIC_WITH_SHORT_SALT_AND_TOKEN: u8 = 0x01;
/// Encoding ID for encrypted sections with a MIC and an extended salt
pub const V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN: u8 = 0x02;
/// Encoding ID for encrypted sections with a signature and an extended salt
pub const V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN: u8 = 0x03;

// The maximum de length that fits into a non-extended de header
const MAX_NON_EXTENDED_LEN: u8 = 7;
// The maximum type code that fits into a non-extended de header
const MAX_NON_EXTENDED_TYPE_CODE: u32 = 15;

fn de_requires_extended_bit(type_code: u32, de_len: u8) -> bool {
    de_len > MAX_NON_EXTENDED_LEN || type_code > MAX_NON_EXTENDED_TYPE_CODE
}

/// 16-byte plaintext identity token.
///
/// Identity tokens are present in encrypted form in a section's header.
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq)]
pub struct V1IdentityToken([u8; V1_IDENTITY_TOKEN_LEN]);

impl From<[u8; V1_IDENTITY_TOKEN_LEN]> for V1IdentityToken {
    fn from(value: [u8; V1_IDENTITY_TOKEN_LEN]) -> Self {
        Self(value)
    }
}

impl V1IdentityToken {
    /// Returns a reference to the inner byte array
    pub fn bytes(&self) -> &[u8; V1_IDENTITY_TOKEN_LEN] {
        &self.0
    }

    /// Returns the inner byte array
    pub const fn into_bytes(self) -> [u8; V1_IDENTITY_TOKEN_LEN] {
        self.0
    }

    /// Returns the token bytes as a slice
    pub fn as_slice(&self) -> &[u8] {
        &self.0
    }
}

impl AsRef<[u8]> for V1IdentityToken {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl crypto_provider::FromCryptoRng for V1IdentityToken {
    fn new_random<R: CryptoRng>(rng: &mut R) -> Self {
        Self(rng.gen())
    }
}

/// Max V1 DE length (7 bit length field).
pub(crate) const MAX_DE_LEN: usize = 127;

/// Length of a DE's content -- must be in `[0, 127]`
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct DeLength {
    len: u8,
}

impl DeLength {
    /// A convenient constant for zero length.
    pub const ZERO: DeLength = DeLength { len: 0 };

    fn as_u8(&self) -> u8 {
        self.len
    }
}

impl TryFrom<u8> for DeLength {
    type Error = DeLengthOutOfRange;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        if usize::from(value) <= MAX_DE_LEN {
            Ok(Self { len: value })
        } else {
            Err(DeLengthOutOfRange {})
        }
    }
}

impl TryFrom<usize> for DeLength {
    type Error = DeLengthOutOfRange;

    fn try_from(value: usize) -> Result<Self, Self::Error> {
        value.try_into().map_err(|_e| DeLengthOutOfRange).and_then(|num: u8| num.try_into())
    }
}

/// Convert a tinyvec into an equivalent ArrayView
pub(crate) fn to_array_view<T, const N: usize>(vec: tinyvec::ArrayVec<[T; N]>) -> ArrayView<T, N>
where
    [T; N]: tinyvec::Array,
{
    let len = vec.len();
    ArrayView::try_from_array(vec.into_inner(), len).expect("len is from original vec")
}

#[cfg(test)]
mod tests {
    use super::*;
    use rand::{distributions, Rng};

    // support randomly generating tokens just for tests
    impl distributions::Distribution<V1IdentityToken> for distributions::Standard {
        fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> V1IdentityToken {
            V1IdentityToken::from(rng.gen::<[u8; V1_IDENTITY_TOKEN_LEN]>())
        }
    }
}
