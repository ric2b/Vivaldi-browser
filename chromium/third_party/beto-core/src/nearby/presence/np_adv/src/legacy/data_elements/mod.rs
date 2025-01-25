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

//! V0 data elements and core trait impls.

use crate::{
    extended::serialize::CapacityLimitedVec,
    legacy::{
        data_elements::de_type::{DeActualLength, DeEncodedLength, DeTypeCode},
        PacketFlavor, PacketFlavorEnum, NP_MAX_ADV_CONTENT_LEN,
    },
    private::Sealed,
    DeLengthOutOfRange,
};
use core::marker;
use nom::error::{self};
use sink::Sink;

pub mod actions;
pub mod de_type;
pub mod tx_power;

#[cfg(test)]
pub(in crate::legacy) mod tests;

/// Deserialization for a specific data element type.
///
/// See also [SerializeDataElement] for flavor-specific, infallible serialization.
pub(in crate::legacy) trait DeserializeDataElement: Sized + Sealed {
    const DE_TYPE_CODE: DeTypeCode;

    /// Define how length mapping is done for this DE type
    type LengthMapper: LengthMapper;

    /// Deserialize `Self` from the provided DE contents (not including the
    /// DE header).
    ///
    /// Returns `Err` if the flavor is not supported or if parsing fails.
    ///
    /// `<F>` is the flavor of the packet being deserialized.
    fn deserialize<F: PacketFlavor>(
        de_contents: &[u8],
    ) -> Result<Self, DataElementDeserializeError>;
}

/// Errors possible when deserializing a DE
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum DataElementDeserializeError {
    /// The data element doesn't support the [PacketFlavor] of the advertisement packet.
    FlavorNotSupported {
        /// The DE type attempting to be deserialized
        de_type: DeTypeCode,
        /// The flavor that was not supported
        flavor: PacketFlavorEnum,
    },
    /// The data element couldn't be deserialized from the supplied data.
    DeserializeError {
        /// The DE type attempting to be deserialized
        de_type: DeTypeCode,
    },
    /// Invalid DE type
    InvalidDeType {
        /// The unknown type
        de_type: DeTypeCode,
    },
    /// Invalid DE length
    InvalidDeLength {
        /// The DE type attempting to be deserialized
        de_type: DeTypeCode,
        /// The invalid length
        len: DeEncodedLength,
    },
    /// The same de type code was encountered more than once in an advertisement
    DuplicateDeTypes,
    /// Other parse error, e.g. the adv is truncated
    InvalidStructure,
}

impl error::FromExternalError<&[u8], DataElementDeserializeError> for DataElementDeserializeError {
    fn from_external_error(
        _input: &[u8],
        _kind: error::ErrorKind,
        e: DataElementDeserializeError,
    ) -> Self {
        e
    }
}

impl error::ParseError<&[u8]> for DataElementDeserializeError {
    /// Creates an error from the input position and an [error::ErrorKind]
    fn from_error_kind(_input: &[u8], _kind: error::ErrorKind) -> Self {
        Self::InvalidStructure
    }

    /// Combines an existing error with a new one created from the input
    /// position and an [error::ErrorKind]. This is useful when backtracking
    /// through a parse tree, accumulating error context on the way
    fn append(_input: &[u8], _kind: error::ErrorKind, _other: Self) -> Self {
        Self::InvalidStructure
    }
}

/// Serialization of a DE for a particular flavor.
///
/// The flavor is a type parameter on the trait, rather than on the method,
/// so that a DE can indicate its flavor support by implementing this trait
/// only with the relevant flavors. Deserialization, on the other hand, can
/// express "flavor not supported" for invalid input.
pub trait SerializeDataElement<F: PacketFlavor>: Sealed {
    /// Returns the DE type code this DE uses in the header.
    fn de_type_code(&self) -> DeTypeCode;

    /// Convert the actual DE content length to the encoded length included in the header.
    ///
    /// This has a `&self` receiver only so that it can be object-safe; it
    /// should not be relevant in the calculation.
    ///
    /// # Panics
    ///
    /// Panics if the actual length is invalid for this DE type, or if the
    /// encoded length cannot fit in [DeEncodedLength], either of which means
    /// that the serialization impl is broken.
    fn map_actual_len_to_encoded_len(&self, actual_len: DeActualLength) -> DeEncodedLength;

    /// Serialize the data element's data (not including the header) into the sink.
    fn serialize_contents(
        &self,
        sink: &mut DataElementSerializationBuffer,
    ) -> Result<(), DataElementSerializeError>;
}

/// Errors possible when serializing a DE
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum DataElementSerializeError {
    /// Not enough available space
    InsufficientSpace,
}

/// Buffer to serialize a DE, including the header, into.
pub struct DataElementSerializationBuffer {
    vec: CapacityLimitedVec<u8, NP_MAX_ADV_CONTENT_LEN>,
}

impl DataElementSerializationBuffer {
    /// Returns `None` if `capacity` exceeds [NP_MAX_ADV_CONTENT_LEN].
    pub(crate) fn new(capacity: usize) -> Option<Self> {
        CapacityLimitedVec::new(capacity).map(|vec| Self { vec })
    }

    pub(crate) fn len(&self) -> usize {
        self.vec.len()
    }

    pub(crate) fn into_inner(self) -> CapacityLimitedVec<u8, NP_MAX_ADV_CONTENT_LEN> {
        self.vec
    }
}

impl Sink<u8> for DataElementSerializationBuffer {
    fn try_extend_from_slice(&mut self, items: &[u8]) -> Option<()> {
        Sink::try_extend_from_slice(&mut self.vec, items)
    }

    fn try_push(&mut self, item: u8) -> Option<()> {
        Sink::try_push(&mut self.vec, item)
    }
}

/// Trait object reference to a `ToDataElementBundle<I>` with lifetime `'a`.
/// Implements [SerializeDataElement] by deferring to the wrapped trait object.
pub struct DynamicSerializeDataElement<'a, I: PacketFlavor> {
    wrapped: &'a dyn SerializeDataElement<I>,
}

impl<'a, I: PacketFlavor> From<&'a dyn SerializeDataElement<I>>
    for DynamicSerializeDataElement<'a, I>
{
    fn from(wrapped: &'a dyn SerializeDataElement<I>) -> Self {
        DynamicSerializeDataElement { wrapped }
    }
}

impl<'a, F: PacketFlavor> Sealed for DynamicSerializeDataElement<'a, F> {}

impl<'a, F: PacketFlavor> SerializeDataElement<F> for DynamicSerializeDataElement<'a, F> {
    fn de_type_code(&self) -> DeTypeCode {
        self.wrapped.de_type_code()
    }

    fn map_actual_len_to_encoded_len(&self, actual_len: DeActualLength) -> DeEncodedLength {
        self.wrapped.map_actual_len_to_encoded_len(actual_len)
    }

    fn serialize_contents(
        &self,
        sink: &mut DataElementSerializationBuffer,
    ) -> Result<(), DataElementSerializeError> {
        self.wrapped.serialize_contents(sink)
    }
}

/// Maps encoded to actual lengths and vice versa.
///
/// Each v0 DE type has their own length mapping rules.
pub(in crate::legacy) trait LengthMapper {
    /// Convert the encoded DE content length in the header to the actual length to consume from
    /// the advertisement, or an error if the encoded length is invalid for the DE type.
    fn map_encoded_len_to_actual_len(
        encoded_len: DeEncodedLength,
    ) -> Result<DeActualLength, DeLengthOutOfRange>;

    /// Convert the actual DE content length to the encoded length included in the header.
    ///
    /// # Panics
    ///
    /// Panics if the actual length is invalid for this DE type, or if the
    /// encoded length cannot fit in [DeEncodedLength], either of which means
    /// that the serialization impl is broken.
    fn map_actual_len_to_encoded_len(actual_len: DeActualLength) -> DeEncodedLength;
}

/// A length predicate used with [DirectMapper].
pub(in crate::legacy) trait DirectMapPredicate {
    /// Return `true` iff the len is valid as a DE encoded len _and_ actual len.
    fn is_valid(len: usize) -> bool;
}

/// A [LengthMapper] that maps the input number directly to the output number without any scaling or
/// shifting.
///
/// Iff `predicate` evaluates to true, the input number will be transformed into the output type,
/// for both directions.
pub(in crate::legacy) struct DirectMapper<P: DirectMapPredicate> {
    _marker: marker::PhantomData<P>,
}

impl<P: DirectMapPredicate> LengthMapper for DirectMapper<P> {
    fn map_encoded_len_to_actual_len(
        encoded_len: DeEncodedLength,
    ) -> Result<DeActualLength, DeLengthOutOfRange> {
        let enc = encoded_len.as_usize();
        if P::is_valid(enc) {
            DeActualLength::try_from(enc)
        } else {
            Err(DeLengthOutOfRange)
        }
    }

    fn map_actual_len_to_encoded_len(actual_len: DeActualLength) -> DeEncodedLength {
        assert!(
            P::is_valid(actual_len.as_usize()),
            "Broken DE implementation produced invalid length"
        );
        DeEncodedLength::try_from(actual_len.as_u8())
            .expect("Actual length has already been validated")
    }
}
