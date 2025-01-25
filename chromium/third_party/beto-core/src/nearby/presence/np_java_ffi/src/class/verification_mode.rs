// Copyright 2024 Google LLC
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

use jni::{sys::jint, JNIEnv};
use np_ffi_core::v1::V1VerificationMode;
use pourover::desc::{ClassDesc, StaticFieldDesc};
use std::sync::RwLock;

static VERIFICATION_MODE_CLASS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/VerificationMode");

/// Rust representation of `@VerificationMode`.
#[derive(Copy, Clone, PartialEq, Eq)]
pub enum VerificationMode {
    /// Verification is done using the Mic scheme.
    Mic,
    /// Verification is done using the Signature scheme.
    Signature,
}

impl VerificationMode {
    /// Convert a Java `int` to the Rust version. Will return `None` if the given `value` is not
    /// valid.
    pub fn from_java<'local>(
        env: &mut JNIEnv<'local>,
        value: jint,
    ) -> jni::errors::Result<Option<Self>> {
        if value == Self::mic(env)? {
            Ok(Some(Self::Mic))
        } else if value == Self::signature(env)? {
            Ok(Some(Self::Signature))
        } else {
            Ok(None)
        }
    }

    /// Convert to a Java `int` value.
    pub fn to_java<'local>(&self, env: &mut JNIEnv<'local>) -> jni::errors::Result<jint> {
        match self {
            Self::Mic => Self::mic(env),
            Self::Signature => Self::signature(env),
        }
    }

    /// Fetch the `SIGNATURE` constant
    fn signature<'local>(env: &mut JNIEnv<'local>) -> jni::errors::Result<jint> {
        static SIGNATURE: StaticFieldDesc = VERIFICATION_MODE_CLASS.static_field("SIGNATURE", "I");
        static VALUE: RwLock<Option<jint>> = RwLock::new(None);
        Self::lookup_static_value(env, &SIGNATURE, &VALUE)
    }

    /// Fetch the `MIC` constant
    fn mic<'local>(env: &mut JNIEnv<'local>) -> jni::errors::Result<jint> {
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

impl From<V1VerificationMode> for VerificationMode {
    fn from(mode: V1VerificationMode) -> Self {
        match mode {
            V1VerificationMode::Mic => Self::Mic,
            V1VerificationMode::Signature => Self::Signature,
        }
    }
}

impl From<VerificationMode> for V1VerificationMode {
    fn from(mode: VerificationMode) -> Self {
        match mode {
            VerificationMode::Mic => Self::Mic,
            VerificationMode::Signature => Self::Signature,
        }
    }
}
