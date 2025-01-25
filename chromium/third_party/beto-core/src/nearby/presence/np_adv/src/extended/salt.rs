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

//! Various representations of salts for extended advertisements.

use nom::combinator;

use crypto_provider::{aes::ctr::AesCtrNonce, CryptoProvider, CryptoRng, FromCryptoRng};

use crate::helpers::parse_byte_array;

pub use np_hkdf::v1_salt::ExtendedV1Salt;

/// Common behavior for V1 section salts.
pub trait V1Salt: Copy + Into<MultiSalt> {
    /// Derive the nonce used for section encryption.
    ///
    /// Both kinds of salts can compute the nonce needed for de/encrypting a
    /// section, but only extended salts can have data derived from them.
    fn compute_nonce<C: CryptoProvider>(&self) -> AesCtrNonce;
}

impl V1Salt for ExtendedV1Salt {
    fn compute_nonce<C: CryptoProvider>(&self) -> AesCtrNonce {
        self.derive::<12, C>(None).expect("AES-CTR nonce is a valid HKDF size")
    }
}

pub(crate) const SHORT_SALT_LEN: usize = 2;

/// A byte buffer the size of a V1 short salt
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct ShortV1Salt([u8; SHORT_SALT_LEN]);

impl From<[u8; SHORT_SALT_LEN]> for ShortV1Salt {
    fn from(value: [u8; SHORT_SALT_LEN]) -> Self {
        Self(value)
    }
}

impl ShortV1Salt {
    pub(crate) fn bytes(&self) -> &[u8; SHORT_SALT_LEN] {
        &self.0
    }

    pub(crate) fn parse(input: &[u8]) -> nom::IResult<&[u8], Self> {
        combinator::map(parse_byte_array::<SHORT_SALT_LEN>, Self)(input)
    }
}

impl V1Salt for ShortV1Salt {
    fn compute_nonce<C: CryptoProvider>(&self) -> AesCtrNonce {
        np_hkdf::extended_mic_section_short_salt_nonce::<C>(self.0)
    }
}

impl FromCryptoRng for ShortV1Salt {
    fn new_random<R: CryptoRng>(rng: &mut R) -> Self {
        rng.gen::<[u8; SHORT_SALT_LEN]>().into()
    }
}

/// Either a short or extended salt.
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum MultiSalt {
    /// A 2-byte salt
    Short(ShortV1Salt),
    /// A 16-byte salt
    Extended(ExtendedV1Salt),
}

impl MultiSalt {
    /// Salt bytes as a slice, for when variable-size access is sensible
    pub fn as_slice(&self) -> &[u8] {
        match self {
            MultiSalt::Short(s) => s.bytes().as_slice(),
            MultiSalt::Extended(s) => s.bytes().as_slice(),
        }
    }
}

impl From<ExtendedV1Salt> for MultiSalt {
    fn from(value: ExtendedV1Salt) -> Self {
        Self::Extended(value)
    }
}

impl From<ShortV1Salt> for MultiSalt {
    fn from(value: ShortV1Salt) -> Self {
        Self::Short(value)
    }
}

impl V1Salt for MultiSalt {
    /// Both kinds of salts can compute the nonce needed for decrypting an
    /// advertisement, but only extended salts can have data derived from them.
    fn compute_nonce<C: CryptoProvider>(&self) -> AesCtrNonce {
        match self {
            Self::Short(s) => V1Salt::compute_nonce::<C>(s),
            Self::Extended(s) => s.compute_nonce::<C>(),
        }
    }
}
