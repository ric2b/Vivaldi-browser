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

use crate::class::v0_data_element::{TxPower, V0Actions};
use handle_map::{Handle, HandleLike};
use jni::{
    objects::{JClass, JObject},
    sys::{jint, jlong},
    JNIEnv,
};
use np_ffi_core::deserialize::{
    v0::{DeserializedV0IdentityDetails, DeserializedV0IdentityKind, V0Payload},
    DecryptMetadataResult,
};
use np_ffi_core::v0::V0Actions as CoreV0Actions;
use pourover::{desc::ClassDesc, jni_method};

static IDENTITY_DETAILS_CLASS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/V0Payload$IdentityDetails");

#[repr(transparent)]
pub struct IdentityDetails<Obj>(pub Obj);

impl<'local> IdentityDetails<JObject<'local>> {
    pub fn construct(
        env: &mut JNIEnv<'local>,
        details: DeserializedV0IdentityDetails,
    ) -> jni::errors::Result<Self> {
        Self::construct_from_parts(env, details.cred_id(), details.identity_token(), details.salt())
    }

    /// Create an IdentityDetails instance
    pub fn construct_from_parts(
        env: &mut JNIEnv<'local>,
        credential_id: i64,
        identity_token: [u8; 14],
        salt: [u8; 2],
    ) -> jni::errors::Result<Self> {
        let credential_id = credential_id as jint;
        let identity_token = env.byte_array_from_slice(&identity_token)?;
        let salt = env.byte_array_from_slice(&salt)?;

        pourover::call_constructor!(
            env,
            &IDENTITY_DETAILS_CLASS,
            "(I[B[B)V",
            credential_id,
            identity_token,
            salt
        )
        .map(Self)
    }
}

#[jni_method(package = "com.google.android.nearby.presence.rust", class = "V0Payload")]
extern "system" fn nativeGetDataElement<'local>(
    mut env: JNIEnv<'local>,
    _cls: JClass<'local>,
    handle_id: jlong,
    index: jint,
) -> JObject<'local> {
    let v0_payload = V0Payload::from_handle(Handle::from_id(handle_id as u64));
    let Ok(index) = u8::try_from(index) else {
        return JObject::null();
    };

    use np_ffi_core::{
        deserialize::v0::GetV0DEResult::{Error, Success},
        v0::V0DataElement::{Actions, TxPower as TxPow},
    };
    let ret = match v0_payload.get_de(index) {
        Success(TxPow(tx_power)) => {
            TxPower::construct(&mut env, jint::from(tx_power.as_i8())).map(|obj| obj.0)
        }
        Success(Actions(actions)) => {
            let identity_kind = match &actions {
                CoreV0Actions::Plaintext(_) => DeserializedV0IdentityKind::Plaintext,
                CoreV0Actions::Encrypted(_) => DeserializedV0IdentityKind::Decrypted,
            };

            V0Actions::construct(&mut env, identity_kind, actions.as_u32() as jint).map(|obj| obj.0)
        }
        Error => {
            return JObject::null();
        }
    };

    match ret {
        Ok(de) => de,
        Err(_jni_err) => JObject::null(),
    }
}

#[jni_method(package = "com.google.android.nearby.presence.rust", class = "V0Payload")]
extern "system" fn nativeGetIdentityDetails<'local>(
    mut env: JNIEnv<'local>,
    _cls: JClass<'local>,
    handle_id: jlong,
) -> JObject<'local> {
    let v0_payload = V0Payload::from_handle(Handle::from_id(handle_id as u64));

    use np_ffi_core::deserialize::v0::GetV0IdentityDetailsResult::*;
    match v0_payload.get_identity_details() {
        Success(details) => IdentityDetails::construct(&mut env, details)
            .map_or_else(|_err| JObject::null(), |obj| obj.0),
        Error => JObject::null(),
    }
}

#[jni_method(package = "com.google.android.nearby.presence.rust", class = "V0Payload")]
extern "system" fn nativeGetDecryptedMetadata<'local>(
    env: JNIEnv<'local>,
    _cls: JClass<'local>,
    handle_id: jlong,
) -> JObject<'local> {
    let v0_payload = V0Payload::from_handle(Handle::from_id(handle_id as u64));

    let DecryptMetadataResult::Success(metadata) = v0_payload.decrypt_metadata() else {
        return JObject::null();
    };

    let Ok(buffer) = metadata.take_buffer() else {
        return JObject::null();
    };
    let Ok(decrypted_metadata_array) = env.byte_array_from_slice(&buffer[..]) else {
        return JObject::null();
    };

    decrypted_metadata_array.into()
}

#[jni_method(package = "com.google.android.nearby.presence.rust", class = "V0Payload")]
extern "system" fn deallocate<'local>(
    _env: JNIEnv<'local>,
    _cls: JClass<'local>,
    handle_id: jlong,
) {
    // Swallow errors here since there's nothing meaningful to do.
    let _ = V0Payload::from_handle(Handle::from_id(handle_id as u64)).deallocate();
}
