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
    objects::{JByteArray, JObject},
    signature::ReturnType,
    JNIEnv,
};

use np_ffi_core::credentials::V0DiscoveryCredential as CoreV0DiscoveryCredential;
use pourover::desc::{ClassDesc, FieldDesc};

static V0_DISCOVERY_CREDENTIAL_CLS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/credential/V0DiscoveryCredential");

/// Rust representation of `class V0DiscoveryCredential`.
#[repr(transparent)]
pub struct V0DiscoveryCredential<Obj>(pub Obj);

impl<'local, Obj: AsRef<JObject<'local>>> V0DiscoveryCredential<Obj> {
    /// Get an array field as a Rust array.
    fn get_array<'env>(
        &self,
        env: &mut JNIEnv<'env>,
        field: &FieldDesc,
    ) -> jni::errors::Result<[u8; 32]> {
        let arr: JByteArray<'env> =
            env.get_field_unchecked(self.0.as_ref(), field, ReturnType::Array)?.l()?.into();

        let mut buf = [0; 32];
        env.get_byte_array_region(arr, 0, &mut buf[..])?;
        Ok(buf.map(|byte| byte as u8))
    }

    /// Get the key seed.
    pub fn get_key_seed<'env>(&self, env: &mut JNIEnv<'env>) -> jni::errors::Result<[u8; 32]> {
        static KEY_SEED: FieldDesc = V0_DISCOVERY_CREDENTIAL_CLS.field("keySeed", "[B");
        self.get_array(env, &KEY_SEED)
    }

    /// Get the identity token hmac.
    pub fn get_identity_token_hmac<'env>(
        &self,
        env: &mut JNIEnv<'env>,
    ) -> jni::errors::Result<[u8; 32]> {
        static IDENTITY_TOKEN_HMAC: FieldDesc =
            V0_DISCOVERY_CREDENTIAL_CLS.field("identityTokenHmac", "[B");
        self.get_array(env, &IDENTITY_TOKEN_HMAC)
    }

    /// Convert this to the `np_ffi_core` representation.
    pub fn get_as_core<'env>(
        &self,
        env: &mut JNIEnv<'env>,
    ) -> jni::errors::Result<CoreV0DiscoveryCredential> {
        Ok(CoreV0DiscoveryCredential::new(
            self.get_key_seed(env)?,
            self.get_identity_token_hmac(env)?,
        ))
    }
}
