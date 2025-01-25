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
    sys::{jint, jlong},
    JNIEnv,
};
use np_ffi_core::deserialize::v0::{DeserializedV0IdentityKind, V0Payload};
use pourover::desc::ClassDesc;

use crate::class::{CredentialBook, IdentityKind};

static DESERIALIZED_V0_ADVERTISEMENT_CLASS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/DeserializedV0Advertisement");

/// Rust representation of `class DeserializedV0Advertisement`.
#[repr(transparent)]
pub struct DeserializedV0Advertisement<Obj>(pub Obj);

impl<'local> DeserializedV0Advertisement<JObject<'local>> {
    /// Create an illegible advertisment with the given error.
    pub fn construct_from_error(
        env: &mut JNIEnv<'local>,
        error: V0AdvertisementError,
    ) -> jni::errors::Result<Self> {
        let error = IdentityKind::from(error).to_java(env)?;

        pourover::call_constructor!(env, &DESERIALIZED_V0_ADVERTISEMENT_CLASS, "(I)V", error)
            .map(Self)
    }

    /// Create a legible advertisment.
    pub fn construct<'book>(
        env: &mut JNIEnv<'local>,
        num_des: u8,
        v0_payload: V0Payload,
        identity: DeserializedV0IdentityKind,
        credential_book: CredentialBook<impl AsRef<JObject<'book>>>,
    ) -> jni::errors::Result<Self> {
        let num_des = jint::from(num_des);
        let payload_handle = v0_payload.get_as_handle().get_id() as jlong;
        let identity = IdentityKind::from(identity).to_java(env)?;

        pourover::call_constructor!(
            env,
            &DESERIALIZED_V0_ADVERTISEMENT_CLASS,
            "(IJILcom/google/android/nearby/presence/rust/credential/CredentialBook;)V",
            num_des,
            payload_handle,
            identity,
            credential_book.as_obj()
        )
        .map(Self)
    }
}

impl<'local, Obj: AsRef<JObject<'local>>> DeserializedV0Advertisement<Obj> {
    /// Get a reference to the inner `jni` crate [`JObject`].
    pub fn as_obj(&self) -> &JObject<'local> {
        self.0.as_ref()
    }
}

/// A reason for an advertisment being illegible
#[derive(Copy, Clone, Eq, PartialEq)]
pub enum V0AdvertisementError {
    /// There is no matching credential in the credential book.
    NoMatchingCredentials,
}
