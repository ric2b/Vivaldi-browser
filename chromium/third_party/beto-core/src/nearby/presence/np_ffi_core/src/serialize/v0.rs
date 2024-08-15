// Copyright 2023 Google LLC
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

//! NP Rust FFI structures and methods for V0 advertisement serialization.

use crate::common::*;
use crate::credentials::V0BroadcastCredential;
use crate::serialize::AdvertisementBuilderKind;
use crate::utils::FfiEnum;
use crate::v0::V0DataElement;
use crypto_provider_default::CryptoProviderImpl;
use handle_map::{declare_handle_map, HandleLike, HandleMapDimensions, HandleMapFullError};
use np_adv;
use np_adv_dynamic;

/// A handle to a builder for V0 advertisements.
#[derive(Clone, Copy)]
#[repr(C)]
pub struct V0AdvertisementBuilder {
    kind: AdvertisementBuilderKind,
    handle: V0AdvertisementBuilderHandle,
}

impl V0AdvertisementBuilder {
    /// Attempts to add the given data element to the V0
    /// advertisement builder behind this handle.
    pub fn add_de(&self, de: V0DataElement) -> Result<AddV0DEResult, InvalidStackDataStructure> {
        match self.handle.get_mut() {
            Ok(mut adv_builder_write_guard) => adv_builder_write_guard.add_de(de),
            Err(_) => Ok(AddV0DEResult::InvalidAdvertisementBuilderHandle),
        }
    }
    /// Attempts to serialize the contents of the advertisement builder
    /// behind this handle to bytes. Assuming that the handle is valid,
    /// this operation will always result in the contents behind the
    /// advertisement builder handle being deallocated.
    pub fn into_advertisement(self) -> SerializeV0AdvertisementResult {
        match self.handle.deallocate() {
            Ok(adv_builder) => adv_builder.into_advertisement(),
            Err(_) => SerializeV0AdvertisementResult::InvalidAdvertisementBuilderHandle,
        }
    }
    /// Attempts to deallocate the V0 advertisement builder
    /// behind this handle.
    pub fn deallocate(self) -> DeallocateResult {
        self.handle.deallocate().map(|_| ()).into()
    }
}

/// Discriminant for `CreateV0AdvertisementBuilderResult`
#[derive(Copy, Clone)]
#[repr(u8)]
pub enum CreateV0AdvertisementBuilderResultKind {
    /// The attempt to create a new advertisement builder
    /// failed since there are no more available
    /// slots for V0 advertisement builders in their handle-map.
    NoSpaceLeft = 0,
    /// The attempt succeeded. The wrapped advertisement builder
    /// may be obtained via
    /// `CreateV0AdvertisementBuilderResult#into_success`.
    Success = 1,
}

/// The result of attempting to create a new V0 advertisement builder.
#[repr(C)]
#[allow(missing_docs)]
pub enum CreateV0AdvertisementBuilderResult {
    NoSpaceLeft,
    Success(V0AdvertisementBuilder),
}

impl From<Result<V0AdvertisementBuilder, HandleMapFullError>>
    for CreateV0AdvertisementBuilderResult
{
    fn from(result: Result<V0AdvertisementBuilder, HandleMapFullError>) -> Self {
        match result {
            Ok(builder) => CreateV0AdvertisementBuilderResult::Success(builder),
            Err(_) => CreateV0AdvertisementBuilderResult::NoSpaceLeft,
        }
    }
}

impl FfiEnum for CreateV0AdvertisementBuilderResult {
    type Kind = CreateV0AdvertisementBuilderResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            CreateV0AdvertisementBuilderResult::NoSpaceLeft => {
                CreateV0AdvertisementBuilderResultKind::NoSpaceLeft
            }
            CreateV0AdvertisementBuilderResult::Success(_) => {
                CreateV0AdvertisementBuilderResultKind::Success
            }
        }
    }
}

impl CreateV0AdvertisementBuilderResult {
    declare_enum_cast! {into_success, Success, V0AdvertisementBuilder }
}

/// Creates a new V0 advertisement builder for a public advertisement.
pub fn create_v0_public_advertisement_builder() -> CreateV0AdvertisementBuilderResult {
    V0AdvertisementBuilderHandle::allocate(V0AdvertisementBuilderInternals::new_public)
        .map(|handle| V0AdvertisementBuilder { kind: AdvertisementBuilderKind::Public, handle })
        .into()
}

/// Creates a new V0 advertisement builder for an encrypted advertisement.
pub fn create_v0_encrypted_advertisement_builder(
    broadcast_cred: V0BroadcastCredential,
    identity_type: EncryptedIdentityType,
    salt: FixedSizeArray<2>,
) -> CreateV0AdvertisementBuilderResult {
    V0AdvertisementBuilderHandle::allocate(move || {
        V0AdvertisementBuilderInternals::new_ldt(broadcast_cred, identity_type, salt.into_array())
    })
    .map(|handle| V0AdvertisementBuilder { kind: AdvertisementBuilderKind::Encrypted, handle })
    .into()
}

impl V0AdvertisementBuilder {
    /// Gets the kind of advertisement builder (public/encrypted)
    pub fn kind(&self) -> AdvertisementBuilderKind {
        self.kind
    }
}

/// Discriminant for `SerializeV0AdvertisementResult`.
#[repr(u8)]
pub enum SerializeV0AdvertisementResultKind {
    /// Serializing the advertisement to bytes was successful.
    Success = 0,
    /// Serializing the advertisement to bytes failed
    /// because the data in the advertisement wasn't
    /// of an appropriate size for LDT encryption
    /// to succeed.
    LdtError = 1,
    /// The advertisement builder handle was invalid.
    InvalidAdvertisementBuilderHandle = 2,
}

/// The result of attempting to serialize the contents
/// of a V0 advertisement builder to raw bytes.
#[repr(C)]
#[allow(missing_docs)]
pub enum SerializeV0AdvertisementResult {
    Success(ByteBuffer<24>),
    LdtError,
    InvalidAdvertisementBuilderHandle,
}

impl SerializeV0AdvertisementResult {
    declare_enum_cast! { into_success, Success, ByteBuffer<24> }
}

impl FfiEnum for SerializeV0AdvertisementResult {
    type Kind = SerializeV0AdvertisementResultKind;
    fn kind(&self) -> SerializeV0AdvertisementResultKind {
        match self {
            Self::Success(_) => SerializeV0AdvertisementResultKind::Success,
            Self::LdtError => SerializeV0AdvertisementResultKind::LdtError,
            Self::InvalidAdvertisementBuilderHandle => {
                SerializeV0AdvertisementResultKind::InvalidAdvertisementBuilderHandle
            }
        }
    }
}

/// Internal Rust-side implementation of a V0 advertisement builder.
pub struct V0AdvertisementBuilderInternals {
    adv_builder: np_adv_dynamic::legacy::BoxedAdvBuilder<CryptoProviderImpl>,
}

impl V0AdvertisementBuilderInternals {
    pub(crate) fn new_public() -> Self {
        Self::new(np_adv::PublicIdentity.into())
    }
    pub(crate) fn new_ldt(
        broadcast_cred: V0BroadcastCredential,
        identity_type: EncryptedIdentityType,
        salt: [u8; 2],
    ) -> Self {
        // TODO: What do about salts? Need to prevent re-use fo the same salt,
        // but have no current rich representation of used salts...
        let salt = ldt_np_adv::LegacySalt::from(salt);
        let identity_type = identity_type.into();
        let internal_broadcast_cred = broadcast_cred.into_internal();
        let identity = np_adv::legacy::serialize::LdtIdentity::new(
            identity_type,
            salt,
            &internal_broadcast_cred,
        );
        Self::new(identity.into())
    }
    fn new(identity: np_adv_dynamic::legacy::BoxedIdentity<CryptoProviderImpl>) -> Self {
        let adv_builder =
            np_adv_dynamic::legacy::BoxedAdvBuilder::<CryptoProviderImpl>::new(identity);
        Self { adv_builder }
    }
    fn add_de(&mut self, de: V0DataElement) -> Result<AddV0DEResult, InvalidStackDataStructure> {
        let to_boxed = np_adv_dynamic::legacy::ToBoxedDataElementBundle::try_from(de)?;
        use np_adv::legacy::serialize::AddDataElementError;
        use np_adv_dynamic::legacy::BoxedAddDataElementError;
        match self.adv_builder.add_data_element(to_boxed) {
            Ok(_) => Ok(AddV0DEResult::Success),
            Err(BoxedAddDataElementError::FlavorMismatchError) => {
                Ok(AddV0DEResult::InvalidIdentityTypeForDataElement)
            }
            Err(BoxedAddDataElementError::UnderlyingError(
                AddDataElementError::InsufficientAdvSpace,
            )) => Ok(AddV0DEResult::InsufficientAdvertisementSpace),
        }
    }
    fn into_advertisement(self) -> SerializeV0AdvertisementResult {
        match self.adv_builder.into_advertisement() {
            Ok(serialized_bytes) => {
                let byte_buffer = ByteBuffer::from_array_view(serialized_bytes);
                SerializeV0AdvertisementResult::Success(byte_buffer)
            }
            Err(np_adv_dynamic::legacy::BoxedAdvConstructionError::Ldt(_)) => {
                SerializeV0AdvertisementResult::LdtError
            }
        }
    }
}

fn get_v0_advertisement_builder_handle_map_dimensions() -> HandleMapDimensions {
    HandleMapDimensions {
        num_shards: global_num_shards(),
        max_active_handles: global_max_num_v0_advertisement_builders(),
    }
}

/// A `#[repr(C)]` handle to a value of type `V0AdvertisementBuilderInternals`
#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq)]
struct V0AdvertisementBuilderHandle {
    handle_id: u64,
}

declare_handle_map!(
    advertisement_builder,
    super::get_v0_advertisement_builder_handle_map_dimensions(),
    super::V0AdvertisementBuilderHandle,
    super::V0AdvertisementBuilderInternals
);

/// Result code for the operation of adding a DE to a V0
/// advertisement builder.
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum AddV0DEResult {
    /// The DE was successfully added to the advertisement builder
    /// behind the given handle.
    Success = 0,
    /// The handle for the advertisement builder was invalid.
    InvalidAdvertisementBuilderHandle = 1,
    /// There was not enough available space left in the advertisement
    /// to append the given data element.
    InsufficientAdvertisementSpace = 2,
    /// The passed data element is not broadcastable under the
    /// identity type of the advertisement (public/private).
    InvalidIdentityTypeForDataElement = 3,
}

impl From<np_adv_dynamic::legacy::BoxedAddDataElementError> for AddV0DEResult {
    fn from(err: np_adv_dynamic::legacy::BoxedAddDataElementError) -> Self {
        use np_adv_dynamic::legacy::BoxedAddDataElementError;
        match err {
            BoxedAddDataElementError::UnderlyingError(_) => Self::InsufficientAdvertisementSpace,
            BoxedAddDataElementError::FlavorMismatchError => {
                Self::InvalidIdentityTypeForDataElement
            }
        }
    }
}
