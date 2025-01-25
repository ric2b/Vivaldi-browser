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

use np_ffi_core::credentials::V1DiscoveryCredential as CoreV1DiscoveryCredential;
use pourover::desc::{ClassDesc, FieldDesc};

static V1_DISCOVERY_CREDENTIAL_CLS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/credential/V1DiscoveryCredential");

/// Rust representation of `class V1DiscoveryCredential`.
#[repr(transparent)]
pub struct V1DiscoveryCredential<Obj>(pub Obj);

impl<'local, Obj: AsRef<JObject<'local>>> V1DiscoveryCredential<Obj> {
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
        static KEY_SEED: FieldDesc = V1_DISCOVERY_CREDENTIAL_CLS.field("keySeed", "[B");
        self.get_array(env, &KEY_SEED)
    }

    /// Get the expected mic short salt identity token hmac.
    pub fn get_expected_mic_short_salt_identity_token_hmac<'env>(
        &self,
        env: &mut JNIEnv<'env>,
    ) -> jni::errors::Result<[u8; 32]> {
        static EXPECTED_MIC_SHORT_SALT_IDENTITY_TOKEN_HMAC: FieldDesc =
            V1_DISCOVERY_CREDENTIAL_CLS.field("expectedMicShortSaltIdentityTokenHmac", "[B");
        self.get_array(env, &EXPECTED_MIC_SHORT_SALT_IDENTITY_TOKEN_HMAC)
    }

    /// Get the expected mic extended salt identity token hmac.
    pub fn get_expected_mic_extended_salt_identity_token_hmac<'env>(
        &self,
        env: &mut JNIEnv<'env>,
    ) -> jni::errors::Result<[u8; 32]> {
        static EXPECTED_MIC_EXTENDED_SALT_IDENTITY_TOKEN_HMAC: FieldDesc =
            V1_DISCOVERY_CREDENTIAL_CLS.field("expectedMicExtendedSaltIdentityTokenHmac", "[B");
        self.get_array(env, &EXPECTED_MIC_EXTENDED_SALT_IDENTITY_TOKEN_HMAC)
    }

    /// Get the expected signature identity token hmac.
    pub fn get_expected_signature_identity_token_hmac<'env>(
        &self,
        env: &mut JNIEnv<'env>,
    ) -> jni::errors::Result<[u8; 32]> {
        static EXPECTED_SIGNATURE_IDENTITY_TOKEN_HMAC: FieldDesc =
            V1_DISCOVERY_CREDENTIAL_CLS.field("expectedSignatureIdentityTokenHmac", "[B");
        self.get_array(env, &EXPECTED_SIGNATURE_IDENTITY_TOKEN_HMAC)
    }

    /// Get the pub key.
    pub fn get_pub_key<'env>(&self, env: &mut JNIEnv<'env>) -> jni::errors::Result<[u8; 32]> {
        static PUB_KEY: FieldDesc = V1_DISCOVERY_CREDENTIAL_CLS.field("pubKey", "[B");
        self.get_array(env, &PUB_KEY)
    }

    /// Convert this to the `np_ffi_core` representation.
    pub fn get_as_core<'env>(
        &self,
        env: &mut JNIEnv<'env>,
    ) -> jni::errors::Result<CoreV1DiscoveryCredential> {
        Ok(CoreV1DiscoveryCredential::new(
            self.get_key_seed(env)?,
            self.get_expected_mic_short_salt_identity_token_hmac(env)?,
            self.get_expected_mic_extended_salt_identity_token_hmac(env)?,
            self.get_expected_signature_identity_token_hmac(env)?,
            self.get_pub_key(env)?,
        ))
    }
}
