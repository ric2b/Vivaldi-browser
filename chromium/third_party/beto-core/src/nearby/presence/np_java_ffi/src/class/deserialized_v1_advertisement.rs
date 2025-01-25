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
use pourover::desc::ClassDesc;

use crate::class::CredentialBook;

static DESERIALIZED_V1_ADVERTISEMENT_CLASS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/DeserializedV1Advertisement");

/// Rust representation of `class DeserializedV1Advertisement`.
#[repr(transparent)]
pub struct DeserializedV1Advertisement<Obj>(pub Obj);

impl<'local> DeserializedV1Advertisement<JObject<'local>> {
    /// Create a new advertisement.
    pub fn construct<'sections, 'book>(
        env: &mut JNIEnv<'local>,
        num_legible_sections: jint,
        num_undecryptable_sections: jint,
        legible_sections: super::LegibleV1Sections<impl AsRef<JObject<'sections>>>,
        credential_book: CredentialBook<impl AsRef<JObject<'book>>>,
    ) -> jni::errors::Result<Self> {
        pourover::call_constructor!(
            env,
            &DESERIALIZED_V1_ADVERTISEMENT_CLASS,
            "(IILcom/google/android/nearby/presence/rust/LegibleV1Sections;Lcom/google/android/nearby/presence/rust/credential/CredentialBook;)V",
            num_legible_sections,
            num_undecryptable_sections,
            legible_sections.as_obj(),
            credential_book.as_obj(),
        )
        .map(Self)
    }
}

impl<'local, Obj: AsRef<JObject<'local>>> DeserializedV1Advertisement<Obj> {
    /// Get a reference to the inner `jni` crate [`JObject`].
    pub fn as_obj(&self) -> &JObject<'local> {
        self.0.as_ref()
    }
}
