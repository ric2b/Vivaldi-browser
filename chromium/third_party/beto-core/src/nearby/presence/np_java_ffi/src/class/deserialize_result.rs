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
    objects::JObject,
    signature::{JavaType, Primitive},
    sys::jint,
    JNIEnv,
};
use pourover::desc::{ClassDesc, StaticFieldDesc};

use crate::class::{DeserializedV0Advertisement, DeserializedV1Advertisement};

static DESERIALIZE_RESULT_CLASS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/DeserializeResult");

/// Rust representation of `class DeserializeResult`.
#[repr(transparent)]
pub struct DeserializeResult<Obj>(pub Obj);

impl<'local> DeserializeResult<JObject<'local>> {
    /// Create a result representing the given error.
    pub fn construct_from_error(
        env: &mut JNIEnv<'local>,
        error: DeserializeResultError,
    ) -> jni::errors::Result<Self> {
        let error = error.lookup_java_value(env)?;
        pourover::call_constructor!(env, &DESERIALIZE_RESULT_CLASS, "(I)V", error).map(Self)
    }

    /// Create a result containing the given advertisement.
    pub fn from_v0_advertisement(
        env: &mut JNIEnv<'local>,
        adv: DeserializedV0Advertisement<impl AsRef<JObject<'local>>>,
    ) -> jni::errors::Result<Self> {
        pourover::call_constructor!(
            env,
            &DESERIALIZE_RESULT_CLASS,
            "(Lcom/google/android/nearby/presence/rust/DeserializedV0Advertisement;)V",
            adv.as_obj()
        )
        .map(Self)
    }

    /// Create a result containing the given advertisement.
    pub fn from_v1_advertisement(
        env: &mut JNIEnv<'local>,
        adv: DeserializedV1Advertisement<impl AsRef<JObject<'local>>>,
    ) -> jni::errors::Result<Self> {
        pourover::call_constructor!(
            env,
            &DESERIALIZE_RESULT_CLASS,
            "(Lcom/google/android/nearby/presence/rust/DeserializedV1Advertisement;)V",
            adv.as_obj()
        )
        .map(Self)
    }
}

impl<'local, Obj: AsRef<JObject<'local>>> DeserializeResult<Obj> {
    /// Get a reference to the inner `jni` crate [`JObject`].
    pub fn as_obj(&self) -> &JObject<'local> {
        self.0.as_ref()
    }
}

static DESERIALIZE_RESULT_KIND_CLASS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/DeserializeResult$Kind");

/// An error that occurs during deserialization
#[derive(Copy, Clone, Eq, PartialEq)]
pub enum DeserializeResultError {
    /// An unspecified error has occurred.
    UnknownError,
}

impl DeserializeResultError {
    /// Fetch the Java `@IntDef` value for this error.
    fn lookup_java_value(&self, env: &mut JNIEnv<'_>) -> jni::errors::Result<jint> {
        static UNKNOWN_ERROR_STATIC_FIELD: StaticFieldDesc =
            DESERIALIZE_RESULT_KIND_CLASS.static_field("UNKNOWN_ERROR", "I");
        match self {
            DeserializeResultError::UnknownError => env
                .get_static_field_unchecked(
                    UNKNOWN_ERROR_STATIC_FIELD.cls(),
                    &UNKNOWN_ERROR_STATIC_FIELD,
                    JavaType::Primitive(Primitive::Int),
                )
                .and_then(|ret| ret.i()),
        }
    }
}
