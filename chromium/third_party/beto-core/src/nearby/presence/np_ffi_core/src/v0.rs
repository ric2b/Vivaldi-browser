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
use np_adv::{
    legacy::data_elements::actions::ActionsDataElement,
    legacy::{data_elements as np_adv_de, Ciphertext, PacketFlavorEnum, Plaintext},
};
use strum::IntoEnumIterator;

/// Discriminant for `V0DataElement`.
#[repr(u8)]
pub enum V0DataElementKind {
    /// A transmission Power (Tx Power) data-element.
    /// The associated payload may be obtained via
    /// `V0DataElement#into_tx_power`.
    TxPower = 1,
    /// The Actions data-element.
    /// The associated payload may be obtained via
    /// `V0DataElement#into_actions`.
    Actions = 2,
}

/// Representation of a V0 data element.
#[repr(C)]
#[allow(missing_docs)]
#[derive(Clone)]
pub enum V0DataElement {
    TxPower(TxPower),
    Actions(V0Actions),
}

impl TryFrom<V0DataElement> for np_adv_dynamic::legacy::ToBoxedSerializeDataElement {
    type Error = InvalidStackDataStructure;
    fn try_from(de: V0DataElement) -> Result<Self, InvalidStackDataStructure> {
        match de {
            V0DataElement::TxPower(x) => x.try_into(),
            V0DataElement::Actions(x) => x.try_into(),
        }
    }
}

impl<F: np_adv::legacy::PacketFlavor> From<np_adv::legacy::deserialize::DeserializedDataElement<F>>
    for V0DataElement
{
    fn from(de: np_adv::legacy::deserialize::DeserializedDataElement<F>) -> Self {
        use np_adv::legacy::deserialize::DeserializedDataElement;
        match de {
            DeserializedDataElement::Actions(x) => V0DataElement::Actions(x.into()),
            DeserializedDataElement::TxPower(x) => V0DataElement::TxPower(x.into()),
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
    /// Attempts to construct a new TxPower from the given signed-byte value.
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

impl From<np_adv_de::tx_power::TxPowerDataElement> for TxPower {
    fn from(de: np_adv_de::tx_power::TxPowerDataElement) -> Self {
        Self { tx_power: de.tx_power_value() }
    }
}

impl TryFrom<TxPower> for np_adv_dynamic::legacy::ToBoxedSerializeDataElement {
    type Error = InvalidStackDataStructure;
    fn try_from(value: TxPower) -> Result<Self, InvalidStackDataStructure> {
        np_adv::shared_data::TxPower::try_from(value.as_i8())
            .map_err(|_| InvalidStackDataStructure)
            .map(|x| x.into())
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

impl TryFrom<V0Actions> for np_adv_dynamic::legacy::ToBoxedSerializeDataElement {
    type Error = InvalidStackDataStructure;
    fn try_from(value: V0Actions) -> Result<Self, InvalidStackDataStructure> {
        let boxed_action_bits = np_adv_dynamic::legacy::BoxedActionBits::try_from(value)?;
        Ok(boxed_action_bits.into())
    }
}

impl<F: np_adv::legacy::PacketFlavor> From<ActionsDataElement<F>> for V0Actions {
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
/// The bitfield data of a V0Actions data element
pub struct V0ActionBits {
    bitfield: u32,
}

impl From<u32> for V0ActionBits {
    fn from(bitfield: u32) -> Self {
        // No need to validate here. Validation of this struct is done at all places where it is
        // taken as a parameter. See [`InvalidStackDataStructure`]. Since this struct is
        // `#[repr(C)]` if rules were enforced here, they could be broken by a foreign language
        // user.
        Self { bitfield }
    }
}

#[derive(Clone, Copy, strum_macros::EnumIter)]
#[allow(missing_docs)]
#[repr(u8)]
/// The possible boolean action types which can be present in an Actions data element
pub enum ActionType {
    CrossDevSdk = 1,
    CallTransfer = 4,
    ActiveUnlock = 8,
    NearbyShare = 9,
    InstantTethering = 10,
    PhoneHub = 11,
}

#[derive(Clone, Copy, Debug)]
/// The given int is out of range for conversion.
pub struct TryFromIntError;

impl From<core::num::TryFromIntError> for TryFromIntError {
    fn from(_: core::num::TryFromIntError) -> Self {
        Self
    }
}

impl TryFrom<u8> for ActionType {
    type Error = TryFromIntError;
    fn try_from(n: u8) -> Result<Self, Self::Error> {
        // cast is safe since it's a repr(u8) unit enum
        Self::iter().find(|t| *t as u8 == n).ok_or(TryFromIntError)
    }
}

impl ActionType {
    pub(crate) fn as_boxed_action_element(
        &self,
        value: bool,
    ) -> np_adv_dynamic::legacy::ToBoxedActionElement {
        use np_adv_dynamic::legacy::ToBoxedActionElement;
        match self {
            Self::CrossDevSdk => ToBoxedActionElement::CrossDevSdk(value),
            Self::CallTransfer => ToBoxedActionElement::CallTransfer(value),
            Self::ActiveUnlock => ToBoxedActionElement::ActiveUnlock(value),
            Self::NearbyShare => ToBoxedActionElement::NearbyShare(value),
            Self::InstantTethering => ToBoxedActionElement::InstantTethering(value),
            Self::PhoneHub => ToBoxedActionElement::PhoneHub(value),
        }
    }
}

impl From<ActionType> for np_adv::legacy::data_elements::actions::ActionType {
    fn from(value: ActionType) -> Self {
        match value {
            ActionType::CrossDevSdk => {
                np_adv::legacy::data_elements::actions::ActionType::CrossDevSdk
            }
            ActionType::CallTransfer => {
                np_adv::legacy::data_elements::actions::ActionType::CallTransfer
            }
            ActionType::ActiveUnlock => {
                np_adv::legacy::data_elements::actions::ActionType::ActiveUnlock
            }
            ActionType::NearbyShare => {
                np_adv::legacy::data_elements::actions::ActionType::NearbyShare
            }
            ActionType::InstantTethering => {
                np_adv::legacy::data_elements::actions::ActionType::InstantTethering
            }
            ActionType::PhoneHub => np_adv::legacy::data_elements::actions::ActionType::PhoneHub,
        }
    }
}

// ensure bidirectional mapping
impl From<np_adv::legacy::data_elements::actions::ActionType> for ActionType {
    fn from(value: np_adv_de::actions::ActionType) -> Self {
        match value {
            np_adv_de::actions::ActionType::CrossDevSdk => ActionType::CrossDevSdk,
            np_adv_de::actions::ActionType::CallTransfer => ActionType::CallTransfer,
            np_adv_de::actions::ActionType::ActiveUnlock => ActionType::ActiveUnlock,
            np_adv_de::actions::ActionType::NearbyShare => ActionType::NearbyShare,
            np_adv_de::actions::ActionType::InstantTethering => ActionType::InstantTethering,
            np_adv_de::actions::ActionType::PhoneHub => ActionType::PhoneHub,
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
                let bits =
                    np_adv::legacy::data_elements::actions::ActionBits::<Plaintext>::try_from(
                        action_bits.bitfield,
                    )
                    .map_err(|_| InvalidStackDataStructure)?;
                Ok(bits.into())
            }
            V0Actions::Encrypted(action_bits) => {
                let bits =
                    np_adv::legacy::data_elements::actions::ActionBits::<Ciphertext>::try_from(
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
    pub fn has_action(&self, action_type: ActionType) -> Result<bool, InvalidStackDataStructure> {
        let boxed_action_bits = np_adv_dynamic::legacy::BoxedActionBits::try_from(*self)?;
        let action_type = action_type.into();
        Ok(boxed_action_bits.has_action(action_type))
    }

    /// Attempts to set the given action bit to the given boolean value.
    /// This operation may fail if the requested action bit may not be
    /// set for the kind of containing advertisement (public/encrypted)
    /// that this action DE is intended to belong to. In this case,
    /// the original action bits will be yielded back to the caller,
    /// unaltered.
    pub fn set_action(
        self,
        action_type: ActionType,
        value: bool,
    ) -> Result<SetV0ActionResult, InvalidStackDataStructure> {
        let mut boxed_action_bits = np_adv_dynamic::legacy::BoxedActionBits::try_from(self)?;
        let boxed_action_element = action_type.as_boxed_action_element(value);
        match boxed_action_bits.set_action(boxed_action_element) {
            Ok(()) => Ok(SetV0ActionResult::Success(boxed_action_bits.into())),
            Err(_) => Ok(SetV0ActionResult::Error(self)),
        }
    }
}
