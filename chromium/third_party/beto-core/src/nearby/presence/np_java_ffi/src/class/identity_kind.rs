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

use jni::{
    signature::{JavaType, Primitive},
    sys::jint,
    JNIEnv,
};
use np_ffi_core::deserialize::{v0::DeserializedV0IdentityKind, v1::DeserializedV1IdentityKind};
use pourover::desc::{ClassDesc, StaticFieldDesc};
use std::sync::RwLock;

use crate::class::V0AdvertisementError;

static IDENTITY_KIND_CLASS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/IdentityKind");

/// Rust representation of `@interface IdentityKind`.
#[derive(Copy, Clone, PartialEq, Eq)]
pub enum IdentityKind {
    /// An illegible identity
    NoMatchingCredentials,
    /// A public identity
    Plaintext,
    /// An encrypted identity
    Decrypted,
}

impl IdentityKind {
    /// Convert a Java `int` to the Rust version. Will return `None` if the given `value` is not
    /// valid.
    pub fn from_java<'local>(
        env: &mut JNIEnv<'local>,
        value: jint,
    ) -> jni::errors::Result<Option<Self>> {
        if value == Self::no_matching_credentials(env)? {
            Ok(Some(Self::NoMatchingCredentials))
        } else if value == Self::plaintext(env)? {
            Ok(Some(Self::Plaintext))
        } else if value == Self::decrypted(env)? {
            Ok(Some(Self::Decrypted))
        } else {
            Ok(None)
        }
    }

    /// Convert to a Java `int` value.
    pub fn to_java<'local>(&self, env: &mut JNIEnv<'local>) -> jni::errors::Result<jint> {
        match self {
            Self::NoMatchingCredentials => Self::no_matching_credentials(env),
            Self::Plaintext => Self::plaintext(env),
            Self::Decrypted => Self::decrypted(env),
        }
    }

    /// Fetch the `NO_MATCHING_CREDENTIALS` constant
    fn no_matching_credentials<'local>(env: &mut JNIEnv<'local>) -> jni::errors::Result<jint> {
        static NO_MATCHING_CREDENTIALS: StaticFieldDesc =
            IDENTITY_KIND_CLASS.static_field("NO_MATCHING_CREDENTIALS", "I");
        static VALUE: RwLock<Option<jint>> = RwLock::new(None);
        Self::lookup_static_value(env, &NO_MATCHING_CREDENTIALS, &VALUE)
    }

    /// Fetch the `PLAINTEXT` constant
    fn plaintext<'local>(env: &mut JNIEnv<'local>) -> jni::errors::Result<jint> {
        static PLAINTEXT: StaticFieldDesc = IDENTITY_KIND_CLASS.static_field("PLAINTEXT", "I");
        static VALUE: RwLock<Option<jint>> = RwLock::new(None);
        Self::lookup_static_value(env, &PLAINTEXT, &VALUE)
    }

    /// Fetch the `DECRYPTED` constant
    fn decrypted<'local>(env: &mut JNIEnv<'local>) -> jni::errors::Result<jint> {
        static DECRYPTED: StaticFieldDesc = IDENTITY_KIND_CLASS.static_field("DECRYPTED", "I");
        static VALUE: RwLock<Option<jint>> = RwLock::new(None);
        Self::lookup_static_value(env, &DECRYPTED, &VALUE)
    }

    /// Look up the given field and cache it in the given cache. The lookup will only be performed
    /// once if successful. This uses `RwLock` instead of `OnceCell` since the fallible `OnceCell`
    /// APIs are nightly only.
    fn lookup_static_value<'local>(
        env: &mut JNIEnv<'local>,
        field: &StaticFieldDesc,
        cache: &RwLock<Option<jint>>,
    ) -> jni::errors::Result<jint> {
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

impl From<V0AdvertisementError> for IdentityKind {
    fn from(err: V0AdvertisementError) -> Self {
        match err {
            V0AdvertisementError::NoMatchingCredentials => Self::NoMatchingCredentials,
        }
    }
}

impl From<DeserializedV0IdentityKind> for IdentityKind {
    fn from(kind: DeserializedV0IdentityKind) -> Self {
        match kind {
            DeserializedV0IdentityKind::Plaintext => Self::Plaintext,
            DeserializedV0IdentityKind::Decrypted => Self::Decrypted,
        }
    }
}

impl From<DeserializedV1IdentityKind> for IdentityKind {
    fn from(kind: DeserializedV1IdentityKind) -> Self {
        match kind {
            DeserializedV1IdentityKind::Plaintext => Self::Plaintext,
            DeserializedV1IdentityKind::Decrypted => Self::Decrypted,
        }
    }
}
