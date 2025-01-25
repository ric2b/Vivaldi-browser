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

use handle_map::{Handle, HandleLike};
use jni::{
    objects::{JByteArray, JClass, JObject},
    sys::jlong,
    JNIEnv,
};
use np_ffi_core::{
    serialize::v0::{
        create_v0_encrypted_advertisement_builder, create_v0_public_advertisement_builder,
        AddV0DEResult, CreateV0AdvertisementBuilderResult, SerializeV0AdvertisementResult,
        V0AdvertisementBuilder,
    },
    v0::{BuildTxPowerResult, V0DataElement},
};
use pourover::{desc::ClassDesc, jni_method};

use crate::class::{
    v0_data_element::{TxPower, V0Actions},
    InsufficientSpaceException, InvalidDataElementException, InvalidHandleException,
    LdtEncryptionException, NoSpaceLeftException, UnencryptedSizeException, V0BroadcastCredential,
};

static V0_BUILDER_HANDLE_CLASS: ClassDesc = ClassDesc::new(
    "com/google/android/nearby/presence/rust/V0AdvertisementBuilder$V0BuilderHandle",
);

#[repr(transparent)]
pub struct V0BuilderHandle<Obj>(pub Obj);

impl<'local, Obj: AsRef<JObject<'local>>> V0BuilderHandle<Obj> {
    /// Get a reference to the inner `jni` crate [`JObject`].
    pub fn as_obj(&self) -> &JObject<'local> {
        self.0.as_ref()
    }

    /// Get the Rust [`HandleLike`] representation from this Java object.
    pub fn as_rust_handle<'env>(
        &self,
        env: &mut JNIEnv<'env>,
    ) -> jni::errors::Result<V0AdvertisementBuilder> {
        let handle_id = self.get_handle_id(env)?;
        Ok(V0AdvertisementBuilder::from_handle(Handle::from_id(handle_id as u64)))
    }

    /// Get `long handleId` from the Java object
    fn get_handle_id<'env_local>(
        &self,
        env: &mut JNIEnv<'env_local>,
    ) -> jni::errors::Result<jlong> {
        use V0_BUILDER_HANDLE_CLASS as CLS;
        pourover::call_method!(env, &CLS, "getId", "()J", self.as_obj())
    }

    fn add_de<'env>(
        &self,
        env: &mut JNIEnv<'env>,
        data_element: V0DataElement,
    ) -> jni::errors::Result<()> {
        let builder = self.as_rust_handle(env)?;
        let Ok(res) = builder.add_de(data_element) else {
            let _ = env.throw_new(
                "java/lang/IllegalStateException",
                "V0Actions is not validly constructed",
            );
            return Err(jni::errors::Error::JavaException);
        };

        match res {
            AddV0DEResult::Success => {}
            AddV0DEResult::InvalidAdvertisementBuilderHandle => {
                InvalidHandleException::throw_new(env)?;
            }
            AddV0DEResult::InsufficientAdvertisementSpace => {
                InsufficientSpaceException::throw_new(env)?;
            }
            AddV0DEResult::InvalidIdentityTypeForDataElement => {
                let _ = env
                    .new_string("Mismatched identity kind for V0Actions data element")
                    .map(|string| env.auto_local(string))
                    .and_then(|string| InvalidDataElementException::throw_new(env, &string));
            }
        }

        Ok(())
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V0AdvertisementBuilder.V0BuilderHandle"
)]
extern "system" fn allocatePublic<'local>(mut env: JNIEnv<'local>, _cls: JClass<'local>) -> jlong {
    match create_v0_public_advertisement_builder() {
        CreateV0AdvertisementBuilderResult::Success(builder) => {
            builder.get_as_handle().get_id() as jlong
        }
        CreateV0AdvertisementBuilderResult::NoSpaceLeft => {
            let _ = NoSpaceLeftException::throw_new(&mut env);
            0
        }
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V0AdvertisementBuilder.V0BuilderHandle"
)]
extern "system" fn allocatePrivate<'local>(
    mut env: JNIEnv<'local>,
    _cls: JClass<'local>,
    credential: V0BroadcastCredential<JObject<'local>>,
    salt_arr: JByteArray<'local>,
) -> jlong {
    let Ok(credential) = credential.get_as_core(&mut env) else {
        return 0;
    };

    let mut salt = [0; 2];
    if let Err(_jni_err) = env.get_byte_array_region(salt_arr, 0, &mut salt[..]) {
        return 0;
    }
    let salt = salt.map(|byte| byte as u8).into();

    match create_v0_encrypted_advertisement_builder(credential, salt) {
        CreateV0AdvertisementBuilderResult::Success(builder) => {
            builder.get_as_handle().get_id() as jlong
        }
        CreateV0AdvertisementBuilderResult::NoSpaceLeft => {
            let _ = NoSpaceLeftException::throw_new(&mut env);
            0
        }
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V0AdvertisementBuilder.V0BuilderHandle",
    method_name = "nativeAddTxPowerDataElement"
)]
extern "system" fn add_tx_power_de<'local>(
    mut env: JNIEnv<'local>,
    this: V0BuilderHandle<JObject<'local>>,
    tx_power: TxPower<JObject<'local>>,
) {
    let tx_power = match tx_power.get_as_core(&mut env) {
        Ok(BuildTxPowerResult::Success(tx_power)) => tx_power,
        Ok(BuildTxPowerResult::OutOfRange) => {
            let _ = env
                .new_string("TX Power value out of range")
                .map(|string| env.auto_local(string))
                .and_then(|string| InvalidDataElementException::throw_new(&mut env, &string));
            return;
        }
        Err(_jni_err) => {
            // `crate jni` should have already thrown
            return;
        }
    };
    let Ok(()) = this.add_de(&mut env, V0DataElement::TxPower(tx_power)) else {
        // `crate jni` should have already thrown
        return;
    };
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V0AdvertisementBuilder.V0BuilderHandle",
    method_name = "nativeAddV0ActionsDataElement"
)]
extern "system" fn add_v0_actions_de<'local>(
    mut env: JNIEnv<'local>,
    this: V0BuilderHandle<JObject<'local>>,
    v0_actions: V0Actions<JObject<'local>>,
) {
    let v0_actions = match v0_actions.get_as_core(&mut env) {
        Ok(Some(v0_actions)) => v0_actions,
        Ok(None) => {
            let _ = env
                .new_string("V0Actions is not valid")
                .map(|string| env.auto_local(string))
                .and_then(|string| InvalidDataElementException::throw_new(&mut env, &string));
            return;
        }
        Err(_jni_err) => {
            // `crate jni` should have already thrown
            return;
        }
    };
    let Ok(()) = this.add_de(&mut env, V0DataElement::Actions(v0_actions)) else {
        // `crate jni` should have already thrown
        return;
    };
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V0AdvertisementBuilder.V0BuilderHandle"
)]
extern "system" fn nativeBuild<'local>(
    mut env: JNIEnv<'local>,
    this: V0BuilderHandle<JObject<'local>>,
) -> JObject<'local> {
    let Ok(builder) = this.as_rust_handle(&mut env) else {
        return JObject::null();
    };

    match builder.into_advertisement() {
        SerializeV0AdvertisementResult::Success(bytes) => {
            #[allow(clippy::expect_used)]
            let adv_bytes = bytes.as_slice().expect("should never be malformed from core");
            env.byte_array_from_slice(adv_bytes).map_or(JObject::null(), JObject::from)
        }
        SerializeV0AdvertisementResult::InvalidAdvertisementBuilderHandle => {
            let _ = InvalidHandleException::throw_new(&mut env);
            JObject::null()
        }
        SerializeV0AdvertisementResult::LdtError => {
            let _ = LdtEncryptionException::throw_new(&mut env);
            JObject::null()
        }
        SerializeV0AdvertisementResult::UnencryptedError => {
            let _ = UnencryptedSizeException::throw_new(&mut env);
            JObject::null()
        }
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V0AdvertisementBuilder.V0BuilderHandle"
)]
extern "system" fn deallocate<'local>(
    _env: JNIEnv<'local>,
    _cls: JClass<'local>,
    handle_id: jlong,
) {
    let handle = V0AdvertisementBuilder::from_handle(Handle::from_id(handle_id as u64));
    let _ = handle.deallocate();
}
