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

use array_view::ArrayView;
use crypto_provider::CryptoProvider;
use np_adv::legacy::actions::*;
use np_adv::legacy::data_elements::*;
use np_adv::legacy::serialize::*;
use np_adv::legacy::*;
use np_adv::shared_data::*;
use np_adv::PublicIdentity;
use thiserror::Error;

/// Wrapper around a V0 advertisement builder which
/// is agnostic to the kind of identity used in the advertisement.
/// Instead of compile-time errors for non-matching packet flavors,
/// this builder instead defers any generated errors to run-time.
/// Generic over the Aes algorithm used for any encrypted identities,
/// since that is generally specified at compile-time.
pub enum BoxedAdvBuilder<C: CryptoProvider> {
    /// Builder for public advertisements.
    Public(AdvBuilder<PublicIdentity>),
    /// Builder for LDT-encryptedadvertisements.
    Ldt(AdvBuilder<LdtIdentity<C>>),
}

/// Wrapper around possible errors which occur only during
/// advertisement construction from a builder.
#[derive(Debug)]
pub enum BoxedAdvConstructionError {
    /// An error originating from a problem with LDT
    /// encryption of the advertisement contents.
    Ldt(LdtPostprocessError),
}

impl<C: CryptoProvider> BoxedAdvBuilder<C> {
    /// Returns true if this wraps an adv builder which
    /// leverages some encrypted identity.
    pub fn is_encrypted(&self) -> bool {
        match self {
            BoxedAdvBuilder::Public(_) => false,
            BoxedAdvBuilder::Ldt(_) => true,
        }
    }
    /// Constructs a new BoxedAdvBuilder from the given BoxedIdentity
    pub fn new(identity: BoxedIdentity<C>) -> Self {
        match identity {
            BoxedIdentity::Public(identity) => BoxedAdvBuilder::Public(AdvBuilder::new(identity)),
            BoxedIdentity::LdtIdentity(identity) => BoxedAdvBuilder::Ldt(AdvBuilder::new(identity)),
        }
    }
    /// Attempts to add a data element to the advertisement
    /// being built by this BoxedAdvBuilder. Returns
    /// nothing if successful, and a BoxedAddDataElementError
    /// if something went wrong in the attempt to add the DE.
    pub fn add_data_element(
        &mut self,
        data_element: ToBoxedDataElementBundle,
    ) -> Result<(), BoxedAddDataElementError> {
        match self {
            BoxedAdvBuilder::Public(public_builder) => {
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
    pub fn into_advertisement(
        self,
    ) -> Result<ArrayView<u8, BLE_ADV_SVC_CONTENT_LEN>, BoxedAdvConstructionError> {
        match self {
            BoxedAdvBuilder::Public(x) => {
                match x.into_advertisement() {
                    Ok(x) => Ok(x),
                    Err(x) => match x {}, //Infallible
                }
            }
            BoxedAdvBuilder::Ldt(x) => {
                x.into_advertisement().map_err(BoxedAdvConstructionError::Ldt)
            }
        }
    }
}

/// Possible errors generated when trying to add a DE to a
/// BoxedAdvBuilder.
#[derive(Debug, Error)]
pub enum BoxedAddDataElementError {
    /// Some kind of error in adding the data element which
    /// is not an issue with trying to add a DE of the incorrect
    /// packet flavoring.
    #[error("{0:?}")]
    UnderlyingError(AddDataElementError),
    #[error(
        "Expected packet flavoring for added DEs does not match the actual packet flavor of the DE"
    )]
    /// Error when attempting to add a DE which requires one
    /// of an (encrypted/plaintext) advertisement, but the
    /// advertisement builder doesn't match this requirement.
    FlavorMismatchError,
}

impl From<AddDataElementError> for BoxedAddDataElementError {
    fn from(add_data_element_error: AddDataElementError) -> Self {
        BoxedAddDataElementError::UnderlyingError(add_data_element_error)
    }
}

/// Trait object reference to a `ToDataElementBundle<I>` with lifetime `'a`.
/// Implements `ToDataElementBundle<I>` by deferring to the wrapped trait object.
pub struct DynamicToDataElementBundle<'a, I: PacketFlavor> {
    wrapped: &'a dyn ToDataElementBundle<I>,
}

impl<'a, I: PacketFlavor> From<&'a dyn ToDataElementBundle<I>>
    for DynamicToDataElementBundle<'a, I>
{
    fn from(wrapped: &'a dyn ToDataElementBundle<I>) -> Self {
        DynamicToDataElementBundle { wrapped }
    }
}

impl<'a, I: PacketFlavor> ToDataElementBundle<I> for DynamicToDataElementBundle<'a, I> {
    fn to_de_bundle(&self) -> DataElementBundle<I> {
        self.wrapped.to_de_bundle()
    }
}

/// Trait for types which can provide trait object
/// references to either plaintext or ciphertext [ToDataElementBundle]
pub trait ToMultiFlavorElementBundle {
    /// Gets the associated trait object reference to a `ToDataElementBundle<Plaintext>`
    /// with the same lifetime as a reference to the implementor.
    fn to_plaintext(&self) -> DynamicToDataElementBundle<Plaintext>;

    /// Gets the associated trait object reference to a `ToDataElementBundle<Ciphertext>`
    /// with the same lifetime as a reference to the implementor.
    fn to_ciphertext(&self) -> DynamicToDataElementBundle<Ciphertext>;
}

/// Blanket impl of [ToMultiFlavorElementBundle] for implementors of [ToDataElementBundle]
/// for both [Plaintext] and [Ciphertext] packet flavors.
impl<T: ToDataElementBundle<Plaintext> + ToDataElementBundle<Ciphertext>> ToMultiFlavorElementBundle
    for T
{
    fn to_plaintext(&self) -> DynamicToDataElementBundle<Plaintext> {
        let reference: &dyn ToDataElementBundle<Plaintext> = self;
        reference.into()
    }
    fn to_ciphertext(&self) -> DynamicToDataElementBundle<Ciphertext> {
        let reference: &dyn ToDataElementBundle<Ciphertext> = self;
        reference.into()
    }
}

/// Boxed trait object version of [ToDataElementBundle] which incorporates
/// all possible variants on generatable packet flavoring
/// (`Plaintext`, `Ciphertext`, or both, as a [ToMultiFlavorElementBundle])
pub enum ToBoxedDataElementBundle {
    /// The underlying DE is plaintext-only.
    Plaintext(Box<dyn ToDataElementBundle<Plaintext>>),
    /// The underlying DE is ciphertext-only.
    Ciphertext(Box<dyn ToDataElementBundle<Ciphertext>>),
    /// The underlying DE may exist in plaintext or
    /// in ciphertext advertisements.
    Both(Box<dyn ToMultiFlavorElementBundle>),
}

impl ToBoxedDataElementBundle {
    /// If this [ToBoxedDataElementBundle] can generate plaintext, returns
    /// a trait object reference to a `ToDataElementBundle<Plaintext>`
    pub fn to_plaintext(&self) -> Option<DynamicToDataElementBundle<Plaintext>> {
        match &self {
            ToBoxedDataElementBundle::Plaintext(x) => Some(x.as_ref().into()),
            ToBoxedDataElementBundle::Ciphertext(_) => None,
            ToBoxedDataElementBundle::Both(x) => Some(x.as_ref().to_plaintext()),
        }
    }
    /// If this [ToBoxedDataElementBundle] can generate ciphertext, returns
    /// a trait object reference to a `ToDataElementBundle<Ciphertext>`
    pub fn to_ciphertext(&self) -> Option<DynamicToDataElementBundle<Ciphertext>> {
        match &self {
            ToBoxedDataElementBundle::Plaintext(_) => None,
            ToBoxedDataElementBundle::Ciphertext(x) => Some(x.as_ref().into()),
            ToBoxedDataElementBundle::Both(x) => Some(x.as_ref().to_ciphertext()),
        }
    }
}

/// Boxed version of implementors of the Identity trait.
/// A is the underlying Aes algorithm leveraged by ciphertext-based identities.
pub enum BoxedIdentity<C: CryptoProvider> {
    /// Public Identity.
    Public(PublicIdentity),
    /// An encrypted identity, using LDT encryption.
    LdtIdentity(LdtIdentity<C>),
}

impl<C: CryptoProvider> From<PublicIdentity> for BoxedIdentity<C> {
    fn from(public_identity: PublicIdentity) -> BoxedIdentity<C> {
        BoxedIdentity::Public(public_identity)
    }
}

impl<C: CryptoProvider> From<LdtIdentity<C>> for BoxedIdentity<C> {
    fn from(ldt_identity: LdtIdentity<C>) -> BoxedIdentity<C> {
        BoxedIdentity::LdtIdentity(ldt_identity)
    }
}

impl From<TxPower> for ToBoxedDataElementBundle {
    fn from(data: TxPower) -> Self {
        ToBoxedDataElementBundle::Both(Box::new(TxPowerDataElement::from(data)))
    }
}

impl From<BoxedActionBits> for ToBoxedDataElementBundle {
    fn from(action_bits: BoxedActionBits) -> Self {
        match action_bits {
            BoxedActionBits::Plaintext(action_bits) => {
                ToBoxedDataElementBundle::Plaintext(Box::new(ActionsDataElement::from(action_bits)))
            }
            BoxedActionBits::Ciphertext(action_bits) => ToBoxedDataElementBundle::Ciphertext(
                Box::new(ActionsDataElement::from(action_bits)),
            ),
        }
    }
}

/// Boxed version of `ToActionElement` which allows abstracting over
/// what packet flavors are supported by a given action.
pub enum ToBoxedActionElement {
    /// A context-sync sequence number.
    ContextSyncSeqNum(ContextSyncSeqNum),
    /// Action bit for active unlock.
    ActiveUnlock(bool),
    /// Action bit for nearby share.
    NearbyShare(bool),
    /// Action bit for instant tethering.
    InstantTethering(bool),
    /// Action bit for PhoneHub.
    PhoneHub(bool),
    /// Action bit for Finder.
    Finder(bool),
    /// Action bit for Fast Pair/SASS
    FastPairSass(bool),
    /// Action bit for Presence Manager.
    PresenceManager(bool),
}

/// [`ActionBits`] with runtime-determined packet flavoring
pub enum BoxedActionBits {
    /// Action-bits for a plaintext advertisement.
    Plaintext(ActionBits<Plaintext>),
    /// Action-bits for a ciphertext advertisement.
    Ciphertext(ActionBits<Ciphertext>),
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

    fn set<F: PacketFlavor, E: ToActionElement<F>>(
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
                ToBoxedActionElement::ContextSyncSeqNum(x) => Self::set(action_bits, x),
                ToBoxedActionElement::NearbyShare(b) => {
                    Self::set(action_bits, NearbyShare::from(b))
                }
                ToBoxedActionElement::Finder(b) => Self::set(action_bits, Finder::from(b)),
                ToBoxedActionElement::FastPairSass(b) => {
                    Self::set(action_bits, FastPairSass::from(b))
                }
                _ => Err(BoxedSetActionFlavorError),
            },
            BoxedActionBits::Ciphertext(action_bits) => match to_element {
                ToBoxedActionElement::ContextSyncSeqNum(x) => Self::set(action_bits, x),
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
                ToBoxedActionElement::PresenceManager(b) => {
                    Self::set(action_bits, PresenceManager::from(b))
                }
                _ => Err(BoxedSetActionFlavorError),
            },
        }
    }
}
