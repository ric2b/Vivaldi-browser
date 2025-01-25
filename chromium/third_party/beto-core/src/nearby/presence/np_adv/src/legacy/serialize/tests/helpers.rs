// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

extern crate std;

use std::prelude::rust_2021::*;

use sink::Sink;

use crate::legacy::data_elements::{
    DataElementDeserializeError, DeserializeDataElement, DirectMapPredicate, DirectMapper,
    LengthMapper,
};
use crate::{
    legacy::{
        data_elements::{
            de_type::{DeActualLength, DeEncodedLength, DeTypeCode, MAX_DE_ENCODED_LEN},
            DataElementSerializationBuffer, DataElementSerializeError, SerializeDataElement,
        },
        PacketFlavor, NP_MAX_DE_CONTENT_LEN,
    },
    private::Sealed,
    DeLengthOutOfRange,
};

/// A DE allowing arbitrary data lengths. Encoded length is offset from actual length.
///
/// Data longer than [NP_MAX_DE_CONTENT_LEN] or shorter than [NP_MAX_DE_CONTENT_LEN] - [MAX_DE_ENCODED_LEN]
/// will fail to map the actual length.
#[derive(Debug, PartialEq, Eq, Clone)]
pub(crate) struct LongDataElement {
    data: Vec<u8>,
}

impl LongDataElement {
    /// The offset lengths are shifted by when encoded
    // `as usize` is safe since all u8s can fit. Can't use .into() or ::from() in a const.
    pub(in crate::legacy) const OFFSET: usize =
        NP_MAX_DE_CONTENT_LEN - (MAX_DE_ENCODED_LEN as usize);
    pub fn new(data: Vec<u8>) -> Self {
        Self { data }
    }
}

impl Sealed for LongDataElement {}

impl<F: PacketFlavor> SerializeDataElement<F> for LongDataElement {
    fn de_type_code(&self) -> DeTypeCode {
        Self::DE_TYPE_CODE
    }

    fn map_actual_len_to_encoded_len(&self, actual_len: DeActualLength) -> DeEncodedLength {
        <Self as DeserializeDataElement>::LengthMapper::map_actual_len_to_encoded_len(actual_len)
    }

    fn serialize_contents(
        &self,
        sink: &mut DataElementSerializationBuffer,
    ) -> Result<(), DataElementSerializeError> {
        sink.try_extend_from_slice(&self.data).ok_or(DataElementSerializeError::InsufficientSpace)
    }
}

impl DeserializeDataElement for LongDataElement {
    const DE_TYPE_CODE: DeTypeCode = match DeTypeCode::try_from(15) {
        Ok(t) => t,
        Err(_) => unreachable!(),
    };
    type LengthMapper = OffsetMapper<{ Self::OFFSET }>;

    fn deserialize<F: PacketFlavor>(
        de_contents: &[u8],
    ) -> Result<Self, DataElementDeserializeError> {
        Ok(Self { data: de_contents.to_vec() })
    }
}

impl From<Vec<u8>> for LongDataElement {
    fn from(value: Vec<u8>) -> Self {
        Self { data: value }
    }
}

/// Subtracts `O` from actual lengths to yield encoded lengths, and vice versa.
pub(in crate::legacy) struct OffsetMapper<const O: usize>;

impl<const O: usize> LengthMapper for OffsetMapper<O> {
    fn map_encoded_len_to_actual_len(
        encoded_len: DeEncodedLength,
    ) -> Result<DeActualLength, DeLengthOutOfRange> {
        DeActualLength::try_from(encoded_len.as_usize() + O)
    }

    fn map_actual_len_to_encoded_len(actual_len: DeActualLength) -> DeEncodedLength {
        actual_len
            .as_u8()
            .checked_sub(O.try_into().expect("Actual len is too short to use offset"))
            .ok_or(DeLengthOutOfRange)
            .and_then(DeEncodedLength::try_from)
            .expect("Couldn't encode actual len")
    }
}

/// A DE allowing arbitrary data with a straightforward actual len = encoded len scheme.
///
/// Data longer than [MAX_DE_ENCODED_LEN] will fail when mapping the length.
#[derive(Debug, PartialEq, Eq, Clone)]
pub(crate) struct ShortDataElement {
    data: Vec<u8>,
}

impl ShortDataElement {
    pub fn new(data: Vec<u8>) -> Self {
        Self { data }
    }
}

impl Sealed for ShortDataElement {}

impl<F: PacketFlavor> SerializeDataElement<F> for ShortDataElement {
    fn de_type_code(&self) -> DeTypeCode {
        Self::DE_TYPE_CODE
    }

    fn map_actual_len_to_encoded_len(&self, actual_len: DeActualLength) -> DeEncodedLength {
        <Self as DeserializeDataElement>::LengthMapper::map_actual_len_to_encoded_len(actual_len)
    }

    fn serialize_contents(
        &self,
        sink: &mut DataElementSerializationBuffer,
    ) -> Result<(), DataElementSerializeError> {
        sink.try_extend_from_slice(&self.data).ok_or(DataElementSerializeError::InsufficientSpace)
    }
}

impl DeserializeDataElement for ShortDataElement {
    const DE_TYPE_CODE: DeTypeCode = match DeTypeCode::try_from(14) {
        Ok(t) => t,
        Err(_) => unreachable!(),
    };

    type LengthMapper = DirectMapper<YoloLengthPredicate>;

    fn deserialize<F: PacketFlavor>(
        de_contents: &[u8],
    ) -> Result<Self, DataElementDeserializeError> {
        Ok(Self { data: de_contents.to_vec() })
    }
}

/// Approves all lengths
pub(in crate::legacy) struct YoloLengthPredicate;

impl DirectMapPredicate for YoloLengthPredicate {
    fn is_valid(_len: usize) -> bool {
        true
    }
}

impl From<Vec<u8>> for ShortDataElement {
    fn from(value: Vec<u8>) -> Self {
        Self { data: value }
    }
}
