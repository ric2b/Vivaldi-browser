// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! Rust wrappers for Java classes.
//!
//! The pattern being used here it to create a new wrapper type for each Java class being used by
//! this library. Each wrapper type will implement the required accessors for its members so that
//! JNI code using a member can be easily found in case that member is changed. Native methods will
//! also be implemented along side the wrapper of the class they are implementing.
//!
//! This library is primarily meant to be used from Java. The Java entrypoint to this library is
//! `class NpAdv`.

use jni::JNIEnv;

/// Trait to allow Java exceptions to be thrown from rust Errors
pub trait ToJavaException {
    /// Convert this error to a Java exception and throw it.
    fn throw_java_exception<'env>(&self, env: &mut JNIEnv<'env>) -> jni::errors::Result<()>;
}

mod credential_book;
mod credential_slab;
mod deserialization_exception;
mod deserialize_result;
mod deserialized_v0_advertisement;
mod deserialized_v1_advertisement;
mod deserialized_v1_section;
mod handle;
mod identity_kind;
mod legible_v1_sections;
mod np_adv;
mod owned_handle;
mod serialization_exception;
mod v0_advertisement_builder;
mod v0_broadcast_credential;
mod v0_discovery_credential;
mod v0_payload;
mod v1_advertisement_builder;
mod v1_broadcast_credential;
mod v1_discovery_credential;
mod verification_mode;

pub mod v0_data_element;
pub mod v1_data_element;

pub use credential_book::CredentialBook;
pub use deserialization_exception::{InvalidFormatException, InvalidHeaderException};
pub use deserialize_result::{DeserializeResult, DeserializeResultError};
pub use deserialized_v0_advertisement::{DeserializedV0Advertisement, V0AdvertisementError};
pub use deserialized_v1_advertisement::DeserializedV1Advertisement;
pub use deserialized_v1_section::DeserializedV1Section;
pub use handle::InvalidHandleException;
pub use identity_kind::IdentityKind;
pub use legible_v1_sections::LegibleV1Sections;
pub use owned_handle::NoSpaceLeftException;
pub use serialization_exception::{
    InsufficientSpaceException, InvalidDataElementException, InvalidSectionKindException,
    LdtEncryptionException, UnclosedActiveSectionException, UnencryptedSizeException,
};
pub use v0_broadcast_credential::V0BroadcastCredential;
pub use v0_discovery_credential::V0DiscoveryCredential;
pub use v1_advertisement_builder::{V1BuilderHandle, V1SectionBuilder};
pub use v1_broadcast_credential::V1BroadcastCredential;
pub use v1_discovery_credential::V1DiscoveryCredential;
pub use verification_mode::VerificationMode;
