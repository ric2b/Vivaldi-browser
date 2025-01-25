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

//! NP Rust FFI structures and methods for V1 advertisement serialization.

use crate::common::*;
use crate::credentials::V1BroadcastCredential;
use crate::serialize::AdvertisementBuilderKind;
use crate::utils::FfiEnum;
use crate::v1::V1VerificationMode;
use crypto_provider_default::CryptoProviderImpl;
use handle_map::{declare_handle_map, HandleLike, HandleMapFullError};
use np_adv::extended::serialize::AdvertisementType;

#[cfg(feature = "testing")]
use np_adv::extended::salt::MultiSalt;

/// A handle to a builder for V1 advertisements.
#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct V1AdvertisementBuilder {
    handle_id: u64,
}

declare_handle_map!(
    advertisement_builder,
    crate::common::default_handle_map_dimensions(),
    super::V1AdvertisementBuilder,
    super::V1AdvertisementBuilderInternals
);

impl V1AdvertisementBuilder {
    /// Attempts to create a builder for a new public section within this advertisement, returning
    /// an owned handle to the newly-created section builder if successful.
    ///
    /// This method may fail if there is another currently-active section builder for the same
    /// advertisement builder, if the kind of section being added does not match the advertisement
    /// type (public/encrypted), or if the section would not manage to fit within the enclosing
    /// advertisement.
    pub fn public_section_builder(&self) -> CreateV1SectionBuilderResult {
        self.section_builder_internals(|internals| internals.public_section_builder())
    }

    /// Attempts to create a builder for a new encrypted section within this advertisement,
    /// returning an owned handle to the newly-created section builder if successful.
    ///
    /// The identity details for the new section builder may be specified
    /// via providing the broadcast credential data, the kind of encrypted
    /// identity being broadcast (private/trusted/provisioned), and the
    /// verification mode (MIC/Signature) to be used for the encrypted section.
    ///
    /// This method may fail if there is another currently-active
    /// section builder for the same advertisement builder, if the
    /// kind of section being added does not match the advertisement
    /// type (public/encrypted), or if the section would not manage
    /// to fit within the enclosing advertisement.
    pub fn encrypted_section_builder(
        &self,
        broadcast_cred: V1BroadcastCredential,
        verification_mode: V1VerificationMode,
    ) -> CreateV1SectionBuilderResult {
        self.section_builder_internals(move |internals| {
            internals.encrypted_section_builder(broadcast_cred, verification_mode)
        })
    }

    /// Attempts to create a builder for a new encrypted section within this advertisement,
    /// returning an owned handle to the newly-created section builder if successful.
    ///
    /// The identity details for the new section builder may be specified
    /// via providing the broadcast credential data, the kind of encrypted
    /// identity being broadcast (private/trusted/provisioned).
    ///
    ///
    /// The section will be encrypted with the MIC verification mode using the given salt.
    ///
    /// This method may fail if there is another currently-active
    /// section builder for the same advertisement builder, if the
    /// kind of section being added does not match the advertisement
    /// type (public/encrypted), or if the section would not manage
    /// to fit within the enclosing advertisement.
    #[cfg(feature = "testing")]
    pub fn mic_custom_salt_section_builder(
        &self,
        broadcast_cred: V1BroadcastCredential,
        salt: MultiSalt,
    ) -> CreateV1SectionBuilderResult {
        self.section_builder_internals(move |internals| {
            internals.mic_custom_salt_section_builder(broadcast_cred, salt)
        })
    }

    /// Attempts to serialize the contents of the advertisement builder behind this handle to
    /// bytes. Assuming that the handle is valid, this operation will always take ownership of the
    /// handle and result in the contents behind the advertisement builder handle being
    /// deallocated.
    pub fn into_advertisement(self) -> SerializeV1AdvertisementResult {
        match self.deallocate() {
            Ok(adv_builder) => adv_builder.into_advertisement(),
            Err(_) => SerializeV1AdvertisementResult::InvalidAdvertisementBuilderHandle,
        }
    }

    fn section_builder_internals(
        &self,
        builder_supplier: impl FnOnce(
            &mut V1AdvertisementBuilderInternals,
        ) -> Result<usize, SectionBuilderError>,
    ) -> CreateV1SectionBuilderResult {
        match self.get_mut() {
            Ok(mut adv_builder_write_guard) => {
                match builder_supplier(&mut adv_builder_write_guard) {
                    Ok(section_index) => CreateV1SectionBuilderResult::Success(V1SectionBuilder {
                        adv_builder: *self,
                        section_index: section_index as u8,
                    }),
                    Err(e) => e.into(),
                }
            }
            Err(_) => CreateV1SectionBuilderResult::InvalidAdvertisementBuilderHandle,
        }
    }
}

/// Discriminant for `CreateV1AdvertisementBuilderResult`
#[derive(Copy, Clone)]
#[repr(u8)]
pub enum CreateV1AdvertisementBuilderResultKind {
    /// The attempt to create a new advertisement builder
    /// failed since there are no more available
    /// slots for V1 advertisement builders in their handle-map.
    NoSpaceLeft = 0,
    /// The attempt succeeded. The wrapped advertisement builder
    /// may be obtained via
    /// `CreateV1AdvertisementBuilderResult#into_success`.
    Success = 1,
}

/// The result of attempting to create a new V1 advertisement builder.
#[repr(C)]
#[allow(missing_docs)]
pub enum CreateV1AdvertisementBuilderResult {
    NoSpaceLeft,
    Success(V1AdvertisementBuilder),
}

impl From<Result<V1AdvertisementBuilder, HandleMapFullError>>
    for CreateV1AdvertisementBuilderResult
{
    fn from(result: Result<V1AdvertisementBuilder, HandleMapFullError>) -> Self {
        match result {
            Ok(builder) => CreateV1AdvertisementBuilderResult::Success(builder),
            Err(_) => CreateV1AdvertisementBuilderResult::NoSpaceLeft,
        }
    }
}

impl FfiEnum for CreateV1AdvertisementBuilderResult {
    type Kind = CreateV1AdvertisementBuilderResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            CreateV1AdvertisementBuilderResult::NoSpaceLeft => {
                CreateV1AdvertisementBuilderResultKind::NoSpaceLeft
            }
            CreateV1AdvertisementBuilderResult::Success(_) => {
                CreateV1AdvertisementBuilderResultKind::Success
            }
        }
    }
}

impl CreateV1AdvertisementBuilderResult {
    declare_enum_cast! {into_success, Success, V1AdvertisementBuilder }
}

/// Creates a new V1 advertisement builder for the given advertisement
/// kind (public/encrypted).
pub fn create_v1_advertisement_builder(
    kind: AdvertisementBuilderKind,
) -> CreateV1AdvertisementBuilderResult {
    V1AdvertisementBuilder::allocate(move || {
        V1AdvertisementBuilderInternals::new(kind.as_internal_v1())
    })
    .into()
}

pub(crate) enum V1AdvertisementBuilderState {
    /// Internal state for when we have an active advertisement
    /// builder, but no currently-active section builder.
    Advertisement(np_adv_dynamic::extended::BoxedAdvBuilder),
    /// Internal state for when we have both an active advertisement
    /// builder and an active section builder.
    Section(np_adv_dynamic::extended::BoxedSectionBuilder<np_adv::extended::serialize::AdvBuilder>),
}

/// Internal version of errors which may be raised when
/// attempting to derive a new section builder from an
/// advertisement builder.
#[derive(Debug, Eq, PartialEq)]
pub(crate) enum SectionBuilderError {
    /// We're currently in the middle of building a section.
    UnclosedActiveSection,
    /// We're attempting to build a section with an identity
    /// kind (public/encrypted) which doesn't match the kind
    /// for the entire advertisement.
    IdentityKindMismatch,
    /// There isn't enough space for a new section, either
    /// because the maximum section count has been exceeded
    /// or because the advertisement is almost full, and
    /// the minimum size of a section wouldn't fit.
    NoSpaceLeft,
}

impl From<np_adv_dynamic::extended::BoxedAddSectionError> for SectionBuilderError {
    fn from(err: np_adv_dynamic::extended::BoxedAddSectionError) -> Self {
        use np_adv::extended::serialize::AddSectionError;
        use np_adv_dynamic::extended::BoxedAddSectionError;
        match err {
            BoxedAddSectionError::IdentityRequiresSaltError
            | BoxedAddSectionError::Underlying(AddSectionError::IncompatibleSectionType) => {
                SectionBuilderError::IdentityKindMismatch
            }
            BoxedAddSectionError::Underlying(AddSectionError::InsufficientAdvSpace)
            | BoxedAddSectionError::Underlying(AddSectionError::MaxSectionCountExceeded) => {
                SectionBuilderError::NoSpaceLeft
            }
        }
    }
}

/// Discriminant for `SerializeV1AdvertisementResult`.
#[repr(u8)]
pub enum SerializeV1AdvertisementResultKind {
    /// Serializing the advertisement to bytes was successful.
    Success = 0,
    /// The state of the advertisement builder was invalid
    /// for the builder to be closed for serialization, likely
    /// because there was an unclosed section builder.
    InvalidBuilderState = 1,
    /// The advertisement builder handle was invalid.
    InvalidAdvertisementBuilderHandle = 2,
}

/// The result of attempting to serialize the contents
/// of a V1 advertisement builder to raw bytes.
#[repr(C)]
#[allow(missing_docs, clippy::large_enum_variant)]
pub enum SerializeV1AdvertisementResult {
    Success(ByteBuffer<250>),
    InvalidBuilderState,
    InvalidAdvertisementBuilderHandle,
}

impl SerializeV1AdvertisementResult {
    declare_enum_cast! { into_success, Success, ByteBuffer<250> }
}

impl FfiEnum for SerializeV1AdvertisementResult {
    type Kind = SerializeV1AdvertisementResultKind;
    fn kind(&self) -> SerializeV1AdvertisementResultKind {
        match self {
            Self::Success(_) => SerializeV1AdvertisementResultKind::Success,
            Self::InvalidBuilderState => SerializeV1AdvertisementResultKind::InvalidBuilderState,
            Self::InvalidAdvertisementBuilderHandle => {
                SerializeV1AdvertisementResultKind::InvalidAdvertisementBuilderHandle
            }
        }
    }
}

/// Internal, Rust-side implementation of a V1 advertisement builder.
pub struct V1AdvertisementBuilderInternals {
    // Note: This is actually always populated from an external
    // perspective in the absence of panics. We only need
    // the `Option` in order to be able to take ownership
    // and perform a state transition when needed.
    state: Option<V1AdvertisementBuilderState>,
}

impl V1AdvertisementBuilderInternals {
    pub(crate) fn new(adv_type: AdvertisementType) -> Self {
        let builder = np_adv::extended::serialize::AdvBuilder::new(adv_type);
        let builder = builder.into();
        let state = Some(V1AdvertisementBuilderState::Advertisement(builder));
        Self { state }
    }
    /// Internals of section_builder-type routines. Upon success, yields the index
    /// of the newly-added section builder.
    pub(crate) fn section_builder_internal(
        &mut self,
        identity: np_adv_dynamic::extended::BoxedEncoder,
    ) -> Result<usize, SectionBuilderError> {
        let state = self.state.take();
        match state {
            Some(V1AdvertisementBuilderState::Advertisement(adv_builder)) => {
                match adv_builder.into_section_builder(identity) {
                    Ok(section_builder) => {
                        let section_index = section_builder.section_index();
                        self.state = Some(V1AdvertisementBuilderState::Section(section_builder));
                        Ok(section_index)
                    }
                    Err((adv_builder, err)) => {
                        self.state = Some(V1AdvertisementBuilderState::Advertisement(adv_builder));
                        Err(err.into())
                    }
                }
            }
            x => {
                // Note: Technically, this case also would leave the `None` state
                // if we ever entered into it, but we never transition to that
                // state during normal operation.
                self.state = x;
                Err(SectionBuilderError::UnclosedActiveSection)
            }
        }
    }

    pub(crate) fn public_section_builder(&mut self) -> Result<usize, SectionBuilderError> {
        let identity = np_adv_dynamic::extended::BoxedEncoder::Unencrypted;
        self.section_builder_internal(identity)
    }
    pub(crate) fn encrypted_section_builder(
        &mut self,
        broadcast_cred: V1BroadcastCredential,
        verification_mode: V1VerificationMode,
    ) -> Result<usize, SectionBuilderError> {
        let mut rng = get_global_crypto_rng();
        let rng = rng.get_rng();
        let internal_broadcast_cred = broadcast_cred.into_internal();
        let encoder = match verification_mode {
            V1VerificationMode::Mic => {
                let encoder =
                    np_adv::extended::serialize::MicEncryptedSectionEncoder::<_>::new_wrapped_salt::<
                        CryptoProviderImpl,
                    >(rng, &internal_broadcast_cred);
                np_adv_dynamic::extended::BoxedEncoder::MicEncrypted(encoder)
            }
            V1VerificationMode::Signature => {
                let encoder =
                    np_adv::extended::serialize::SignedEncryptedSectionEncoder::new_random_salt::<
                        CryptoProviderImpl,
                    >(rng, &internal_broadcast_cred);
                np_adv_dynamic::extended::BoxedEncoder::SignedEncrypted(encoder)
            }
        };
        self.section_builder_internal(encoder)
    }

    #[cfg(feature = "testing")]
    pub(crate) fn mic_custom_salt_section_builder(
        &mut self,
        broadcast_cred: V1BroadcastCredential,
        salt: MultiSalt,
    ) -> Result<usize, SectionBuilderError> {
        let internal_broadcast_cred = broadcast_cred.into_internal();
        let encoder = np_adv::extended::serialize::MicEncryptedSectionEncoder::new_with_salt::<
            CryptoProviderImpl,
        >(salt, &internal_broadcast_cred);
        let encoder = np_adv_dynamic::extended::BoxedEncoder::MicEncrypted(encoder);
        self.section_builder_internal(encoder)
    }

    fn into_advertisement(self) -> SerializeV1AdvertisementResult {
        match self.state {
            Some(V1AdvertisementBuilderState::Advertisement(adv_builder)) => {
                let array_view = adv_builder.into_advertisement().into_array_view();
                SerializeV1AdvertisementResult::Success(ByteBuffer::from_array_view(array_view))
            }
            _ => SerializeV1AdvertisementResult::InvalidBuilderState,
        }
    }
}

/// Discriminant for `CreateV1SectionBuilderResult`
#[derive(Copy, Clone)]
#[repr(u8)]
pub enum CreateV1SectionBuilderResultKind {
    /// The attempt to create a new section builder succeeded.
    Success = 0,
    /// We're currently in the middle of building a section.
    UnclosedActiveSection = 1,
    /// The advertisement builder handle was invalid.
    InvalidAdvertisementBuilderHandle = 2,
    /// We're attempting to build a section with an identity
    /// kind (public/encrypted) which doesn't match the kind
    /// for the entire advertisement.
    IdentityKindMismatch = 3,
    /// There isn't enough space for a new section, either
    /// because the maximum section count has been exceeded
    /// or because the advertisement is almost full, and
    /// the minimum size of a section wouldn't fit.
    NoSpaceLeft = 4,
}

/// The result of attempting to create a new V1 section builder.
#[repr(C)]
#[allow(missing_docs)]
pub enum CreateV1SectionBuilderResult {
    Success(V1SectionBuilder),
    UnclosedActiveSection,
    InvalidAdvertisementBuilderHandle,
    IdentityKindMismatch,
    NoSpaceLeft,
}

impl FfiEnum for CreateV1SectionBuilderResult {
    type Kind = CreateV1SectionBuilderResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            Self::Success(_) => CreateV1SectionBuilderResultKind::Success,
            Self::UnclosedActiveSection => CreateV1SectionBuilderResultKind::UnclosedActiveSection,
            Self::InvalidAdvertisementBuilderHandle => {
                CreateV1SectionBuilderResultKind::InvalidAdvertisementBuilderHandle
            }
            Self::IdentityKindMismatch => CreateV1SectionBuilderResultKind::IdentityKindMismatch,
            Self::NoSpaceLeft => CreateV1SectionBuilderResultKind::NoSpaceLeft,
        }
    }
}

impl CreateV1SectionBuilderResult {
    declare_enum_cast! {into_success, Success, V1SectionBuilder}
}

impl From<SectionBuilderError> for CreateV1SectionBuilderResult {
    fn from(err: SectionBuilderError) -> Self {
        match err {
            SectionBuilderError::UnclosedActiveSection => Self::UnclosedActiveSection,
            SectionBuilderError::IdentityKindMismatch => Self::IdentityKindMismatch,
            SectionBuilderError::NoSpaceLeft => Self::NoSpaceLeft,
        }
    }
}

/// Result code for [`V1SectionBuilder#add_to_advertisement`].
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum AddV1SectionToAdvertisementResult {
    /// The section referenced by the given handle
    /// couldn't be added to the containing advertisement,
    /// possibly because the handle is invalid or the section
    /// has already been added to the containing section.
    Error = 0,
    /// The section referenced by the given handle
    /// was successfully added to the containing advertisement.
    /// After obtaining this result code, the section
    /// handle will no longer be valid.
    Success = 1,
}

/// Result code for operations adding DEs to a section builder.
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum AddV1DEResult {
    /// The DE was successfully added to the section builder
    /// behind the given handle.
    Success = 0,
    /// The handle for the section builder was invalid.
    InvalidSectionHandle = 1,
    /// There was no more space left in the advertisement
    /// to fit the DE in the containing section.
    InsufficientSectionSpace = 2,
    /// The data element itself had invalid characteristics,
    /// most likely a length above 127.
    InvalidDataElement = 3,
}

/// Discriminant for `NextV1DE16ByteSaltResult`.
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum NextV1DE16ByteSaltResultKind {
    /// We couldn't return a 16-byte DE salt, possibly
    /// because the handle to the section builder
    /// was invalid, or possibly because the section
    /// builder was for a public section.
    Error = 0,
    /// A 16-byte DE salt was returned successfully.
    Success = 1,
}

/// The result of attempting to get the derived V1 DE
/// 16-byte salt for the next-added DE to the section
/// builder behind the given handle.
#[derive(Clone)]
#[repr(C)]
#[allow(missing_docs)]
pub enum NextV1DE16ByteSaltResult {
    Error,
    Success(FixedSizeArray<16>),
}

impl FfiEnum for NextV1DE16ByteSaltResult {
    type Kind = NextV1DE16ByteSaltResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            Self::Error => NextV1DE16ByteSaltResultKind::Error,
            Self::Success(_) => NextV1DE16ByteSaltResultKind::Success,
        }
    }
}

impl NextV1DE16ByteSaltResult {
    fn new_from_de_salt(salt: Option<np_adv::extended::serialize::DeSalt>) -> Self {
        match salt.and_then(|salt| salt.derive::<16, CryptoProviderImpl>()) {
            Some(salt) => NextV1DE16ByteSaltResult::Success(FixedSizeArray::from_array(salt)),
            None => NextV1DE16ByteSaltResult::Error,
        }
    }

    declare_enum_cast! {into_success, Success, FixedSizeArray<16> }
}

/// A handle to a builder for V1 sections. This is not a unique handle; it is the same handle as
/// the advertisement builder the section builder was originated from.
#[derive(Clone, Copy)]
#[repr(C)]
pub struct V1SectionBuilder {
    /// The parent advertisement builder for this section
    pub adv_builder: V1AdvertisementBuilder,
    /// This section's index in the parent advertisement
    pub section_index: u8,
}

impl V1SectionBuilder {
    /// Attempts to add the section constructed behind this handle
    /// to a section builder to the containing advertisement it
    /// originated from.
    pub fn add_to_advertisement(self) -> AddV1SectionToAdvertisementResult {
        match self.adv_builder.get_mut() {
            Ok(mut adv_builder) => {
                let state = adv_builder.state.take();
                match state {
                    Some(V1AdvertisementBuilderState::Section(section_builder)) => {
                        // Make sure the index of the section we're trying to close
                        // matches the index of the section currently under construction.
                        let actual_section_index = section_builder.section_index() as u8;
                        if self.section_index == actual_section_index {
                            let updated_adv_builder =
                                section_builder.add_to_advertisement::<CryptoProviderImpl>();
                            adv_builder.state = Some(V1AdvertisementBuilderState::Advertisement(
                                updated_adv_builder,
                            ));
                            AddV1SectionToAdvertisementResult::Success
                        } else {
                            adv_builder.state =
                                Some(V1AdvertisementBuilderState::Section(section_builder));
                            AddV1SectionToAdvertisementResult::Error
                        }
                    }
                    x => {
                        adv_builder.state = x;
                        AddV1SectionToAdvertisementResult::Error
                    }
                }
            }
            Err(_) => AddV1SectionToAdvertisementResult::Error,
        }
    }

    /// Attempts to get the derived 16-byte V1 DE salt for the next
    /// DE to be added to this section builder. May fail if this
    /// section builder handle is invalid, or if the section
    /// is a public section.
    pub fn next_de_salt(&self) -> NextV1DE16ByteSaltResult {
        self.try_apply_to_internals(
            |section_builder| {
                NextV1DE16ByteSaltResult::new_from_de_salt(section_builder.next_de_salt())
            },
            NextV1DE16ByteSaltResult::Error,
        )
    }

    /// Attempts to add the given DE to the section builder behind
    /// this handle. The passed DE may have a payload of up to 127
    /// bytes, the maximum for a V1 DE.
    pub fn add_127_byte_buffer_de(&self, de: V1DE127ByteBuffer) -> AddV1DEResult {
        match de.into_internal() {
            Some(generic_de) => self
                .add_de_internals(np_adv_dynamic::extended::BoxedWriteDataElement::new(generic_de)),
            None => AddV1DEResult::InvalidDataElement,
        }
    }

    fn add_de_internals(
        &self,
        de: np_adv_dynamic::extended::BoxedWriteDataElement,
    ) -> AddV1DEResult {
        self.try_apply_to_internals(
            move |section_builder| match section_builder.add_de(move |_| de) {
                Ok(_) => AddV1DEResult::Success,
                Err(_) => AddV1DEResult::InsufficientSectionSpace,
            },
            AddV1DEResult::InvalidSectionHandle,
        )
    }

    fn try_apply_to_internals<R>(
        &self,
        func: impl FnOnce(
            &mut np_adv_dynamic::extended::BoxedSectionBuilder<
                np_adv::extended::serialize::AdvBuilder,
            >,
        ) -> R,
        invalid_handle_error: R,
    ) -> R {
        match self.adv_builder.get_mut() {
            Ok(mut adv_builder) => {
                match adv_builder.state.as_mut() {
                    Some(V1AdvertisementBuilderState::Section(ref mut section_builder)) => {
                        // Check to make sure that the section index matches, otherwise
                        // we have an invalid handle.
                        let current_section_index = section_builder.section_index() as u8;
                        if current_section_index == self.section_index {
                            func(section_builder)
                        } else {
                            invalid_handle_error
                        }
                    }
                    Some(V1AdvertisementBuilderState::Advertisement(_)) => invalid_handle_error,
                    None => invalid_handle_error,
                }
            }
            Err(_) => invalid_handle_error,
        }
    }
}

/// Represents the contents of a V1 DE whose payload
/// is stored in a buffer which may contain up to 127 bytes,
/// which is the maximum for any V1 DE.
///
/// This representation is stable, and so you may directly
/// reference this struct's fields if you wish.
#[repr(C)]
//TODO: Partial unification with `deserialize::v1::GenericV1DataElement`?
pub struct V1DE127ByteBuffer {
    /// The DE type code of this generic data-element.
    pub de_type: u32,
    /// The raw data-element byte payload, up to
    /// 127 bytes in length.
    pub payload: ByteBuffer<127>,
}

impl V1DE127ByteBuffer {
    /// Attempts to convert this FFI-friendly DE with a byte-buffer size of 127
    /// to the internal representation of a generic DE. May fail in the case
    /// where the underlying payload byte-buffer has an invalid length above 127.
    fn into_internal(self) -> Option<np_adv::extended::data_elements::GenericDataElement> {
        let de_type = np_adv::extended::de_type::DeType::from(self.de_type);
        self.payload.as_slice().and_then(move |payload_slice| {
            np_adv::extended::data_elements::GenericDataElement::try_from(de_type, payload_slice)
                .ok()
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn state_is_advertisement_building(adv_builder_state: &V1AdvertisementBuilderState) -> bool {
        matches!(adv_builder_state, V1AdvertisementBuilderState::Advertisement(_))
    }

    #[allow(clippy::expect_used)]
    #[test]
    fn test_build_section_fails_with_outstanding_section() {
        let mut adv_builder = V1AdvertisementBuilderInternals::new(AdvertisementType::Encrypted);

        let adv_builder_state =
            adv_builder.state.as_ref().expect("Adv builder state should be present.");
        assert!(state_is_advertisement_building(adv_builder_state));
        let section_index = adv_builder
            .encrypted_section_builder(empty_broadcast_cred(), V1VerificationMode::Mic)
            .expect("Should be able to build the first section.");
        assert_eq!(section_index, 0);

        assert!(adv_builder.state.is_some());
        let adv_builder_state =
            adv_builder.state.as_ref().expect("Adv builder state should be present.");
        assert!(!state_is_advertisement_building(adv_builder_state));

        let double_build_error = adv_builder
            .encrypted_section_builder(empty_broadcast_cred(), V1VerificationMode::Mic)
            .expect_err("Shouldn't be able to start a new section builder with an unclosed one.");
        assert_eq!(double_build_error, SectionBuilderError::UnclosedActiveSection);
    }

    fn empty_broadcast_cred() -> V1BroadcastCredential {
        V1BroadcastCredential::new([0; 32], [0; 16].into(), [0; 32])
    }
}
