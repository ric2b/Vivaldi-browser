// Copyright 2024 Google LLC
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

#[cfg(feature = "alloc")]
use alloc::vec::Vec;

use itertools::Itertools;
use nom::error::ErrorKind;
use nom::Err::Error;
use nom::{bytes, combinator, multi};
use sink::Sink;
use tinyvec::ArrayVec;

use crate::array_vec::ArrayVecOption;
use crate::extended::de_type::DeType;
use crate::extended::deserialize::data_element::DataElement;
use crate::extended::serialize::{DeHeader, SingleTypeDataElement, WriteDataElement};
use crate::extended::MAX_DE_LEN;

#[cfg(test)]
mod tests;

pub const MAX_ACTION_ID_VALUE: u16 = 2047;

pub const MAX_ACTIONS_CONTAINER_LENGTH: usize = 64;

// The minimum length of a single container is 2 (one header byte + 1 byte payload) so an actions DE
/// can fit at most 127/2 = 63 total actions containers
pub const MAX_NUM_ACTIONS_CONTAINERS: usize = 63;

/// Represents a valid action ID which can be encoded in an Actions DE
#[derive(Default, Debug, PartialEq, Eq, Clone, Copy, Ord, PartialOrd)]
pub struct ActionId(u16);

impl ActionId {
    /// Gets the u16 value of the action id
    pub fn as_u16(&self) -> u16 {
        self.0
    }
}

// The provided value was not in the range of valid action ids [0, 2047]
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct ActionIdOutOfRange;

impl TryFrom<u16> for ActionId {
    type Error = ActionIdOutOfRange;

    fn try_from(value: u16) -> Result<Self, Self::Error> {
        if value <= MAX_ACTION_ID_VALUE {
            Ok(Self(value))
        } else {
            Err(ActionIdOutOfRange)
        }
    }
}

impl From<ActionId> for u16 {
    fn from(value: ActionId) -> Self {
        value.0
    }
}

#[derive(PartialEq, Eq, Debug, Clone, Copy)]
enum ContainerType {
    DeltaEncoded,
    DeltaEncodedWithOffset,
    BitVectorOffset,
}

#[derive(Debug)]
struct UnknownContainerTypeValue;

impl TryFrom<u8> for ContainerType {
    type Error = UnknownContainerTypeValue;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0b00 => Ok(ContainerType::DeltaEncoded),
            0b01 => Ok(ContainerType::DeltaEncodedWithOffset),
            0b10 => Ok(ContainerType::BitVectorOffset),
            _ => Err(UnknownContainerTypeValue),
        }
    }
}

impl From<ContainerType> for u8 {
    fn from(value: ContainerType) -> Self {
        match value {
            ContainerType::DeltaEncoded => 0b00,
            ContainerType::DeltaEncodedWithOffset => 0b01,
            ContainerType::BitVectorOffset => 0b10,
        }
    }
}

/// Actions data element parsed from a slice of bytes, this type references the original slice of
/// bytes which was parsed
#[derive(Debug)]
pub struct DeserializedActionsDE<'adv> {
    containers: ArrayVecOption<DeserializedActionsContainer<'adv>, MAX_NUM_ACTIONS_CONTAINERS>,
}

impl<'adv> SingleTypeDataElement for DeserializedActionsDE<'adv> {
    const DE_TYPE: DeType = DeType::const_from(0x06);
}

#[derive(Debug, Eq, PartialEq)]
pub enum ActionsDeserializationError {
    InvalidTypeCode,
    InvalidContainerLength,
    InvalidContainerType,
    GenericDeserializationError,
}

/// Convert a deserialized DE into one you can serialize
impl<'a> TryFrom<&'a DataElement<'a>> for DeserializedActionsDE<'a> {
    type Error = ActionsDeserializationError;

    fn try_from(value: &'a DataElement<'a>) -> Result<Self, Self::Error> {
        if value.de_type() != Self::DE_TYPE {
            return Err(ActionsDeserializationError::InvalidTypeCode);
        }
        DeserializedActionsDE::deserialize(value.contents()).map_err(|e| match e {
            Error(e) => match e.code {
                ErrorKind::Eof | ErrorKind::Verify => {
                    ActionsDeserializationError::InvalidContainerLength
                }
                ErrorKind::MapOpt => ActionsDeserializationError::InvalidContainerType,
                _ => ActionsDeserializationError::GenericDeserializationError,
            },
            _ => ActionsDeserializationError::GenericDeserializationError,
        })
    }
}

impl<'adv> DeserializedActionsDE<'adv> {
    /// Returns a collection over of all action ids contained within the data element
    #[cfg(feature = "alloc")]
    pub fn collect_action_ids(&self) -> Vec<Result<ActionId, ActionIdOutOfRange>> {
        let mut result = Vec::new();
        self.containers.iter().for_each(|container| {
            container.iter_action_ids().for_each(|action| result.push(action))
        });
        result
    }

    /// Parses the raw bytes of an Actions DE into an intermediate format, that is the contents
    /// are separated into containers with their corresponding container type, but no further decoding
    /// is done on the contents of the containers, leaving the bytes in their compact encoded format.
    fn deserialize(
        de_contents: &'adv [u8],
    ) -> Result<DeserializedActionsDE, nom::Err<nom::error::Error<&[u8]>>> {
        combinator::all_consuming(multi::fold_many_m_n(
            1,
            MAX_NUM_ACTIONS_CONTAINERS,
            DeserializedActionsContainer::parse,
            Self::new_empty,
            |mut acc, item| {
                acc.add_container(item);
                acc
            },
        ))(de_contents)
        .map(|(rem, actions)| {
            debug_assert!(rem.is_empty());
            actions
        })
    }

    /// Initializes an actions de with empty contents
    fn new_empty() -> Self {
        let internal = ArrayVecOption::default();
        Self { containers: internal }
    }

    /// Appends a container to the DE, panicking in the case where the max length is exceeded
    fn add_container(&mut self, container: DeserializedActionsContainer<'adv>) {
        self.containers.push(container);
    }
}

#[derive(Debug)]
struct DeserializedActionsContainer<'adv> {
    container_type: ContainerType,
    offset: u16,
    payload: &'adv [u8],
}

impl<'adv> DeserializedActionsContainer<'adv> {
    fn parse(bytes: &'adv [u8]) -> nom::IResult<&[u8], Self> {
        let (input, (container_type, container_encoded_len)) =
            combinator::map_opt(nom::number::complete::u8, |b| {
                // right shift by 6 to obtain the upper 2 type bits TTLLLLLL
                let type_value = b >> 6;
                let encoded_len = b & 0b00111111;
                ContainerType::try_from(type_value)
                    .map(|container_type| (container_type, encoded_len))
                    .ok()
            })(bytes)?;

        let (input, payload_bytes) = bytes::complete::take(container_encoded_len + 1)(input)?;

        let (payload, offset) = match container_type {
            ContainerType::DeltaEncoded => (payload_bytes, 0u16),
            // A container which only contains the sub header offset and no
            // subsequent data is disallowed by the spec
            ContainerType::DeltaEncodedWithOffset | ContainerType::BitVectorOffset => {
                combinator::verify(nom::number::complete::u8, |_| payload_bytes.len() > 1)(
                    payload_bytes,
                )
                .map(|(remaining, byte)| (remaining, u16::from(byte) * 8))?
            }
        };
        Ok((input, DeserializedActionsContainer { container_type, offset, payload }))
    }

    fn iter_action_ids(&self) -> ActionsContainerIterator {
        ActionsContainerIterator::new(self)
    }
}

/// Iterates all the action ids in a container
enum ActionsContainerIterator<'c> {
    DeltaEncoded { first: bool, delta: u16, bytes: &'c [u8] },
    BitVector { current_offset: u16, position_in_byte: u8, current_byte: u8, bytes: &'c [u8] },
}

impl<'c> ActionsContainerIterator<'c> {
    fn new(container: &'c DeserializedActionsContainer) -> Self {
        match container.container_type {
            ContainerType::DeltaEncoded | ContainerType::DeltaEncodedWithOffset => {
                Self::DeltaEncoded {
                    delta: container.offset,
                    bytes: container.payload,
                    first: true,
                }
            }
            ContainerType::BitVectorOffset => {
                let (remaining, byte) =
                    nom::number::complete::u8::<&[u8], nom::error::Error<_>>(container.payload)
                        .expect(
                        "we verified bytes has at least one byte when parsing original container",
                    );
                Self::BitVector {
                    current_offset: container.offset,
                    position_in_byte: 0,
                    current_byte: byte,
                    bytes: remaining,
                }
            }
        }
    }
}

impl<'c> Iterator for ActionsContainerIterator<'c> {
    type Item = Result<ActionId, ActionIdOutOfRange>;

    fn next(&mut self) -> Option<Self::Item> {
        match self {
            ActionsContainerIterator::DeltaEncoded { delta, bytes, first } => {
                nom::number::complete::u8::<&[u8], nom::error::Error<_>>(bytes)
                    .map(|(rem, b)| {
                        let mut result = u16::from(b) + *delta;
                        if *first {
                            *first = false;
                        } else {
                            result += 1;
                        }
                        // save the result to be used as the next delta
                        *delta = result;
                        // update the slice to no longer include the current byte since we just used it
                        *bytes = rem;
                        result.try_into()
                    })
                    .ok()
            }
            ActionsContainerIterator::BitVector {
                current_offset,
                position_in_byte,
                current_byte,
                bytes,
            } => {
                while *position_in_byte <= 7 || !bytes.is_empty() {
                    // take another byte and update position and offset
                    if *position_in_byte > 7 && !bytes.is_empty() {
                        let (remaining, byte) =
                            nom::number::complete::u8::<&[u8], nom::error::Error<_>>(bytes)
                                .expect("we verified bytes has at least one byte");
                        *current_byte = byte;
                        *bytes = remaining;
                        *current_offset += 8;
                        *position_in_byte = 0
                    }
                    if ((0b10000000 >> *position_in_byte) & *current_byte) != 0 {
                        let result: Option<Result<ActionId, _>> =
                            Some((*current_offset + u16::from(*position_in_byte)).try_into());
                        *position_in_byte += 1;
                        return result;
                    }
                    *position_in_byte += 1;
                }
                None
            }
        }
    }
}
/// Actions data element consists of one or more containers
#[derive(Debug)]
pub struct ActionsDataElement {
    containers: ArrayVecOption<ActionsContainer, MAX_NUM_ACTIONS_CONTAINERS>,
    len: usize,
}

impl ActionsDataElement {
    /// Creates an Actions DE storing the provided action ids in memory using the default encoding
    /// scheme of simple delta encoding. This limits the amount of encodable actions to 64, and the
    /// action id range to what can fit in a byte after being delta encoded.
    // TODO: provide more flexibility over which encoding scheme is used
    pub fn try_from_actions(
        actions: ArrayVec<[ActionId; 64]>,
    ) -> Result<Self, ActionsDataElementError> {
        DeltaEncodedContainer::try_from_actions(actions).map(|c| {
            let mut actions_de = ActionsDataElement::new_empty();
            // This will always succeed since we are only adding one container which cannot exceed
            // the 127 max DE length
            let res = actions_de.try_add_container(c.into());
            debug_assert!(res.is_none());
            actions_de
        })
    }

    /// Initializes an actions de with empty contents
    fn new_empty() -> Self {
        let internal = ArrayVecOption::default();
        Self { containers: internal, len: 0 }
    }

    /// Appends a container to the DE, returning back the container in the event that max length of
    /// a DE would be exceeded or returning `None` in the case that it has been successfully added
    fn try_add_container(&mut self, container: ActionsContainer) -> Option<ActionsContainer> {
        if self.len + container.payload.len() + 1 > MAX_DE_LEN {
            Some(container)
        } else {
            self.len = self.len + container.payload.len() + 1;
            self.containers.push(container);
            None
        }
    }
}

/// The actions container type saved in the DE which can represent any of the 3 existing container
/// types as specified in its container_type field. This can be converted into from any of the 3
/// more specific actions container types and is what is stored in `ActionsDataElement`
#[derive(Debug, Copy, Clone)]
struct ActionsContainer {
    container_type: ContainerType,
    payload: ArrayVec<[u8; MAX_ACTIONS_CONTAINER_LENGTH]>,
}

trait ContainerEncoder {
    const CONTAINER_TYPE: ContainerType;
    /// encodes the bytes of the container after the initial container header
    fn encoded_payload(&self) -> ArrayVec<[u8; MAX_ACTIONS_CONTAINER_LENGTH]>;
}

impl<C: ContainerEncoder> From<C> for ActionsContainer {
    fn from(value: C) -> Self {
        Self { container_type: C::CONTAINER_TYPE, payload: value.encoded_payload() }
    }
}

#[derive(Debug)]
struct DeltaEncodedContainer {
    payload: ArrayVec<[u8; MAX_ACTIONS_CONTAINER_LENGTH]>,
}

impl DeltaEncodedContainer {
    fn try_from_actions(
        actions: ArrayVec<[ActionId; MAX_ACTIONS_CONTAINER_LENGTH]>,
    ) -> Result<Self, ActionsDataElementError> {
        let sorted = sort(actions);
        let mut payload = ArrayVec::<[u8; MAX_ACTIONS_CONTAINER_LENGTH]>::default();
        let first =
            u8::try_from(sorted.first().ok_or(ActionsDataElementError::EmptyActions)?.as_u16())
                .map_err(|_| ActionsDataElementError::ActionIdDeltaOverflow)?;
        payload.push(first);
        let remaining = delta_encoding(sorted)?;
        payload.extend_from_slice(remaining.as_slice());
        Ok(Self { payload })
    }
}

impl ContainerEncoder for DeltaEncodedContainer {
    const CONTAINER_TYPE: ContainerType = ContainerType::DeltaEncoded;
    fn encoded_payload(&self) -> ArrayVec<[u8; MAX_ACTIONS_CONTAINER_LENGTH]> {
        self.payload
    }
}

fn sort<const N: usize>(actions: ArrayVec<[ActionId; N]>) -> ArrayVec<[ActionId; N]> {
    let mut sorted = actions;
    sorted.sort_unstable_by_key(|x| *x);
    sorted
}

/// Calculates the delta encoded for the given sorted collection offset by 1, so an encoding of
/// 0 represents a delta of 1, and an encoding of 0xFF represents a delta of 256
fn delta_encoding<const N: usize>(
    sorted: ArrayVec<[ActionId; N]>,
) -> Result<ArrayVec<[u8; N]>, ActionsDataElementError> {
    sorted
        .iter()
        .tuple_windows()
        .filter(|(a, b)| *a != *b)
        .map(|(a, b)| u8::try_from(b.as_u16() - (a.as_u16() + 1)))
        .collect::<Result<ArrayVec<[u8; N]>, _>>()
        .map_err(|_| ActionsDataElementError::ActionIdDeltaOverflow)
}

#[derive(Debug)]
struct DeltaEncodedOffsetContainer {
    payload: ArrayVec<[u8; MAX_ACTIONS_CONTAINER_LENGTH]>,
}

impl DeltaEncodedOffsetContainer {
    #[allow(unused)]
    fn try_from_actions(
        actions: ArrayVec<[ActionId; MAX_ACTIONS_CONTAINER_LENGTH - 1]>,
    ) -> Result<Self, ActionsDataElementError> {
        let sorted = sort(actions);
        let first_action = sorted.first().ok_or(ActionsDataElementError::EmptyActions)?.as_u16();
        let mut payload = ArrayVec::<[u8; MAX_ACTIONS_CONTAINER_LENGTH]>::default();
        let offset = u8::try_from(first_action / 8)
            .expect("Max action id 2047 divided by 8 is always within range of a valid u8");
        payload.push(offset);
        let first_action_encoding = u8::try_from(first_action - u16::from(offset) * 8)
            .expect("This will always fit into a u8 because of the offset subtraction");
        payload.push(first_action_encoding);

        let remaining = delta_encoding(sorted)?;
        payload.extend_from_slice(remaining.as_slice());
        Ok(Self { payload })
    }
}

impl ContainerEncoder for DeltaEncodedOffsetContainer {
    const CONTAINER_TYPE: ContainerType = ContainerType::DeltaEncodedWithOffset;
    fn encoded_payload(&self) -> ArrayVec<[u8; MAX_ACTIONS_CONTAINER_LENGTH]> {
        self.payload
    }
}

/// The maximum amount of expressible action_ids in a single bit vector container.
/// Each byte can hold 8 unique actions with a max of 64 bytes per container.
const MAX_BIT_VECTOR_ACTIONS: usize = MAX_ACTIONS_CONTAINER_LENGTH * 8;

#[derive(Debug)]
struct BitVectorOffsetContainer {
    payload: ArrayVec<[u8; MAX_ACTIONS_CONTAINER_LENGTH]>,
}

impl BitVectorOffsetContainer {
    #[allow(unused)]
    fn try_from_actions(actions: &mut [ActionId]) -> Result<Self, ActionsDataElementError> {
        if actions.len() > MAX_BIT_VECTOR_ACTIONS {
            return Err(ActionsDataElementError::TooManyActions);
        }
        actions.sort_unstable_by_key(|x| *x);

        let mut payload = ArrayVec::<[u8; MAX_ACTIONS_CONTAINER_LENGTH]>::new();
        let first = actions.first().ok_or(ActionsDataElementError::EmptyActions)?.as_u16();
        let offset = u8::try_from(first / 8)
            .expect("Max action id 2047 divided by 8 is always within range of a valid u8");
        payload.push(offset);

        let max_value = actions.last().expect("we have verified above that sorted is not empty");
        let bytes_required = (max_value.as_u16() / 8) + 1 - u16::from(offset);
        for _ in 0..bytes_required {
            if payload.try_push(0).is_some() {
                return Err(ActionsDataElementError::ActionIdOutOfRange);
            }
        }
        actions.iter().for_each(|a| {
            // which byte this action id belongs in
            let index = usize::from(a.as_u16() / 8 + 1 - u16::from(offset));
            // how far the action is shifted from the most significant bit in the byte
            let shift = a.as_u16() % 8;
            payload[index] |= 0b1000_0000 >> shift;
        });

        Ok(Self { payload })
    }
}

impl ContainerEncoder for BitVectorOffsetContainer {
    const CONTAINER_TYPE: ContainerType = ContainerType::BitVectorOffset;
    fn encoded_payload(&self) -> ArrayVec<[u8; MAX_ACTIONS_CONTAINER_LENGTH]> {
        self.payload
    }
}

/// Errors that can occur constructing an [ActionsDataElement].
#[derive(Debug, PartialEq, Eq)]
#[allow(unused)]
pub enum ActionsDataElementError {
    /// Must specify at least one action id to encode
    EmptyActions,
    /// The provided ActionIds cannot be delta encoded into u8 values
    ActionIdDeltaOverflow,
    /// The provided range of action ids cannot be encoded into a single container
    ActionIdOutOfRange,
    /// Too many actions provided than what can be encoded into a single container
    TooManyActions,
}

impl SingleTypeDataElement for ActionsDataElement {
    const DE_TYPE: DeType = DeType::const_from(0x06);
}

impl WriteDataElement for ActionsDataElement {
    fn de_header(&self) -> DeHeader {
        DeHeader::new(
            Self::DE_TYPE,
            // each containers length is the header byte +  the length of its encoded contents
            self.containers
                .iter()
                .map(|a| 1 + a.payload.len())
                .sum::<usize>()
                .try_into()
                .expect("An actions DE will always be <= 127, this is enforced upon creation"),
        )
    }

    fn write_de_contents<S: Sink<u8>>(&self, sink: &mut S) -> Option<()> {
        // write actions container header and bytes for each container in the de
        let mut encoded_actions_de_bytes = ArrayVec::<[u8; MAX_DE_LEN]>::new();
        self.containers.iter().for_each(|a| {
            let header_byte = (u8::from(a.container_type) << 6) | ((a.payload.len() as u8) - 1);
            // This will not panic because the length of the actions DE is checked during creation
            // to not exceed the max limit of 127
            encoded_actions_de_bytes.extend_from_slice(&[header_byte]);
            encoded_actions_de_bytes.extend_from_slice(a.payload.as_slice());
        });
        sink.try_extend_from_slice(encoded_actions_de_bytes.as_slice())
    }
}
