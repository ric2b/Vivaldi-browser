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

//! V0 data element types.
//!
//! In V0, there are only 16 DE types total, and parsing unknown types is not possible, so we can
//! represent all known DE types in enums without needing to handle the "unknown type" case.

use crate::{legacy::NP_MAX_DE_CONTENT_LEN, DeLengthOutOfRange};
use strum_macros::EnumIter;

#[cfg(test)]
mod tests;

/// A V0 DE type in `[0, 15]`.
#[derive(Debug, PartialEq, Eq, Hash, Clone, Copy)]
pub struct DeTypeCode {
    /// The code used in a V0 adv header
    code: u8,
}

impl DeTypeCode {
    /// Returns a u8 in `[0, 15`].
    pub(crate) fn as_u8(&self) -> u8 {
        self.code
    }

    pub(crate) const fn try_from(value: u8) -> Result<Self, DeTypeCodeOutOfRange> {
        if value < 16 {
            Ok(Self { code: value })
        } else {
            Err(DeTypeCodeOutOfRange)
        }
    }
}

/// The DE type code is out of range for v0 DE types.
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub(crate) struct DeTypeCodeOutOfRange;

/// The actual length of a DE's contents, not the encoded representation.
///
/// See [DeEncodedLength] for the encoded length.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct DeActualLength {
    /// Invariant: <= [NP_MAX_DE_CONTENT_LEN].
    len: u8,
}

impl DeActualLength {
    pub(crate) fn try_from(value: usize) -> Result<Self, DeLengthOutOfRange> {
        if value <= NP_MAX_DE_CONTENT_LEN {
            Ok(Self { len: value as u8 })
        } else {
            Err(DeLengthOutOfRange)
        }
    }

    pub(crate) fn as_u8(&self) -> u8 {
        self.len
    }

    pub(crate) fn as_usize(&self) -> usize {
        self.len.into()
    }
}

/// Maximum encoded length value
pub(crate) const MAX_DE_ENCODED_LEN: u8 = 15;

/// The encoded length of a DE, not the actual length of the DE contents.
///
/// See [DeActualLength] for the length of the contents.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct DeEncodedLength {
    /// Invariant: `len <= 0x0F` (15, aka 4 bits)
    len: u8,
}

impl DeEncodedLength {
    pub(crate) fn try_from(value: u8) -> Result<Self, DeLengthOutOfRange> {
        if value <= MAX_DE_ENCODED_LEN {
            Ok(Self { len: value })
        } else {
            Err(DeLengthOutOfRange)
        }
    }

    /// Test-only helper that panics for less unwrapping in tests.
    ///
    /// # Panics
    ///
    /// Panics if `value` is invalid.
    #[cfg(test)]
    pub(in crate::legacy) fn from(value: u8) -> Self {
        Self::try_from(value).expect("Invalid len")
    }

    /// Returns a u8 in `[0, 15]`
    pub(crate) fn as_u8(&self) -> u8 {
        self.len
    }

    /// Returns a usize in `[0, 15]`
    pub(crate) fn as_usize(&self) -> usize {
        self.len as usize
    }
}

/// Corresponds to the normal data element types.
#[derive(EnumIter, Debug, Clone, Copy, PartialEq, Eq)]
#[allow(missing_docs)]
pub(in crate::legacy) enum DataElementType {
    TxPower,
    Actions,
}
