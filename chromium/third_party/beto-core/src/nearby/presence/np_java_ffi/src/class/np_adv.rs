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
    objects::{JByteArray, JClass, JObject},
    sys::jint,
    JNIEnv,
};

use crate::class::{
    CredentialBook, DeserializeResult, DeserializeResultError, DeserializedV0Advertisement,
    DeserializedV1Advertisement, LegibleV1Sections, V0AdvertisementError,
};
use np_ffi_core::deserialize::deserialize_advertisement_from_slice;
use pourover::{jni_method, ToUnsigned};

#[jni_method(package = "com.google.android.nearby.presence.rust", class = "NpAdv")]
extern "system" fn nativeDeserializeAdvertisement<'local>(
    mut env: JNIEnv<'local>,
    _cls: JClass<'local>,
    service_data: JByteArray<'local>,
    credential_book_obj: CredentialBook<JObject<'local>>,
) -> JObject<'local> {
    let Ok(credential_book) = credential_book_obj.get_handle(&mut env) else {
        return JObject::null();
    };

    // Unpack the service data
    let mut service_data_buf = [0i8; 256];
    let Some(service_data) = env.get_array_length(&service_data).ok().and_then(|len| {
        let len = usize::try_from(len).ok()?;
        let region = service_data_buf.get_mut(0..len)?;
        env.get_byte_array_region(&service_data, 0, region).ok()?;
        Some(region.to_unsigned())
    }) else {
        return DeserializeResult::construct_from_error(
            &mut env,
            DeserializeResultError::UnknownError,
        )
        .map(|obj| obj.0)
        .unwrap_or(JObject::null());
    };

    use np_ffi_core::deserialize::{
        v0::DeserializedV0Advertisement::{Legible, NoMatchingCredentials},
        DeserializeAdvertisementResult::{Error, V0, V1},
    };

    let res = match deserialize_advertisement_from_slice(service_data, credential_book) {
        Error => {
            DeserializeResult::construct_from_error(&mut env, DeserializeResultError::UnknownError)
                .map(|obj| obj.0)
        }

        V0(NoMatchingCredentials) => DeserializedV0Advertisement::construct_from_error(
            &mut env,
            V0AdvertisementError::NoMatchingCredentials,
        )
        .and_then(|adv| DeserializeResult::from_v0_advertisement(&mut env, adv))
        .map(|obj| obj.0),

        V0(Legible(adv)) => DeserializedV0Advertisement::construct(
            &mut env,
            adv.num_des(),
            adv.payload(),
            adv.identity_kind(),
            credential_book_obj,
        )
        .and_then(|adv| DeserializeResult::from_v0_advertisement(&mut env, adv))
        .map(|obj| obj.0),

        V1(adv) => LegibleV1Sections::construct(&mut env, adv.legible_sections)
            .and_then(|sections| {
                DeserializedV1Advertisement::construct(
                    &mut env,
                    jint::from(adv.num_legible_sections),
                    jint::from(adv.num_undecryptable_sections),
                    sections,
                    credential_book_obj,
                )
            })
            .and_then(|adv| DeserializeResult::from_v1_advertisement(&mut env, adv))
            .map(|res| res.0),
    };

    res.unwrap_or(JObject::null())
}
