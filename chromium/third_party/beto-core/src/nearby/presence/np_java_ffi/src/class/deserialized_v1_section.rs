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
use np_ffi_core::{deserialize::v1::DeserializedV1IdentityKind, v1::V1VerificationMode};
use pourover::desc::{ClassDesc, StaticFieldDesc};
use std::sync::RwLock;

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

static VERIFICATION_MODE_CLASS: ClassDesc = ClassDesc::new(
    "com/google/android/nearby/presence/rust/DeserializedV1Section$VerificationMode",
);

/// Rust representation of `@VerificationMode`. These are `jints` on the Java side, so this type can't
/// be instantiated.
pub enum VerificationMode {}

impl VerificationMode {
    /// Convert a Rust verification mode enum to the Java `jint` representation.
    pub fn value_for<'local>(
        env: &mut JNIEnv<'local>,
        mode: V1VerificationMode,
    ) -> jni::errors::Result<jint> {
        match mode {
            V1VerificationMode::Mic => Self::mic(env),
            V1VerificationMode::Signature => Self::signature(env),
        }
    }

    /// Fetch the `SIGNATURE` constant
    pub fn signature<'local>(env: &mut JNIEnv<'local>) -> jni::errors::Result<jint> {
        static SIGNATURE: StaticFieldDesc = VERIFICATION_MODE_CLASS.static_field("SIGNATURE", "I");
        static VALUE: RwLock<Option<jint>> = RwLock::new(None);
        Self::lookup_static_value(env, &SIGNATURE, &VALUE)
    }

    /// Fetch the `MIC` constant
    pub fn mic<'local>(env: &mut JNIEnv<'local>) -> jni::errors::Result<jint> {
        static MIC: StaticFieldDesc = VERIFICATION_MODE_CLASS.static_field("MIC", "I");
        static VALUE: RwLock<Option<jint>> = RwLock::new(None);
        Self::lookup_static_value(env, &MIC, &VALUE)
    }

    /// Look up the given field and cache it in the given cache. The lookup will only be performed
    /// once if successful. This uses `RwLock` instead of `OnceCell` since the fallible `OnceCell`
    /// APIs are nightly only.
    fn lookup_static_value<'local>(
        env: &mut JNIEnv<'local>,
        field: &StaticFieldDesc,
        cache: &RwLock<Option<jint>>,
    ) -> jni::errors::Result<jint> {
        use jni::signature::{JavaType, Primitive};

        // Read from cache
        if let Some(value) = *cache.read().unwrap_or_else(|poison| poison.into_inner()) {
            return Ok(value);
        }

        // Get exclusive access to the cache for the lookup
        let mut guard = cache.write().unwrap_or_else(|poison| poison.into_inner());

        // In case of races, only lookup the value once
        if let Some(value) = *guard {
            return Ok(value);
        }

        let value = env
            .get_static_field_unchecked(field.cls(), field, JavaType::Primitive(Primitive::Int))
            .and_then(|ret| ret.i())?;

        *guard = Some(value);

        Ok(value)
    }
}
