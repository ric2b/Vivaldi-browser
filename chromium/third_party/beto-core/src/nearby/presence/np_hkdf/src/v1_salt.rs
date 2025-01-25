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

//! Salt used in a V1 advertisement.
use crate::np_salt_hkdf;
use crypto_provider::{hkdf::Hkdf, CryptoProvider, CryptoRng, FromCryptoRng};

/// Length of a V1 extended salt
pub const EXTENDED_SALT_LEN: usize = 16;

/// Salt optionally included in V1 advertisement header.
///
/// The salt is never used directly; rather, a derived salt should be extracted as needed for any
/// section or DE that requires it.
#[derive(Clone, Copy, PartialEq, Debug, Eq)]
pub struct ExtendedV1Salt {
    data: [u8; EXTENDED_SALT_LEN],
}

impl ExtendedV1Salt {
    /// Derive a salt for a particular DE, if applicable.
    ///
    /// Returns none if the requested size is larger than HKDF allows or if offset arithmetic
    /// overflows.
    pub fn derive<const N: usize, C: CryptoProvider>(
        &self,
        de: Option<DataElementOffset>,
    ) -> Option<[u8; N]> {
        let hkdf = np_salt_hkdf::<C>(&self.data);
        let mut arr = [0_u8; N];
        // 0-based offsets -> 1-based indices w/ 0 indicating not present
        hkdf.expand_multi_info(
            &[
                b"V1 derived salt",
                &de.and_then(|d| d.offset.checked_add(1))
                    .map(|o| o.into())
                    .unwrap_or(0_u32)
                    .to_be_bytes(),
            ],
            &mut arr,
        )
        .map(|_| arr)
        .ok()
    }

    /// Returns the salt bytes as a slice
    pub fn as_slice(&self) -> &[u8] {
        self.data.as_slice()
    }

    /// Returns the salt bytes as an array
    pub fn into_array(self) -> [u8; EXTENDED_SALT_LEN] {
        self.data
    }

    /// Returns the salt bytes as a reference to an array
    pub fn bytes(&self) -> &[u8; EXTENDED_SALT_LEN] {
        &self.data
    }
}

impl From<[u8; EXTENDED_SALT_LEN]> for ExtendedV1Salt {
    fn from(arr: [u8; EXTENDED_SALT_LEN]) -> Self {
        Self { data: arr }
    }
}

impl FromCryptoRng for ExtendedV1Salt {
    fn new_random<R: CryptoRng>(rng: &mut R) -> Self {
        rng.gen::<[u8; EXTENDED_SALT_LEN]>().into()
    }
}

/// Offset of a data element in its containing section, used with [ExtendedV1Salt].
#[derive(PartialEq, Eq, Debug, Clone, Copy, PartialOrd, Ord)]
pub struct DataElementOffset {
    /// 0-based offset of the DE in the advertisement
    offset: u8,
}

impl DataElementOffset {
    /// The zero offset
    pub const ZERO: DataElementOffset = Self { offset: 0 };

    /// Returns the offset as a usize
    pub fn as_u8(&self) -> u8 {
        self.offset
    }

    /// Returns the next offset.
    ///
    /// Does not handle overflow as there can't be more than 2^8 DEs in a section.
    pub const fn incremented(&self) -> Self {
        Self { offset: self.offset + 1 }
    }
}

impl From<u8> for DataElementOffset {
    fn from(num: u8) -> Self {
        Self { offset: num }
    }
}
