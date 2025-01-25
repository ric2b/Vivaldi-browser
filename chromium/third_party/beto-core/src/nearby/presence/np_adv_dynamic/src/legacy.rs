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

use crypto_provider::CryptoProvider;
use np_adv::{
    legacy::{
        data_elements::{actions::*, tx_power::TxPowerDataElement, *},
        serialize::*,
        *,
    },
    shared_data::*,
};
use std::fmt::{Display, Formatter};

/// Wrapper around a V0 advertisement builder which
/// is agnostic to the kind of identity used in the advertisement.
/// Instead of compile-time errors for non-matching packet flavors,
/// this builder instead defers any generated errors to run-time.
/// Generic over the Aes algorithm used for any encrypted identities,
/// since that is generally specified at compile-time.
pub enum BoxedAdvBuilder<C: CryptoProvider> {
    /// Builder for unencrypted advertisements.
    Unencrypted(AdvBuilder<UnencryptedEncoder>),
    /// Builder for LDT-encrypted advertisements.
    Ldt(AdvBuilder<LdtEncoder<C>>),
}

/// Wrapper around possible errors which occur only during
/// advertisement construction from a builder.
#[derive(Debug)]
pub enum BoxedAdvConstructionError {
    /// An error originating from a problem with LDT
    /// encryption of the advertisement contents.
    Ldt(LdtEncodeError),
    /// An error from encoding an unencrypted adv
    Unencrypted(UnencryptedEncodeError),
}

impl<C: CryptoProvider> BoxedAdvBuilder<C> {
    /// Returns true if this wraps an adv builder which
    /// leverages some encrypted identity.
    pub fn is_encrypted(&self) -> bool {
        match self {
            BoxedAdvBuilder::Unencrypted(_) => false,
            BoxedAdvBuilder::Ldt(_) => true,
        }
    }
    /// Constructs a new BoxedAdvBuilder from the given BoxedIdentity
    pub fn new(identity: BoxedEncoder<C>) -> Self {
        match identity {
            BoxedEncoder::Unencrypted(encoder) => {
                BoxedAdvBuilder::Unencrypted(AdvBuilder::new(encoder))
            }
            BoxedEncoder::LdtEncrypted(encoder) => BoxedAdvBuilder::Ldt(AdvBuilder::new(encoder)),
        }
    }
    /// Attempts to add a data element to the advertisement
    /// being built by this BoxedAdvBuilder. Returns
    /// nothing if successful, and a BoxedAddDataElementError
    /// if something went wrong in the attempt to add the DE.
    pub fn add_data_element(
        &mut self,
        data_element: ToBoxedSerializeDataElement,
    ) -> Result<(), BoxedAddDataElementError> {
        match self {
            BoxedAdvBuilder::Unencrypted(public_builder) => {
                //Verify that we can get the data element as plaintext
                let maybe_plaintext_data_element = data_element.to_plaintext();
                match maybe_plaintext_data_element {
                    Some(plaintext_data_element) => public_builder
                        .add_data_element(plaintext_data_element)
                        .map_err(|e| e.into()),
                    None => Err(BoxedAddDataElementError::FlavorMismatchError),
                }
            }
            BoxedAdvBuilder::Ldt(private_builder) => {
                //Verify that we can get the data element as ciphertext
                let maybe_ciphertext_data_element = data_element.to_ciphertext();
                match maybe_ciphertext_data_element {
                    Some(ciphertext_data_element) => private_builder
                        .add_data_element(ciphertext_data_element)
                        .map_err(|e| e.into()),
                    None => Err(BoxedAddDataElementError::FlavorMismatchError),
                }
            }
        }
    }
    /// Consume this BoxedAdvBuilder and attempt to create
    /// a serialized advertisement including the added DEs.
    pub fn into_advertisement(self) -> Result<SerializedAdv, BoxedAdvConstructionError> {
        match self {
            BoxedAdvBuilder::Unencrypted(x) => {
                x.into_advertisement().map_err(BoxedAdvConstructionError::Unencrypted)
            }
            BoxedAdvBuilder::Ldt(x) => {
                x.into_advertisement().map_err(BoxedAdvConstructionError::Ldt)
            }
        }
    }
}

/// Possible errors generated when trying to add a DE to a
/// BoxedAdvBuilder.
#[derive(Debug)]
pub enum BoxedAddDataElementError {
    /// Some kind of error in adding the data element which
    /// is not an issue with trying to add a DE of the incorrect
    /// packet flavoring.
    UnderlyingError(AddDataElementError),
    /// Error when attempting to add a DE which requires one
    /// of an (encrypted/plaintext) advertisement, but the
    /// advertisement builder doesn't match this requirement.
    FlavorMismatchError,
}

impl Display for BoxedAddDataElementError {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            BoxedAddDataElementError::UnderlyingError(u) => {
                write!(f, "{0:?}", u)
            }
            BoxedAddDataElementError::FlavorMismatchError => {
                write!(f, "Expected packet flavoring for added DEs does not match the actual packet flavor of the DE")
            }
        }
    }
}

impl std::error::Error for BoxedAddDataElementError {}

impl From<AddDataElementError> for BoxedAddDataElementError {
    fn from(add_data_element_error: AddDataElementError) -> Self {
        BoxedAddDataElementError::UnderlyingError(add_data_element_error)
    }
}

/// Trait for types which can provide trait object
/// references to either plaintext or ciphertext [SerializeDataElement]
pub trait ToMultiFlavorSerializeDataElement {
    /// Gets the associated trait object reference to a `SerializeDataElement<Plaintext>`
    /// with the same lifetime as a reference to the implementor.
    fn to_plaintext(&self) -> DynamicSerializeDataElement<Plaintext>;

    /// Gets the associated trait object reference to a `SerializeDataElement<Ciphertext>`
    /// with the same lifetime as a reference to the implementor.
    fn to_ciphertext(&self) -> DynamicSerializeDataElement<Ciphertext>;
}

/// Blanket impl of [ToMultiFlavorSerializeDataElement] for implementors of [SerializeDataElement]
/// for both [Plaintext] and [Ciphertext] packet flavors.
impl<T: SerializeDataElement<Plaintext> + SerializeDataElement<Ciphertext>>
    ToMultiFlavorSerializeDataElement for T
{
    fn to_plaintext(&self) -> DynamicSerializeDataElement<Plaintext> {
        let reference: &dyn SerializeDataElement<Plaintext> = self;
        reference.into()
    }
    fn to_ciphertext(&self) -> DynamicSerializeDataElement<Ciphertext> {
        let reference: &dyn SerializeDataElement<Ciphertext> = self;
        reference.into()
    }
}

/// Boxed trait object version of [SerializeDataElement] which incorporates
/// all possible variants on generatable packet flavoring
/// (`Plaintext`, `Ciphertext`, or both, as a [ToMultiFlavorSerializeDataElement])
pub enum ToBoxedSerializeDataElement {
    /// The underlying DE is plaintext-only.
    Plaintext(Box<dyn SerializeDataElement<Plaintext>>),
    /// The underlying DE is ciphertext-only.
    Ciphertext(Box<dyn SerializeDataElement<Ciphertext>>),
    /// The underlying DE may exist in plaintext or
    /// in ciphertext advertisements.
    Both(Box<dyn ToMultiFlavorSerializeDataElement>),
}

impl ToBoxedSerializeDataElement {
    /// If this [ToBoxedSerializeDataElement] can generate plaintext, returns
    /// a trait object reference to a `SerializeDataElement<Plaintext>`
    pub fn to_plaintext(&self) -> Option<DynamicSerializeDataElement<Plaintext>> {
        match &self {
            ToBoxedSerializeDataElement::Plaintext(x) => Some(x.as_ref().into()),
            ToBoxedSerializeDataElement::Ciphertext(_) => None,
            ToBoxedSerializeDataElement::Both(x) => Some(x.as_ref().to_plaintext()),
        }
    }
    /// If this [ToBoxedSerializeDataElement] can generate ciphertext, returns
    /// a trait object reference to a `SerializeDataElement<Ciphertext>`
    pub fn to_ciphertext(&self) -> Option<DynamicSerializeDataElement<Ciphertext>> {
        match &self {
            ToBoxedSerializeDataElement::Plaintext(_) => None,
            ToBoxedSerializeDataElement::Ciphertext(x) => Some(x.as_ref().into()),
            ToBoxedSerializeDataElement::Both(x) => Some(x.as_ref().to_ciphertext()),
        }
    }
}

/// Boxed version of implementors of the [AdvEncoder] trait.
pub enum BoxedEncoder<C: CryptoProvider> {
    /// Unencrypted encoding.
    Unencrypted(UnencryptedEncoder),
    /// An encrypted encoding, using LDT encryption.
    LdtEncrypted(LdtEncoder<C>),
}

impl<C: CryptoProvider> From<UnencryptedEncoder> for BoxedEncoder<C> {
    fn from(encoder: UnencryptedEncoder) -> BoxedEncoder<C> {
        BoxedEncoder::Unencrypted(encoder)
    }
}

impl<C: CryptoProvider> From<LdtEncoder<C>> for BoxedEncoder<C> {
    fn from(encoder: LdtEncoder<C>) -> BoxedEncoder<C> {
        BoxedEncoder::LdtEncrypted(encoder)
    }
}

impl From<TxPower> for ToBoxedSerializeDataElement {
    fn from(data: TxPower) -> Self {
        ToBoxedSerializeDataElement::Both(Box::new(TxPowerDataElement::from(data)))
    }
}

impl From<BoxedActionBits> for ToBoxedSerializeDataElement {
    fn from(action_bits: BoxedActionBits) -> Self {
        match action_bits {
            BoxedActionBits::Plaintext(action_bits) => ToBoxedSerializeDataElement::Plaintext(
                Box::new(ActionsDataElement::from(action_bits)),
            ),
            BoxedActionBits::Ciphertext(action_bits) => ToBoxedSerializeDataElement::Ciphertext(
                Box::new(ActionsDataElement::from(action_bits)),
            ),
        }
    }
}

/// Boxed version of `ToActionElement` which allows abstracting over
/// what packet flavors are supported by a given action.
pub enum ToBoxedActionElement {
    /// Action bit for cross device SDK.
    CrossDevSdk(bool),
    /// Action bit for call transfer.
    CallTransfer(bool),
    /// Action bit for active unlock.
    ActiveUnlock(bool),
    /// Action bit for nearby share.
    NearbyShare(bool),
    /// Action bit for instant tethering.
    InstantTethering(bool),
    /// Action bit for PhoneHub.
    PhoneHub(bool),
}

/// [`ActionBits`] with runtime-determined packet flavoring
pub enum BoxedActionBits {
    /// Action-bits for a plaintext advertisement.
    Plaintext(ActionBits<Plaintext>),
    /// Action-bits for a ciphertext advertisement.
    Ciphertext(ActionBits<Ciphertext>),
}

impl From<ActionBits<Plaintext>> for BoxedActionBits {
    fn from(bits: ActionBits<Plaintext>) -> Self {
        Self::Plaintext(bits)
    }
}

impl From<ActionBits<Ciphertext>> for BoxedActionBits {
    fn from(bits: ActionBits<Ciphertext>) -> Self {
        Self::Ciphertext(bits)
    }
}

/// Error which is raised when the flavor of a [`BoxedActionBits`]
/// does not match the supported flavors of a [`ToBoxedActionElement`]
/// upon attempting to add the action to the bit-field.
#[derive(Debug)]
pub struct BoxedSetActionFlavorError;

impl BoxedActionBits {
    /// Constructs the [`BoxedActionBits`] variant with the specified packet
    /// flavor variant, and no bits set.
    pub fn new(packet_flavor: PacketFlavorEnum) -> Self {
        match packet_flavor {
            PacketFlavorEnum::Plaintext => BoxedActionBits::Plaintext(ActionBits::default()),
            PacketFlavorEnum::Ciphertext => BoxedActionBits::Ciphertext(ActionBits::default()),
        }
    }

    /// Returns whether a boolean action type is set in these action bits, or `None`
    /// if the given action type does not represent a boolean (e.g: a context-sync
    /// sequence number).
    pub fn has_action(&self, action_type: ActionType) -> bool {
        match self {
            BoxedActionBits::Plaintext(x) => x.has_action(action_type),
            BoxedActionBits::Ciphertext(x) => x.has_action(action_type),
        }
    }

    fn set<F: PacketFlavor, E: ActionElementFlavor<F>>(
        action_bits: &mut ActionBits<F>,
        to_element: E,
    ) -> Result<(), BoxedSetActionFlavorError> {
        action_bits.set_action(to_element);
        Ok(())
    }

    /// Attempts to set the specified [`ToBoxedActionElement`], yielding
    /// a [`BoxedSetActionFlavorError`] if the flavor of this
    /// [`BoxedActionBits`] isn't supported by the passed [`ToBoxedActionElement`].
    pub fn set_action(
        &mut self,
        to_element: ToBoxedActionElement,
    ) -> Result<(), BoxedSetActionFlavorError> {
        match self {
            BoxedActionBits::Plaintext(action_bits) => match to_element {
                ToBoxedActionElement::CrossDevSdk(b) => {
                    Self::set(action_bits, CrossDevSdk::from(b))
                }
                ToBoxedActionElement::NearbyShare(b) => {
                    Self::set(action_bits, NearbyShare::from(b))
                }
                ToBoxedActionElement::CallTransfer(_)
                | ToBoxedActionElement::ActiveUnlock(_)
                | ToBoxedActionElement::InstantTethering(_)
                | ToBoxedActionElement::PhoneHub(_) => Err(BoxedSetActionFlavorError),
            },
            BoxedActionBits::Ciphertext(action_bits) => match to_element {
                ToBoxedActionElement::CrossDevSdk(b) => {
                    Self::set(action_bits, CrossDevSdk::from(b))
                }
                ToBoxedActionElement::CallTransfer(b) => {
                    Self::set(action_bits, CallTransfer::from(b))
                }
                ToBoxedActionElement::ActiveUnlock(b) => {
                    Self::set(action_bits, ActiveUnlock::from(b))
                }
                ToBoxedActionElement::NearbyShare(b) => {
                    Self::set(action_bits, NearbyShare::from(b))
                }
                ToBoxedActionElement::InstantTethering(b) => {
                    Self::set(action_bits, InstantTethering::from(b))
                }
                ToBoxedActionElement::PhoneHub(b) => Self::set(action_bits, PhoneHub::from(b)),
            },
        }
    }
}
