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

use jni::{
    objects::{JByteArray, JClass, JObject},
    JNIEnv,
};
use np_adv::extended::salt::{ExtendedV1Salt, MultiSalt, ShortV1Salt};
use np_ffi_core::serialize::v1::CreateV1SectionBuilderResult;
use pourover::jni_method;

use crate::class::{
    InsufficientSpaceException, InvalidHandleException, InvalidSectionKindException,
    UnclosedActiveSectionException, V1BroadcastCredential, V1BuilderHandle, V1SectionBuilder,
};

#[jni_method(package = "com.google.android.nearby.presence.rust", class = "TestVectors")]
extern "system" fn nativeAddSaltedSection<'local>(
    mut env: JNIEnv<'local>,
    _cls: JClass<'local>,
    j_handle: V1BuilderHandle<JObject<'local>>,
    credential: V1BroadcastCredential<JObject<'local>>,
    salt: JByteArray<'local>,
) -> JObject<'local> {
    let Ok(handle) = j_handle.as_rust_handle(&mut env) else {
        return JObject::null();
    };

    let Ok(credential) = credential.get_as_core(&mut env) else {
        return JObject::null();
    };

    let salt = {
        match env.get_array_length(&salt) {
            Ok(2) => {
                let mut buf = [0; 2];
                let Ok(_) = env.get_byte_array_region(&salt, 0, &mut buf[..]) else {
                    return JObject::null();
                };
                let buf = buf.map(|b| b as u8);
                MultiSalt::Short(ShortV1Salt::from(buf))
            }
            Ok(16) => {
                let mut buf = [0; 16];
                let Ok(_) = env.get_byte_array_region(&salt, 0, &mut buf[..]) else {
                    return JObject::null();
                };
                let buf = buf.map(|b| b as u8);
                MultiSalt::Extended(ExtendedV1Salt::from(buf))
            }
            Ok(_) => {
                let _ = env.throw_new("java/lang/RuntimeException", "Invalid salt length");
                return JObject::null();
            }
            Err(_jni_err) => return JObject::null(),
        }
    };

    let builder = match handle.mic_custom_salt_section_builder(credential, salt) {
        CreateV1SectionBuilderResult::Success(builder) => builder,
        CreateV1SectionBuilderResult::UnclosedActiveSection => {
            let _ = UnclosedActiveSectionException::throw_new(&mut env);
            return JObject::null();
        }
        CreateV1SectionBuilderResult::InvalidAdvertisementBuilderHandle => {
            let _ = InvalidHandleException::throw_new(&mut env);
            return JObject::null();
        }
        CreateV1SectionBuilderResult::IdentityKindMismatch => {
            let _ = InvalidSectionKindException::throw_new(&mut env);
            return JObject::null();
        }
        CreateV1SectionBuilderResult::NoSpaceLeft => {
            let _ = InsufficientSpaceException::throw_new(&mut env);
            return JObject::null();
        }
    };

    assert_eq!(
        handle.get_as_handle().get_id(),
        builder.adv_builder.get_as_handle().get_id(),
        "Section builder must be the same handle so that we can reuse the Java handle object below"
    );

    let Ok(java_builder) =
        V1SectionBuilder::construct(&mut env, j_handle, builder.section_index.into())
    else {
        return JObject::null();
    };

    java_builder.0
}
