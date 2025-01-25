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

use jni::{objects::JObject, sys::jint, JNIEnv};
use np_ffi_core::deserialize::v1::DeserializedV1IdentityKind;
use pourover::desc::ClassDesc;

use crate::class::{CredentialBook, IdentityKind, LegibleV1Sections};

static DESERIALIZED_V1_SECTION_CLASS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/DeserializedV1Section");

/// Rust representation of `class DeserializedV1Section`.
#[repr(transparent)]
pub struct DeserializedV1Section<Obj>(pub Obj);

impl<'local> DeserializedV1Section<JObject<'local>> {
    /// Create a new deserialized section
    pub fn construct<'handle, 'book>(
        env: &mut JNIEnv<'local>,
        legible_sections_handle: LegibleV1Sections<impl AsRef<JObject<'handle>>>,
        legible_section_index: u8,
        num_des: u8,
        identity_kind: DeserializedV1IdentityKind,
        credential_book: CredentialBook<impl AsRef<JObject<'book>>>,
    ) -> jni::errors::Result<Self> {
        let identity = IdentityKind::from(identity_kind).to_java(env)?;

        pourover::call_constructor!(
            env,
            &DESERIALIZED_V1_SECTION_CLASS,
            "(Lcom/google/android/nearby/presence/rust/LegibleV1Sections;IIILcom/google/android/nearby/presence/rust/credential/CredentialBook;)V",
            legible_sections_handle.as_obj(),
            jint::from(legible_section_index),
            jint::from(num_des),
            identity,
            credential_book.as_obj()
        )
        .map(Self)
    }
}
