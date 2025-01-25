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
    objects::{JByteArray, JClass, JObject, JThrowable},
    sys::{jboolean, jint, jlong, JNI_FALSE, JNI_TRUE},
    JNIEnv,
};

use crate::class::{
    InvalidHandleException, NoSpaceLeftException, V0DiscoveryCredential, V1DiscoveryCredential,
};
use handle_map::Handle;
use np_ffi_core::credentials::{
    create_credential_slab, deallocate_credential_slab, AddV0CredentialToSlabResult,
    AddV1CredentialToSlabResult, CreateCredentialSlabResult, CredentialSlab, MatchedCredential,
};
use pourover::{desc::ClassDesc, jni_method};

static INVALID_KEY_EXCEPTION_CLASS: ClassDesc = ClassDesc::new(
    "com/google/android/nearby/presence/rust/credential/CredentialBook$InvalidPublicKeyException",
);

/// Rust representation of `class InvalidPublicKeyException`.
#[repr(transparent)]
pub struct InvalidPublicKeyException<Obj>(pub Obj);

impl<'local> InvalidPublicKeyException<JObject<'local>> {
    /// Create a new instance.
    pub fn construct(env: &mut JNIEnv<'local>) -> jni::errors::Result<Self> {
        pourover::call_constructor!(env, &INVALID_KEY_EXCEPTION_CLASS, "()V").map(Self)
    }

    /// Create a new instance and throw it.
    pub fn throw_new(env: &mut JNIEnv<'local>) -> jni::errors::Result<()> {
        Self::construct(env)?.throw(env)
    }
}

impl<'local, Obj: AsRef<JObject<'local>>> InvalidPublicKeyException<Obj> {
    /// Throw this exception.
    pub fn throw<'env>(&self, env: &mut JNIEnv<'env>) -> jni::errors::Result<()> {
        env.throw(<&JThrowable>::from(self.0.as_ref()))
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust.credential",
    class = "CredentialSlab",
    method_name = "allocate"
)]
extern "system" fn allocate_slab<'local>(mut env: JNIEnv<'local>, _cls: JClass<'local>) -> jlong {
    let CreateCredentialSlabResult::Success(slab) = create_credential_slab() else {
        let _ = NoSpaceLeftException::throw_new(&mut env);
        return 0;
    };

    slab.get_as_handle().get_id() as jlong
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust.credential",
    class = "CredentialSlab"
)]
extern "system" fn nativeAddV0DiscoveryCredential<'local>(
    mut env: JNIEnv<'local>,
    _cls: JClass<'local>,
    handle_id: jlong,
    credential: V0DiscoveryCredential<JObject<'local>>,
    cred_id: jint,
    encrypted_metadata_bytes: JByteArray<'local>,
) -> jboolean {
    let mut add_cred = move || {
        let slab = CredentialSlab::from_handle(Handle::from_id(handle_id as u64));

        let core_cred = credential.get_as_core(&mut env)?;
        let match_data = MatchedCredential::from_arc_bytes(
            cred_id as i64,
            env.convert_byte_array(&encrypted_metadata_bytes)?.into(),
        );

        Ok::<_, jni::errors::Error>(match slab.add_v0(core_cred, match_data) {
            AddV0CredentialToSlabResult::Success => JNI_TRUE,
            AddV0CredentialToSlabResult::InvalidHandle => {
                InvalidHandleException::throw_new(&mut env)?;
                JNI_FALSE
            }
        })
    };

    match add_cred() {
        Ok(ret) => ret,
        Err(_) => JNI_FALSE,
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust.credential",
    class = "CredentialSlab"
)]
extern "system" fn nativeAddV1DiscoveryCredential<'local>(
    mut env: JNIEnv<'local>,
    _cls: JClass<'local>,
    handle_id: jlong,
    credential: V1DiscoveryCredential<JObject<'local>>,
    cred_id: jint,
    encrypted_metadata_bytes: JByteArray<'local>,
) -> jboolean {
    let mut add_cred = move || {
        let slab = CredentialSlab::from_handle(Handle::from_id(handle_id as u64));

        let core_cred = credential.get_as_core(&mut env)?;
        let match_data = MatchedCredential::from_arc_bytes(
            cred_id as i64,
            env.convert_byte_array(&encrypted_metadata_bytes)?.into(),
        );

        Ok::<_, jni::errors::Error>(match slab.add_v1(core_cred, match_data) {
            AddV1CredentialToSlabResult::Success => JNI_TRUE,
            AddV1CredentialToSlabResult::InvalidHandle => {
                InvalidHandleException::throw_new(&mut env)?;
                JNI_FALSE
            }
            AddV1CredentialToSlabResult::InvalidPublicKeyBytes => {
                InvalidPublicKeyException::throw_new(&mut env)?;
                JNI_FALSE
            }
        })
    };

    match add_cred() {
        Ok(ret) => ret,
        Err(_) => JNI_FALSE,
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust.credential",
    class = "CredentialSlab",
    method_name = "deallocate"
)]
extern "system" fn deallocate_slab<'local>(
    _env: JNIEnv<'local>,
    _cls: JClass<'local>,
    handle_id: jlong,
) {
    let slab = CredentialSlab::from_handle(Handle::from_id(handle_id as u64));
    let _ = deallocate_credential_slab(slab);
}
