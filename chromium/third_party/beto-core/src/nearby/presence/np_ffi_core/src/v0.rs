// Copyright 2023 Google LLC
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

//! Common externally-acessible V0 constructs for both of the
//! serialization+deserialization flows.

use crate::common::InvalidStackDataStructure;
use crate::serialize::AdvertisementBuilderKind;
use crate::utils::FfiEnum;
use np_adv::legacy::actions::ActionsDataElement;
use np_adv::legacy::{data_elements as np_adv_de, Ciphertext, PacketFlavorEnum, Plaintext};

/// Discriminant for `V0DataElement`.
#[repr(u8)]
pub enum V0DataElementKind {
    /// A transmission Power (Tx Power) data-element.
    /// The associated payload may be obtained via
    /// `V0DataElement#into_tx_power`.
    TxPower = 0,
    /// The Actions data-element.
    /// The associated payload may be obtained via
    /// `V0DataElement#into_actions`.
    Actions = 1,
}

/// Representation of a V0 data element.
#[repr(C)]
#[allow(missing_docs)]
#[derive(Clone)]
pub enum V0DataElement {
    TxPower(TxPower),
    Actions(V0Actions),
}

impl TryFrom<V0DataElement> for np_adv_dynamic::legacy::ToBoxedDataElementBundle {
    type Error = InvalidStackDataStructure;
    fn try_from(de: V0DataElement) -> Result<Self, InvalidStackDataStructure> {
        match de {
            V0DataElement::TxPower(x) => x.try_into(),
            V0DataElement::Actions(x) => x.try_into(),
        }
    }
}

impl<F: np_adv::legacy::PacketFlavor> From<np_adv::legacy::deserialize::PlainDataElement<F>>
    for V0DataElement
{
    fn from(de: np_adv::legacy::deserialize::PlainDataElement<F>) -> Self {
        use np_adv::legacy::deserialize::PlainDataElement;
        match de {
            PlainDataElement::Actions(x) => V0DataElement::Actions(x.into()),
            PlainDataElement::TxPower(x) => V0DataElement::TxPower(x.into()),
        }
    }
}

impl FfiEnum for V0DataElement {
    type Kind = V0DataElementKind;
    fn kind(&self) -> Self::Kind {
        match self {
            V0DataElement::Actions(_) => V0DataElementKind::Actions,
            V0DataElement::TxPower(_) => V0DataElementKind::TxPower,
        }
    }
}

impl V0DataElement {
    declare_enum_cast! {into_tx_power, TxPower, TxPower}
    declare_enum_cast! {into_actions, Actions, V0Actions}
}

/// Discriminant for `BuildTxPowerResult`.
#[repr(u8)]
#[derive(Clone, Copy)]
pub enum BuildTxPowerResultKind {
    /// The transmission power was outside the
    /// allowed -100dBm to 20dBm range.
    OutOfRange = 0,
    /// The transmission power was in range,
    /// and so a `TxPower` struct was constructed.
    Success = 1,
}

/// Result type for attempting to construct a
/// Tx Power from a signed byte.
#[repr(C)]
#[allow(missing_docs)]
pub enum BuildTxPowerResult {
    OutOfRange,
    Success(TxPower),
}

impl FfiEnum for BuildTxPowerResult {
    type Kind = BuildTxPowerResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            Self::OutOfRange => BuildTxPowerResultKind::OutOfRange,
            Self::Success(_) => BuildTxPowerResultKind::Success,
        }
    }
}

impl BuildTxPowerResult {
    declare_enum_cast! {into_success, Success, TxPower}
}

/// Representation of a transmission power,
/// as used for the Tx Power DE in V0 and V1.
#[derive(Clone)]
#[repr(C)]
pub struct TxPower {
    tx_power: i8,
}

impl TxPower {
    /// Attempts to construct a new TxPower from
    /// the given signed-byte value.
    pub fn build_from_signed_byte(tx_power: i8) -> BuildTxPowerResult {
        match np_adv::shared_data::TxPower::try_from(tx_power) {
            Ok(_) => BuildTxPowerResult::Success(Self { tx_power }),
            Err(_) => BuildTxPowerResult::OutOfRange,
        }
    }
    /// Yields this Tx Power value as an i8.
    pub fn as_i8(&self) -> i8 {
        self.tx_power
    }
}

impl From<np_adv_de::TxPowerDataElement> for TxPower {
    fn from(de: np_adv_de::TxPowerDataElement) -> Self {
        Self { tx_power: de.tx_power_value() }
    }
}

impl TryFrom<TxPower> for np_adv_dynamic::legacy::ToBoxedDataElementBundle {
    type Error = InvalidStackDataStructure;
    fn try_from(value: TxPower) -> Result<Self, InvalidStackDataStructure> {
        np_adv::shared_data::TxPower::try_from(value.as_i8())
            .map_err(|_| InvalidStackDataStructure)
            .map(|x| x.into())
    }
}

/// Discriminant for `BuildContextSyncSeqNumResult`.
#[repr(u8)]
#[derive(Clone, Copy)]
pub enum BuildContextSyncSeqNumResultKind {
    /// The sequence number was outside the allowed
    /// 0-15 single-nibble range.
    OutOfRange = 0,
    /// The sequence number was in range,
    /// and so a `ContextSyncSeqNum` was constructed.
    Success = 1,
}

/// Result type for attempting to construct a
/// ContextSyncSeqNum from an unsigned byte.
#[repr(C)]
#[allow(missing_docs)]
pub enum BuildContextSyncSeqNumResult {
    OutOfRange,
    Success(ContextSyncSeqNum),
}

impl FfiEnum for BuildContextSyncSeqNumResult {
    type Kind = BuildContextSyncSeqNumResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            Self::OutOfRange => BuildContextSyncSeqNumResultKind::OutOfRange,
            Self::Success(_) => BuildContextSyncSeqNumResultKind::Success,
        }
    }
}

impl BuildContextSyncSeqNumResult {
    declare_enum_cast! {into_success, Success, ContextSyncSeqNum}
}
/// Representation of a context-sync sequence number.
#[derive(Clone, Copy)]
#[repr(C)]
pub struct ContextSyncSeqNum {
    value: u8,
}

impl ContextSyncSeqNum {
    /// Attempts to build a new context sync sequence number
    /// from the given unsigned byte.
    pub fn build_from_unsigned_byte(value: u8) -> BuildContextSyncSeqNumResult {
        match np_adv::shared_data::ContextSyncSeqNum::try_from(value) {
            Ok(_) => BuildContextSyncSeqNumResult::Success(Self { value }),
            Err(_) => BuildContextSyncSeqNumResult::OutOfRange,
        }
    }
    /// Yields this context sync sequence number
    /// as a u8.
    pub fn as_u8(&self) -> u8 {
        self.value
    }
}

impl From<np_adv::shared_data::ContextSyncSeqNum> for ContextSyncSeqNum {
    fn from(value: np_adv::shared_data::ContextSyncSeqNum) -> Self {
        let value = value.as_u8();
        Self { value }
    }
}

impl TryFrom<ContextSyncSeqNum> for np_adv_dynamic::legacy::ToBoxedActionElement {
    type Error = InvalidStackDataStructure;
    fn try_from(value: ContextSyncSeqNum) -> Result<Self, Self::Error> {
        let value = value.as_u8();
        np_adv::shared_data::ContextSyncSeqNum::try_from(value)
            .map(np_adv_dynamic::legacy::ToBoxedActionElement::ContextSyncSeqNum)
            .map_err(|_| InvalidStackDataStructure)
    }
}

/// Representation of the Actions DE in V0.
#[derive(Clone, Copy)]
#[repr(C)]
pub enum V0Actions {
    /// A set of action bits which were present in a plaintext identity advertisement
    Plaintext(V0ActionBits),
    /// A set of action bits which were present in a encrypted identity advertisement
    Encrypted(V0ActionBits),
}

impl TryFrom<V0Actions> for np_adv_dynamic::legacy::ToBoxedDataElementBundle {
    type Error = InvalidStackDataStructure;
    fn try_from(value: V0Actions) -> Result<Self, InvalidStackDataStructure> {
        let boxed_action_bits = np_adv_dynamic::legacy::BoxedActionBits::try_from(value)?;
        Ok(boxed_action_bits.into())
    }
}

impl<F: np_adv::legacy::PacketFlavor> From<np_adv::legacy::actions::ActionsDataElement<F>>
    for V0Actions
{
    fn from(value: ActionsDataElement<F>) -> Self {
        match F::ENUM_VARIANT {
            PacketFlavorEnum::Plaintext => {
                Self::Plaintext(V0ActionBits { bitfield: value.action.as_u32() })
            }
            PacketFlavorEnum::Ciphertext => {
                Self::Encrypted(V0ActionBits { bitfield: value.action.as_u32() })
            }
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
/// The bitfield data of a VOActions data element
pub struct V0ActionBits {
    bitfield: u32,
}

#[derive(Clone, Copy)]
#[allow(missing_docs)]
#[repr(u8)]
/// The possible boolean action types which can be present in an Actions data element
pub enum BooleanActionType {
    ActiveUnlock = 8,
    NearbyShare = 9,
    InstantTethering = 10,
    PhoneHub = 11,
    PresenceManager = 12,
    Finder = 13,
    FastPairSass = 14,
}

impl BooleanActionType {
    pub(crate) fn as_boxed_action_element(
        &self,
        value: bool,
    ) -> np_adv_dynamic::legacy::ToBoxedActionElement {
        use np_adv_dynamic::legacy::ToBoxedActionElement;
        match self {
            Self::ActiveUnlock => ToBoxedActionElement::ActiveUnlock(value),
            Self::NearbyShare => ToBoxedActionElement::NearbyShare(value),
            Self::InstantTethering => ToBoxedActionElement::InstantTethering(value),
            Self::PhoneHub => ToBoxedActionElement::PhoneHub(value),
            Self::PresenceManager => ToBoxedActionElement::PresenceManager(value),
            Self::Finder => ToBoxedActionElement::Finder(value),
            Self::FastPairSass => ToBoxedActionElement::FastPairSass(value),
        }
    }
}

impl From<BooleanActionType> for np_adv::legacy::actions::ActionType {
    fn from(value: BooleanActionType) -> Self {
        match value {
            BooleanActionType::ActiveUnlock => np_adv::legacy::actions::ActionType::ActiveUnlock,
            BooleanActionType::NearbyShare => np_adv::legacy::actions::ActionType::NearbyShare,
            BooleanActionType::InstantTethering => {
                np_adv::legacy::actions::ActionType::InstantTethering
            }
            BooleanActionType::PhoneHub => np_adv::legacy::actions::ActionType::PhoneHub,
            BooleanActionType::Finder => np_adv::legacy::actions::ActionType::Finder,
            BooleanActionType::FastPairSass => np_adv::legacy::actions::ActionType::FastPairSass,
            BooleanActionType::PresenceManager => {
                np_adv::legacy::actions::ActionType::PresenceManager
            }
        }
    }
}

impl From<np_adv_dynamic::legacy::BoxedActionBits> for V0Actions {
    fn from(bits: np_adv_dynamic::legacy::BoxedActionBits) -> Self {
        use np_adv_dynamic::legacy::BoxedActionBits;
        match bits {
            BoxedActionBits::Plaintext(x) => Self::Plaintext(V0ActionBits { bitfield: x.as_u32() }),
            BoxedActionBits::Ciphertext(x) => {
                Self::Encrypted(V0ActionBits { bitfield: x.as_u32() })
            }
        }
    }
}

impl TryFrom<V0Actions> for np_adv_dynamic::legacy::BoxedActionBits {
    type Error = InvalidStackDataStructure;
    fn try_from(actions: V0Actions) -> Result<Self, InvalidStackDataStructure> {
        match actions {
            V0Actions::Plaintext(action_bits) => {
                let bits = np_adv::legacy::actions::ActionBits::<Plaintext>::try_from(
                    action_bits.bitfield,
                )
                .map_err(|_| InvalidStackDataStructure)?;
                Ok(bits.into())
            }
            V0Actions::Encrypted(action_bits) => {
                let bits = np_adv::legacy::actions::ActionBits::<Ciphertext>::try_from(
                    action_bits.bitfield,
                )
                .map_err(|_| InvalidStackDataStructure)?;
                Ok(bits.into())
            }
        }
    }
}

/// Discriminant for `SetV0ActionResult`.
#[repr(u8)]
pub enum SetV0ActionResultKind {
    /// The attempt to set the action bit failed. The
    /// action bits were yielded back to the caller,
    /// unmodified.
    Error = 0,
    /// The attempt to set the action bit succeeded.
    /// The updated action bits were yielded back to the caller.
    Success = 1,
}

/// The result of attempting to set a particular action
/// bit on some `V0Actions`.
#[repr(C)]
#[allow(missing_docs)]
pub enum SetV0ActionResult {
    Success(V0Actions),
    Error(V0Actions),
}

impl FfiEnum for SetV0ActionResult {
    type Kind = SetV0ActionResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            Self::Success(_) => SetV0ActionResultKind::Success,
            Self::Error(_) => SetV0ActionResultKind::Error,
        }
    }
}

impl SetV0ActionResult {
    declare_enum_cast! { into_success, Success, V0Actions }
    declare_enum_cast! { into_error, Error, V0Actions }
}

impl V0Actions {
    /// Constructs a new V0 actions DE with no declared boolean
    /// actions and a zeroed context sync sequence number,
    /// where the DE is intended for the given advertisement
    /// kind (plaintext/encrypted).
    pub fn new_zeroed(kind: AdvertisementBuilderKind) -> Self {
        match kind {
            AdvertisementBuilderKind::Public => Self::Plaintext(V0ActionBits { bitfield: 0 }),
            AdvertisementBuilderKind::Encrypted => Self::Encrypted(V0ActionBits { bitfield: 0 }),
        }
    }

    /// Gets the V0 Action bits as represented by a u32 where the last 8 bits are
    /// always 0 since V0 actions can only hold up to 24 bits.
    pub fn as_u32(&self) -> u32 {
        match self {
            V0Actions::Plaintext(bits) => bits.bitfield,
            V0Actions::Encrypted(bits) => bits.bitfield,
        }
    }

    /// Return whether a boolean action type is set in this data element
    #[allow(clippy::expect_used)]
    pub fn has_action(
        &self,
        action_type: BooleanActionType,
    ) -> Result<bool, InvalidStackDataStructure> {
        let boxed_action_bits = np_adv_dynamic::legacy::BoxedActionBits::try_from(*self)?;
        let action_type = action_type.into();
        Ok(boxed_action_bits.has_action(&action_type).expect("BooleanActionType only has one bit"))
    }

    /// Attempts to set the given action bit to the given boolean value.
    /// This operation may fail if the requested action bit may not be
    /// set for the kind of containing advertisement (public/encrypted)
    /// that this action DE is intended to belong to. In this case,
    /// the original action bits will be yielded back to the caller,
    /// unaltered.
    pub fn set_action(
        self,
        action_type: BooleanActionType,
        value: bool,
    ) -> Result<SetV0ActionResult, InvalidStackDataStructure> {
        let mut boxed_action_bits = np_adv_dynamic::legacy::BoxedActionBits::try_from(self)?;
        let boxed_action_element = action_type.as_boxed_action_element(value);
        match boxed_action_bits.set_action(boxed_action_element) {
            Ok(()) => Ok(SetV0ActionResult::Success(boxed_action_bits.into())),
            Err(_) => Ok(SetV0ActionResult::Error(self)),
        }
    }

    /// Sets the context sequence number for this data element.
    #[allow(clippy::expect_used)]
    pub fn set_context_sync_seq_num(
        self,
        value: ContextSyncSeqNum,
    ) -> Result<Self, InvalidStackDataStructure> {
        let mut boxed_action_bits = np_adv_dynamic::legacy::BoxedActionBits::try_from(self)?;
        let boxed_action_element = np_adv_dynamic::legacy::ToBoxedActionElement::try_from(value)?;
        boxed_action_bits
            .set_action(boxed_action_element)
            .expect("Context sync sequence number always may be present");
        Ok(boxed_action_bits.into())
    }

    /// Return the context sequence number from this data element
    #[allow(clippy::expect_used)]
    pub fn get_context_sync_seq_num(&self) -> Result<ContextSyncSeqNum, InvalidStackDataStructure> {
        let boxed_action_bits = np_adv_dynamic::legacy::BoxedActionBits::try_from(*self)?;
        Ok(boxed_action_bits.get_context_sync_seq_num().into())
    }
}
