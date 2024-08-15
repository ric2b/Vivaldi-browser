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
//! Core NP Rust FFI structures and methods for v1 advertisement deserialization.

use super::DeserializeAdvertisementError;
use crate::common::*;
use crate::credentials::credential_book::CredentialBook;
use crate::credentials::MatchedCredential;
use crate::deserialize::DecryptMetadataError;
use crate::utils::*;
use array_view::ArrayView;
use crypto_provider_default::CryptoProviderImpl;
use handle_map::{declare_handle_map, HandleLike, HandleMapDimensions};
use legible_v1_sections::LegibleV1Sections;
use np_adv::extended::deserialize::DataElementParseError;
use np_adv::HasIdentityMatch;
use std::vec::Vec;

/// Representation of a deserialized V1 advertisement
#[repr(C)]
pub struct DeserializedV1Advertisement {
    num_legible_sections: u8,
    num_undecryptable_sections: u8,
    legible_sections: LegibleV1Sections,
}

impl DeserializedV1Advertisement {
    /// Gets the number of legible sections in this deserialized V1 advertisement.
    pub fn num_legible_sections(&self) -> u8 {
        self.num_legible_sections
    }

    /// Gets the number of undecryptable sections in this deserialized V1 advertisement.
    pub fn num_undecryptable_sections(&self) -> u8 {
        self.num_undecryptable_sections
    }

    /// Gets the legible section with the given index
    /// (which is bounded in `0..self.num_legible_sections()`)
    pub fn get_section(&self, legible_section_index: u8) -> GetV1SectionResult {
        match self.legible_sections.get() {
            Ok(sections_read_guard) => {
                sections_read_guard.get_section(self.legible_sections, legible_section_index)
            }
            Err(_) => GetV1SectionResult::Error,
        }
    }

    /// Attempts to deallocate memory utilized internally by this V1 advertisement
    /// (which contains a handle to actual advertisement contents behind-the-scenes).
    pub fn deallocate(self) -> DeallocateResult {
        self.legible_sections.deallocate().map(|_| ()).into()
    }

    pub(crate) fn allocate_with_contents(
        contents: np_adv::V1AdvertisementContents<
            np_adv::credential::ReferencedMatchedCredential<MatchedCredential>,
        >,
    ) -> Result<Self, DeserializeAdvertisementError> {
        // 16-section limit enforced by np_adv
        let num_undecryptable_sections = contents.invalid_sections_count() as u8;
        let legible_sections = contents.into_sections();
        let num_legible_sections = legible_sections.len() as u8;
        let legible_sections =
            LegibleV1Sections::allocate_with_contents(legible_sections.into_vec())?;
        Ok(Self { num_undecryptable_sections, num_legible_sections, legible_sections })
    }
}

/// Internal, Rust-side implementation of a listing of legible sections
/// in a deserialized V1 advertisement
pub struct LegibleV1SectionsInternals {
    sections: Vec<DeserializedV1SectionInternals>,
}

impl LegibleV1SectionsInternals {
    fn get_section_internals(
        &self,
        legible_section_index: u8,
    ) -> Option<&DeserializedV1SectionInternals> {
        self.sections.get(legible_section_index as usize)
    }
    fn get_section(
        &self,
        legible_sections_handle: LegibleV1Sections,
        legible_section_index: u8,
    ) -> GetV1SectionResult {
        match self.get_section_internals(legible_section_index) {
            Some(section_ref) => {
                // Determine whether the section is plaintext
                // or decrypted to report back to the caller,
                // and also determine the number of contained DEs.
                let num_des = section_ref.num_des();
                let identity_tag = section_ref.identity_kind();
                GetV1SectionResult::Success(DeserializedV1Section {
                    legible_sections_handle,
                    legible_section_index,
                    num_des,
                    identity_tag,
                })
            }
            None => GetV1SectionResult::Error,
        }
    }
}

impl<'adv>
    TryFrom<
        Vec<
            np_adv::V1DeserializedSection<
                'adv,
                np_adv::credential::ReferencedMatchedCredential<'adv, MatchedCredential>,
            >,
        >,
    > for LegibleV1SectionsInternals
{
    type Error = DataElementParseError;

    fn try_from(
        contents: Vec<
            np_adv::V1DeserializedSection<
                'adv,
                np_adv::credential::ReferencedMatchedCredential<'adv, MatchedCredential>,
            >,
        >,
    ) -> Result<Self, Self::Error> {
        let sections = contents
            .into_iter()
            .map(DeserializedV1SectionInternals::try_from)
            .collect::<Result<Vec<_>, _>>()?;
        Ok(Self { sections })
    }
}

fn get_legible_v1_sections_handle_map_dimensions() -> HandleMapDimensions {
    HandleMapDimensions {
        num_shards: global_num_shards(),
        max_active_handles: global_max_num_deserialized_v1_advertisements(),
    }
}

declare_handle_map! {
    mod legible_v1_sections {
        #[dimensions = super::get_legible_v1_sections_handle_map_dimensions()]
        type LegibleV1Sections: HandleLike<Object = super::LegibleV1SectionsInternals>;
    }
}

impl LocksLongerThan<LegibleV1Sections> for CredentialBook {}

impl LegibleV1Sections {
    pub(crate) fn allocate_with_contents(
        contents: Vec<
            np_adv::V1DeserializedSection<
                np_adv::credential::ReferencedMatchedCredential<MatchedCredential>,
            >,
        >,
    ) -> Result<Self, DeserializeAdvertisementError> {
        let section = LegibleV1SectionsInternals::try_from(contents)
            .map_err(|_| DeserializeAdvertisementError)?;
        Self::allocate(move || section).map_err(|e| e.into())
    }
}

/// Discriminant for `GetV1SectionResult`
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum GetV1SectionResultKind {
    /// The attempt to get the section failed,
    /// possibly due to the section index being
    /// out-of-bounds or due to the underlying
    /// advertisement having already been deallocated.
    Error = 0,
    /// The attempt to get the section succeeded.
    /// The wrapped section may be obtained via
    /// `GetV1SectionResult#into_success`.
    Success = 1,
}

/// The result of attempting to get a particular V1 section
/// from its' index within the list of legible sections
/// via `DeserializedV1Advertisement::get_section`.
#[repr(C)]
#[allow(missing_docs)]
pub enum GetV1SectionResult {
    Error,
    Success(DeserializedV1Section),
}

impl FfiEnum for GetV1SectionResult {
    type Kind = GetV1SectionResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            GetV1SectionResult::Error => GetV1SectionResultKind::Error,
            GetV1SectionResult::Success(_) => GetV1SectionResultKind::Success,
        }
    }
}

impl GetV1SectionResult {
    declare_enum_cast! {into_success, Success, DeserializedV1Section}
}

/// Discriminant for `GetV1DE16ByteSaltResult`.
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum GetV1DE16ByteSaltResultKind {
    /// The attempt to get the derived salt failed, possibly
    /// because the passed DE offset was invalid (==255),
    /// or because there was no salt included for the
    /// referenced advertisement section (i.e: it was
    /// a public advertisement section, or it was deallocated.)
    Error = 0,
    /// A 16-byte salt for the given DE offset was successfully
    /// derived.
    Success = 1,
}

/// The result of attempting to get a derived 16-byte salt
/// for a given DE within a section.
#[derive(Copy, Clone)]
#[repr(C)]
#[allow(missing_docs)]
pub enum GetV1DE16ByteSaltResult {
    Error,
    Success([u8; 16]),
}

impl FfiEnum for GetV1DE16ByteSaltResult {
    type Kind = GetV1DE16ByteSaltResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            GetV1DE16ByteSaltResult::Error => GetV1DE16ByteSaltResultKind::Error,
            GetV1DE16ByteSaltResult::Success(_) => GetV1DE16ByteSaltResultKind::Success,
        }
    }
}

/// The internal FFI-friendly representation of a deserialized v1 section
pub struct DeserializedV1SectionInternals {
    des: Vec<V1DataElement>,
    identity: Option<DeserializedV1IdentityInternals>,
}

impl DeserializedV1SectionInternals {
    /// Gets the number of data-elements in this section.
    fn num_des(&self) -> u8 {
        self.des.len() as u8
    }
    /// Gets the enum tag of the identity used for this section.
    fn identity_kind(&self) -> DeserializedV1IdentityKind {
        if self.identity.is_some() {
            DeserializedV1IdentityKind::Decrypted
        } else {
            DeserializedV1IdentityKind::Plaintext
        }
    }
    /// Attempts to get the DE with the given index in this section.
    fn get_de(&self, index: u8) -> GetV1DEResult {
        match self.des.get(index as usize) {
            Some(de) => GetV1DEResult::Success(de.clone()),
            None => GetV1DEResult::Error,
        }
    }
    /// Attempts to get the directly-transmissible details about
    /// the deserialized V1 identity for this section. Does
    /// not include decrypted metadata bytes nor the section salt.
    pub(crate) fn get_identity_details(&self) -> GetV1IdentityDetailsResult {
        match &self.identity {
            Some(identity) => GetV1IdentityDetailsResult::Success(identity.details()),
            None => GetV1IdentityDetailsResult::Error,
        }
    }
    /// Attempts to decrypt the metadata for the matched
    /// credential for this V1 section (if any).
    pub(crate) fn decrypt_metadata(&self) -> Result<Vec<u8>, DecryptMetadataError> {
        match &self.identity {
            None => Err(DecryptMetadataError::EncryptedMetadataNotAvailable),
            Some(identity) => {
                identity.decrypt_metadata().ok_or(DecryptMetadataError::DecryptionFailed)
            }
        }
    }
    /// Attempts to derive a 16-byte DE salt for a DE in this section
    /// with the given DE offset. This operation may fail if the
    /// passed offset is 255 (causes overflow) or if the section
    /// is leveraging a public identity, and hence, doesn't have
    /// an associated salt.
    pub(crate) fn derive_16_byte_salt_for_offset(&self, de_offset: u8) -> GetV1DE16ByteSaltResult {
        self.identity
            .as_ref()
            .and_then(|x| x.derive_16_byte_salt_for_offset(de_offset))
            .map_or(GetV1DE16ByteSaltResult::Error, GetV1DE16ByteSaltResult::Success)
    }
}

impl<'adv>
    TryFrom<
        np_adv::V1DeserializedSection<
            'adv,
            np_adv::credential::ReferencedMatchedCredential<'adv, MatchedCredential>,
        >,
    > for DeserializedV1SectionInternals
{
    type Error = DataElementParseError;

    fn try_from(
        section: np_adv::V1DeserializedSection<
            np_adv::credential::ReferencedMatchedCredential<'adv, MatchedCredential>,
        >,
    ) -> Result<Self, Self::Error> {
        use np_adv::extended::deserialize::Section;
        use np_adv::V1DeserializedSection;
        match section {
            V1DeserializedSection::Plaintext(section) => {
                let des = section
                    .iter_data_elements()
                    .map(|r| r.map(|de| V1DataElement::from(&de)))
                    .collect::<Result<Vec<_>, _>>()?;
                let identity = None;
                Ok(Self { des, identity })
            }
            V1DeserializedSection::Decrypted(with_matched) => {
                let section = with_matched.contents();
                let des = section
                    .iter_data_elements()
                    .map(|r| r.map(|de| V1DataElement::from(&de)))
                    .collect::<Result<Vec<_>, _>>()?;

                let identity_type = section.identity_type();
                let verification_mode = section.verification_mode();
                let salt = section.salt();

                let match_data = with_matched.clone_match_data();
                let match_data = match_data.map(|x| x.metadata_key());

                let identity = Some(DeserializedV1IdentityInternals::new(
                    identity_type,
                    verification_mode,
                    salt,
                    match_data,
                ));
                Ok(Self { des, identity })
            }
        }
    }
}
/// Discriminant for `DeserializedV1Identity`.
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum DeserializedV1IdentityKind {
    /// The deserialized v1 identity was plaintext
    Plaintext = 0,
    /// The deserialized v1 identity corresponded
    /// to some kind of decrypted identity.
    Decrypted = 1,
}

/// Internals for the representation of a decrypted
/// V1 section identity.
pub(crate) struct DeserializedV1IdentityInternals {
    /// The details about the identity, suitable
    /// for direct communication over FFI
    details: DeserializedV1IdentityDetails,
    /// The metadata key, together with the matched
    /// credential and enough information to decrypt
    /// the credential metadata, if desired.
    match_data: np_adv::WithMatchedCredential<MatchedCredential, np_adv::MetadataKey>,
    /// The 16-byte section salt
    salt: np_adv::extended::deserialize::RawV1Salt,
}

impl DeserializedV1IdentityInternals {
    pub(crate) fn new(
        identity_type: np_adv::de_type::EncryptedIdentityDataElementType,
        verification_mode: np_adv::extended::deserialize::VerificationMode,
        salt: np_adv::extended::deserialize::RawV1Salt,
        match_data: np_adv::WithMatchedCredential<MatchedCredential, np_adv::MetadataKey>,
    ) -> Self {
        let cred_id = match_data.matched_credential().id();
        let metadata_key = match_data.contents();
        let details = DeserializedV1IdentityDetails::new(
            cred_id,
            identity_type,
            verification_mode,
            *metadata_key,
        );
        Self { details, match_data, salt }
    }
    /// Gets the directly-transmissible details about
    /// this deserialized V1 identity. Does not include
    /// decrypted metadata bytes nor the section salt.
    pub(crate) fn details(&self) -> DeserializedV1IdentityDetails {
        self.details
    }
    /// Attempts to decrypt the metadata associated
    /// with this identity.
    pub(crate) fn decrypt_metadata(&self) -> Option<Vec<u8>> {
        self.match_data.decrypt_metadata::<CryptoProviderImpl>().ok()
    }
    /// For a given data-element offset, derives a 16-byte DE salt
    /// for a DE in that position within this section.
    pub(crate) fn derive_16_byte_salt_for_offset(&self, de_offset: u8) -> Option<[u8; 16]> {
        let section_salt = np_hkdf::v1_salt::V1Salt::<CryptoProviderImpl>::from(self.salt);
        let de_offset = np_hkdf::v1_salt::DataElementOffset::from(de_offset);
        section_salt.derive::<16>(Some(de_offset))
    }
}

/// Information about the verification scheme used
/// for verifying the integrity of the contents
/// of a decrypted section.
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum V1VerificationMode {
    /// Message integrity code verification.
    Mic = 0,
    /// Signature verification.
    Signature = 1,
}

impl From<np_adv::extended::deserialize::VerificationMode> for V1VerificationMode {
    fn from(verification_mode: np_adv::extended::deserialize::VerificationMode) -> Self {
        use np_adv::extended::deserialize::VerificationMode;
        match verification_mode {
            VerificationMode::Mic => Self::Mic,
            VerificationMode::Signature => Self::Signature,
        }
    }
}

/// Discriminant for `GetV1IdentityDetailsResult`
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum GetV1IdentityDetailsResultKind {
    /// The attempt to get the identity details
    /// for the section failed, possibly
    /// due to the section being a public
    /// section, or the underlying
    /// advertisement has already been deallocated.
    Error = 0,
    /// The attempt to get the identity details succeeded.
    /// The wrapped identity details may be obtained via
    /// `GetV1IdentityDetailsResult#into_success`.
    Success = 1,
}

/// The result of attempting to get the identity details
/// for a V1 advertisement section via
/// `DeserializedV1Advertisement#get_identity_details`.
#[repr(C)]
#[allow(missing_docs)]
pub enum GetV1IdentityDetailsResult {
    Error,
    Success(DeserializedV1IdentityDetails),
}

impl FfiEnum for GetV1IdentityDetailsResult {
    type Kind = GetV1IdentityDetailsResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            GetV1IdentityDetailsResult::Error => GetV1IdentityDetailsResultKind::Error,
            GetV1IdentityDetailsResult::Success(_) => GetV1IdentityDetailsResultKind::Success,
        }
    }
}

impl GetV1IdentityDetailsResult {
    declare_enum_cast! {into_success, Success, DeserializedV1IdentityDetails}
}

/// Information about the identity which matched
/// a decrypted V1 section.
#[derive(Clone, Copy)]
#[repr(C)]
pub struct DeserializedV1IdentityDetails {
    /// The identity type (private/provisioned/trusted)
    identity_type: EncryptedIdentityType,
    /// The verification mode (MIC/Signature) which
    /// was used to verify the decrypted adv contents.
    verification_mode: V1VerificationMode,
    /// The ID of the credential which
    /// matched the deserialized section.
    cred_id: u32,
    /// The 16-byte metadata key.
    metadata_key: [u8; 16],
}

impl DeserializedV1IdentityDetails {
    pub(crate) fn new(
        cred_id: u32,
        identity_type: np_adv::de_type::EncryptedIdentityDataElementType,
        verification_mode: np_adv::extended::deserialize::VerificationMode,
        metadata_key: np_adv::MetadataKey,
    ) -> Self {
        let metadata_key = metadata_key.0;
        let identity_type = identity_type.into();
        let verification_mode = verification_mode.into();
        Self { cred_id, identity_type, verification_mode, metadata_key }
    }
    /// Returns the ID of the credential which
    /// matched the deserialized section.
    pub fn cred_id(&self) -> u32 {
        self.cred_id
    }
    /// Returns the identity type (private/provisioned/trusted)
    pub fn identity_type(&self) -> EncryptedIdentityType {
        self.identity_type
    }
    /// Returns the verification mode (MIC/Signature)
    /// employed for the decrypted section.
    pub fn verification_mode(&self) -> V1VerificationMode {
        self.verification_mode
    }
    /// Returns the 16-byte section metadata key.
    pub fn metadata_key(&self) -> [u8; 16] {
        self.metadata_key
    }
}

/// Handle to a deserialized V1 section
#[repr(C)]
pub struct DeserializedV1Section {
    legible_sections_handle: LegibleV1Sections,
    legible_section_index: u8,
    num_des: u8,
    identity_tag: DeserializedV1IdentityKind,
}

impl DeserializedV1Section {
    /// Gets the number of data elements contained in this section.
    /// Suitable as an iteration bound on `Self::get_de`.
    pub fn num_des(&self) -> u8 {
        self.num_des
    }

    /// Gets the enum tag of the identity employed by this deserialized section.
    pub fn identity_kind(&self) -> DeserializedV1IdentityKind {
        self.identity_tag
    }

    fn apply_to_section_internals<R>(
        &self,
        func: impl FnOnce(&DeserializedV1SectionInternals) -> R,
        lookup_failure_result: R,
    ) -> R {
        // TODO: Once the `FromResidual` trait is stabilized, this can be simplified.
        match self.legible_sections_handle.get() {
            Ok(legible_sections_read_guard) => {
                match legible_sections_read_guard.get_section_internals(self.legible_section_index)
                {
                    Some(section_ref) => func(section_ref),
                    None => lookup_failure_result,
                }
            }
            Err(_) => lookup_failure_result,
        }
    }
    /// Gets the DE with the given index in this section.
    pub fn get_de(&self, de_index: u8) -> GetV1DEResult {
        self.apply_to_section_internals(
            move |section_ref| section_ref.get_de(de_index),
            GetV1DEResult::Error,
        )
    }
    /// Attempts to get the details of the identity employed
    /// for the section referenced by this handle. May fail
    /// if the handle is invalid, or if the advertisement
    /// section leverages a public identity.
    pub fn get_identity_details(&self) -> GetV1IdentityDetailsResult {
        self.apply_to_section_internals(
            DeserializedV1SectionInternals::get_identity_details,
            GetV1IdentityDetailsResult::Error,
        )
    }
    /// Attempts to decrypt the metadata for the matched
    /// credential for the V1 section referenced by
    /// this handle (if any).
    pub fn decrypt_metadata(&self) -> Result<Vec<u8>, DecryptMetadataError> {
        self.apply_to_section_internals(
            DeserializedV1SectionInternals::decrypt_metadata,
            Err(DecryptMetadataError::EncryptedMetadataNotAvailable),
        )
    }
    /// Attempts to derive a 16-byte DE salt for a DE in this section
    /// with the given DE offset. This operation may fail if the
    /// passed offset is 255 (causes overflow) or if the section
    /// is leveraging a public identity, and hence, doesn't have
    /// an associated salt.
    pub fn derive_16_byte_salt_for_offset(&self, de_offset: u8) -> GetV1DE16ByteSaltResult {
        self.apply_to_section_internals(
            move |section_ref| section_ref.derive_16_byte_salt_for_offset(de_offset),
            GetV1DE16ByteSaltResult::Error,
        )
    }
}

/// Discriminant for the `GetV1DEResult` enum.
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum GetV1DEResultKind {
    /// Attempting to get the DE at the given position failed,
    /// possibly due to the index being out-of-bounds or due
    /// to the whole advertisement having been previously deallocated.
    Error = 0,
    /// Attempting to get the DE at the given position succeeded.
    /// The underlying DE may be extracted with `GetV1DEResult#into_success`.
    Success = 1,
}

/// Represents the result of the `DeserializedV1Section#get_de` operation.
#[repr(C)]
#[allow(missing_docs)]
pub enum GetV1DEResult {
    Error,
    Success(V1DataElement),
}

impl FfiEnum for GetV1DEResult {
    type Kind = GetV1DEResultKind;
    fn kind(&self) -> Self::Kind {
        match self {
            GetV1DEResult::Error => GetV1DEResultKind::Error,
            GetV1DEResult::Success(_) => GetV1DEResultKind::Success,
        }
    }
}

impl GetV1DEResult {
    declare_enum_cast! {into_success, Success, V1DataElement}
}

/// FFI-transmissible representation of a V1 data-element
#[derive(Clone)]
#[repr(C)]
pub enum V1DataElement {
    /// A "generic" V1 data-element, for which we have no
    /// particular information about its schema (just
    /// a DE type code and a byte payload.)
    Generic(GenericV1DataElement),
}

impl V1DataElement {
    // Note: not using declare_enum_cast! for this one, because if V1DataElement
    // gets more variants, this will have a different internal implementation
    /// Converts a `V1DataElement` to a `GenericV1DataElement` which
    /// only maintains information about the DE's type-code and payload.
    pub fn to_generic(self) -> GenericV1DataElement {
        match self {
            V1DataElement::Generic(x) => x,
        }
    }
}

impl<'a> From<&'a np_adv::extended::deserialize::DataElement<'a>> for V1DataElement {
    fn from(de: &'a np_adv::extended::deserialize::DataElement<'a>) -> Self {
        let offset = de.offset().as_u8();
        let de_type = V1DEType::from(de.de_type());
        let contents_as_slice = de.contents();
        //Guaranteed not to panic due DE size limit.
        #[allow(clippy::unwrap_used)]
        let array_view: ArrayView<u8, 127> = ArrayView::try_from_slice(contents_as_slice).unwrap();
        let payload = ByteBuffer::from_array_view(array_view);
        Self::Generic(GenericV1DataElement { de_type, offset, payload })
    }
}

/// FFI-transmissible representation of a generic V1 data-element.
/// This representation is stable, and so you may directly
/// reference this struct's fields if you wish.
#[derive(Clone)]
#[repr(C)]
pub struct GenericV1DataElement {
    /// The offset of this generic data-element.
    pub offset: u8,
    /// The DE type code of this generic data-element.
    pub de_type: V1DEType,
    /// The raw data-element byte payload, up to
    /// 127 bytes in length.
    pub payload: ByteBuffer<127>,
}

impl GenericV1DataElement {
    /// Gets the offset for this generic V1 data element.
    pub fn offset(&self) -> u8 {
        self.offset
    }
    /// Gets the DE-type of this generic V1 data element.
    pub fn de_type(&self) -> V1DEType {
        self.de_type
    }
    /// Destructures this `GenericV1DataElement` into just the
    /// DE payload byte-buffer.
    pub fn into_payload(self) -> ByteBuffer<127> {
        self.payload
    }
}

/// Representation of the data-element type tag
/// of a V1 data element.
#[derive(Clone, Copy)]
#[repr(C)]
pub struct V1DEType {
    code: u32,
}

impl From<np_adv::extended::de_type::DeType> for V1DEType {
    fn from(de_type: np_adv::extended::de_type::DeType) -> Self {
        let code = de_type.as_u32();
        Self { code }
    }
}

impl V1DEType {
    /// Yields this V1 DE type code as a u32.
    pub fn to_u32(&self) -> u32 {
        self.code
    }
}
