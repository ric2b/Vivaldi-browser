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

//! The "Actions" data element and associated types.
//!
//! This DE is somewhat more complex than other DEs. Whether or not it supports a particular flavor
//! depends on the actions set, so it has to be treated as two separate types based on which
//! flavor type parameter is used.
use crate::legacy::data_elements::{DirectMapPredicate, DirectMapper, LengthMapper};
use crate::{
    legacy::{
        data_elements::{
            de_type::{DeActualLength, DeEncodedLength, DeTypeCode},
            DataElementDeserializeError, DataElementSerializationBuffer, DataElementSerializeError,
            DeserializeDataElement, SerializeDataElement,
        },
        PacketFlavor, PacketFlavorEnum,
    },
    private::Sealed,
};

#[cfg(feature = "devtools")]
use core::ops::Range;
use core::{marker, ops};
use nom::{bytes, combinator, error};
use sink::Sink;
use strum::IntoEnumIterator as _;

mod macros;
#[cfg(test)]
pub(crate) mod tests;

/// Actions DE.
/// Only as many DE payload bytes will be present as needed to represent all set bits that are encoded,
/// with a lower bound of 1 byte in the special case of no set action bits, and an upper bound
/// of 3 bytes occupied by the DE payload.
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct ActionsDataElement<F: PacketFlavor> {
    /// The action bits
    pub action: ActionBits<F>,
}

/// Max length of an actions DE contents
pub(crate) const ACTIONS_MAX_LEN: usize = 3;
/// Range of valid actual lengths
pub(crate) const ACTIONS_VALID_ACTUAL_LEN: ops::RangeInclusive<usize> = 1..=ACTIONS_MAX_LEN;

impl<F> ActionsDataElement<F>
where
    F: PacketFlavor,
{
    /// Generic deserialize, not meant to be called directly -- use [DeserializeDataElement] impls instead.
    #[allow(clippy::assertions_on_constants)]
    fn deserialize(de_contents: &[u8]) -> Result<Self, DataElementDeserializeError> {
        combinator::all_consuming::<&[u8], _, error::Error<&[u8]>, _>(combinator::map(
            bytes::complete::take_while_m_n(0, ACTIONS_MAX_LEN, |_| true),
            |bytes: &[u8]| {
                // pack bits into u32 for convenient access
                debug_assert!(4 >= ACTIONS_MAX_LEN, "Actions must fit in u32");
                let mut action_bytes = [0_u8; 4];
                action_bytes[..bytes.len()].copy_from_slice(bytes);
                u32::from_be_bytes(action_bytes)
            },
        ))(de_contents)
        .map_err(|_| DataElementDeserializeError::DeserializeError { de_type: Self::DE_TYPE_CODE })
        .map(|(_remaining, actions)| actions)
        .and_then(|action_bits_num| {
            let action = ActionBits::try_from(action_bits_num).map_err(|e| {
                DataElementDeserializeError::FlavorNotSupported {
                    de_type: Self::DE_TYPE_CODE,
                    flavor: e.flavor,
                }
            })?;
            Ok(Self { action })
        })
    }
}

impl<F: PacketFlavor> From<ActionBits<F>> for ActionsDataElement<F> {
    fn from(action: ActionBits<F>) -> Self {
        Self { action }
    }
}

impl<F: PacketFlavor> Sealed for ActionsDataElement<F> {}

impl<F: PacketFlavor> SerializeDataElement<F> for ActionsDataElement<F> {
    fn de_type_code(&self) -> DeTypeCode {
        ActionsDataElement::<F>::DE_TYPE_CODE
    }

    fn map_actual_len_to_encoded_len(&self, actual_len: DeActualLength) -> DeEncodedLength {
        <Self as DeserializeDataElement>::LengthMapper::map_actual_len_to_encoded_len(actual_len)
    }

    fn serialize_contents(
        &self,
        sink: &mut DataElementSerializationBuffer,
    ) -> Result<(), DataElementSerializeError> {
        let used = self.action.bytes_used();
        sink.try_extend_from_slice(&self.action.bits.to_be_bytes()[..used])
            .ok_or(DataElementSerializeError::InsufficientSpace)
    }
}

impl<E: PacketFlavor> DeserializeDataElement for ActionsDataElement<E> {
    const DE_TYPE_CODE: DeTypeCode = match DeTypeCode::try_from(0b0110) {
        Ok(t) => t,
        Err(_) => unreachable!(),
    };

    type LengthMapper = DirectMapper<ActionsLengthPredicate>;

    fn deserialize<F: PacketFlavor>(
        de_contents: &[u8],
    ) -> Result<Self, DataElementDeserializeError> {
        if E::ENUM_VARIANT == F::ENUM_VARIANT {
            ActionsDataElement::deserialize(de_contents)
        } else {
            Err(DataElementDeserializeError::FlavorNotSupported {
                de_type: Self::DE_TYPE_CODE,
                flavor: F::ENUM_VARIANT,
            })
        }
    }
}

pub(in crate::legacy) struct ActionsLengthPredicate;

impl DirectMapPredicate for ActionsLengthPredicate {
    fn is_valid(len: usize) -> bool {
        ACTIONS_VALID_ACTUAL_LEN.contains(&len)
    }
}

/// Container for the 24 bits defined for "actions" (feature flags and the like).
/// This internally stores a u32, but only the 24 highest bits of this
/// field will actually ever be populated.
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct ActionBits<F: PacketFlavor> {
    bits: u32,
    // marker for element type
    flavor: marker::PhantomData<F>,
}

impl<F: PacketFlavor> ActionBits<F> {
    /// Returns the actions bits as a u32. The upper limit of an actions field is 3 bytes,
    /// so the last bytes of this u32 will always be 0
    pub fn as_u32(self) -> u32 {
        self.bits
    }

    /// Return whether a boolean action type is set in this data element, or `None` if the given
    /// action type does not represent a boolean.
    pub fn has_action(&self, action_type: ActionType) -> bool {
        self.bits_for_type(action_type) != 0
    }
}

impl<F: PacketFlavor> Default for ActionBits<F> {
    fn default() -> Self {
        ActionBits {
            bits: 0, // no bits set
            flavor: marker::PhantomData,
        }
    }
}

/// At least one action doesn't support the required flavor
#[derive(PartialEq, Eq, Debug)]
pub struct FlavorNotSupported {
    flavor: PacketFlavorEnum,
}

lazy_static::lazy_static! {
    /// All bits for plaintext action types: 1 where a plaintext action could have a bit, 0 elsewhere.
    static ref ALL_PLAINTEXT_ELEMENT_BITS: u32 = ActionType::iter()
        .filter(|t| t.supports_flavor(PacketFlavorEnum::Plaintext))
        .map(|t| t.all_bits())
        .fold(0_u32, |accum, bits| accum | bits);
}

lazy_static::lazy_static! {
    /// All bits for ciphertext action types: 1 where a ciphertext action could have a bit, 0 elsewhere.
    static ref ALL_CIPHERTEXT_ELEMENT_BITS: u32 = ActionType::iter()
        .filter(|t| t.supports_flavor(PacketFlavorEnum::Ciphertext))
        .map(|t| t.all_bits())
        .fold(0_u32, |accum, bits| accum | bits);
}

impl<F: PacketFlavor> ActionBits<F> {
    /// Tries to create ActionBits from a u32, returning error in the event a specific bit is set for
    /// an unsupported flavor
    pub fn try_from(value: u32) -> Result<Self, FlavorNotSupported> {
        let ok_bits: u32 = match F::ENUM_VARIANT {
            PacketFlavorEnum::Plaintext => *ALL_PLAINTEXT_ELEMENT_BITS,
            PacketFlavorEnum::Ciphertext => *ALL_CIPHERTEXT_ELEMENT_BITS,
        };

        // no bits set beyond what's allowed for this flavor
        if value | ok_bits == ok_bits {
            Ok(Self { bits: value, flavor: marker::PhantomData })
        } else {
            Err(FlavorNotSupported { flavor: F::ENUM_VARIANT })
        }
    }

    /// Set the bits for the provided element.
    /// Bits outside the range set by the action will be unaffected.
    pub fn set_action<E: ActionElementFlavor<F>>(&mut self, action_element: E) {
        let bits = action_element.bits();

        // validate that the element is not horribly broken
        debug_assert!(E::HIGH_BIT_INDEX < 32);
        // must not have bits set past the low `len` bits
        debug_assert_eq!(0, bits >> 1);

        // 0-extend to u32
        let byte_extended = bits as u32;
        // Shift so that the high bit is at the desired index.
        // Won't overflow since length > 0.
        let bits_in_position = byte_extended << (31 - E::HIGH_BIT_INDEX);

        // We want to effectively clear out the bits already in place, so we don't want to just |=.
        // Instead, we construct a u32 with all 1s above and below the relevant bits and &=, so that
        // if the new bits are 0, the stored bits will be cleared.

        // avoid overflow when index = 0 -- need zero 1 bits to the left in that case
        let left_1s = u32::MAX.checked_shl(32 - E::HIGH_BIT_INDEX).unwrap_or(0);
        // avoid underflow when index + len = 32 -- zero 1 bits to the right
        let right_1s = u32::MAX.checked_shr(E::HIGH_BIT_INDEX + 1).unwrap_or(0);
        let mask = left_1s | right_1s;
        let bits_for_other_actions = self.bits & mask;
        self.bits = bits_for_other_actions | bits_in_position;
    }

    /// How many bytes (1-3) are needed to represent the set bits, starting from the most
    /// significant bit. The lower bound of 1 is because the unique special case of
    /// an actions field of all zeroes is required by the spec to occupy exactly one byte.
    fn bytes_used(&self) -> usize {
        let bits_used = 32 - self.bits.trailing_zeros();
        let raw_count = (bits_used as usize + 7) / 8;
        if raw_count == 0 {
            1 // Uncommon case - should only be hit for all-zero action bits
        } else {
            raw_count
        }
    }

    /// Return the bits for a given action type as the low bits in the returned u32.
    ///
    /// For example, when extracting the bits `B` from `0bXXXXXXXXXXBBBBBBXXXXXXXXXXXXXXXX`, the
    /// return value will be `0b00000000000000000000000000BBBBBB`.
    pub fn bits_for_type(&self, action_type: ActionType) -> u32 {
        self.bits << action_type.high_bit_index() >> (31)
    }
}

/// Core trait for an individual action
pub trait ActionElement {
    /// The assigned offset for this type from the high bit in the eventual bit sequence of all
    /// actions.
    ///
    /// Each implementation must have a non-conflicting index defined by
    /// [Self::HIGH_BIT_INDEX]
    const HIGH_BIT_INDEX: u32;

    /// Forces implementations to have a matching enum variant so the enum can be kept up to date.
    const ACTION_TYPE: ActionType;

    /// Returns whether this action supports the provided `flavor`.
    ///
    /// Must match the implementations of [ActionElementFlavor].
    fn supports_flavor(flavor: PacketFlavorEnum) -> bool;

    /// Returns the low bit that should be included in the final bit vector
    /// starting at [Self::HIGH_BIT_INDEX].
    fn bits(&self) -> u8;
}

/// Marker trait indicating support for a particular [PacketFlavor].
pub trait ActionElementFlavor<F: PacketFlavor>: ActionElement {}

/// Provides a way to iterate over all action types.
#[derive(Clone, Copy, strum_macros::EnumIter, PartialEq, Eq, Hash, Debug)]
#[allow(missing_docs)]
pub enum ActionType {
    CrossDevSdk,
    CallTransfer,
    ActiveUnlock,
    NearbyShare,
    InstantTethering,
    PhoneHub,
}

impl ActionType {
    /// A u32 with all possible bits for this action type set
    const fn all_bits(&self) -> u32 {
        (u32::MAX << (31_u32)) >> self.high_bit_index()
    }

    /// Get the range of the bits occupied used by this bit index. For example, if the action type
    /// uses the 5th and 6th bits, the returned range will be (5..7).
    /// (0 is the index of the most significant bit).
    #[cfg(feature = "devtools")]
    pub const fn bits_range_for_devtools(&self) -> Range<u32> {
        let high_bit_index = self.high_bit_index();
        high_bit_index..high_bit_index + 1
    }

    const fn high_bit_index(&self) -> u32 {
        match self {
            ActionType::CrossDevSdk => CrossDevSdk::HIGH_BIT_INDEX,
            ActionType::CallTransfer => CallTransfer::HIGH_BIT_INDEX,
            ActionType::ActiveUnlock => ActiveUnlock::HIGH_BIT_INDEX,
            ActionType::NearbyShare => NearbyShare::HIGH_BIT_INDEX,
            ActionType::InstantTethering => InstantTethering::HIGH_BIT_INDEX,
            ActionType::PhoneHub => PhoneHub::HIGH_BIT_INDEX,
        }
    }

    pub(crate) fn supports_flavor(&self, flavor: PacketFlavorEnum) -> bool {
        match self {
            ActionType::CrossDevSdk => CrossDevSdk::supports_flavor(flavor),
            ActionType::CallTransfer => CallTransfer::supports_flavor(flavor),
            ActionType::ActiveUnlock => ActiveUnlock::supports_flavor(flavor),
            ActionType::NearbyShare => NearbyShare::supports_flavor(flavor),
            ActionType::InstantTethering => InstantTethering::supports_flavor(flavor),
            ActionType::PhoneHub => PhoneHub::supports_flavor(flavor),
        }
    }
}

// enabling an element for public adv requires privacy approval due to fingerprinting risk
macros::boolean_element!(CrossDevSdk, 1, plaintext_and_ciphertext);
macros::boolean_element!(CallTransfer, 4, ciphertext_only);
macros::boolean_element!(ActiveUnlock, 8, ciphertext_only);
macros::boolean_element!(NearbyShare, 9, plaintext_and_ciphertext);
macros::boolean_element!(InstantTethering, 10, ciphertext_only);
macros::boolean_element!(PhoneHub, 11, ciphertext_only);
