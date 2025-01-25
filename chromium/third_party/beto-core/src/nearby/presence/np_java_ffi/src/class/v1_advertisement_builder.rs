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

use handle_map::{Handle, HandleLike};
use jni::{
    objects::{JClass, JObject},
    sys::{jboolean, jint, jlong, JNI_TRUE},
    JNIEnv,
};
use np_ffi_core::{
    serialize::v1::{
        self, create_v1_advertisement_builder, AddV1DEResult, AddV1SectionToAdvertisementResult,
        CreateV1AdvertisementBuilderResult, CreateV1SectionBuilderResult,
        SerializeV1AdvertisementResult, V1AdvertisementBuilder,
    },
    serialize::AdvertisementBuilderKind,
};
use pourover::{desc::ClassDesc, jni_method};

use crate::class::{
    v1_data_element::Generic, InsufficientSpaceException, InvalidDataElementException,
    InvalidHandleException, InvalidSectionKindException, NoSpaceLeftException,
    UnclosedActiveSectionException, V1BroadcastCredential, VerificationMode,
};

static V1_BUILDER_HANDLE_CLASS: ClassDesc = ClassDesc::new(
    "com/google/android/nearby/presence/rust/V1AdvertisementBuilder$V1BuilderHandle",
);

/// Rust representation of `class V1AdvertisementBuilder.V1BuilderHandle`.
#[repr(transparent)]
pub struct V1BuilderHandle<Obj>(pub Obj);

impl<'local, Obj: AsRef<JObject<'local>>> V1BuilderHandle<Obj> {
    /// Get a reference to the inner `jni` crate [`JObject`].
    pub fn as_obj(&self) -> &JObject<'local> {
        self.0.as_ref()
    }

    /// Get the Rust [`HandleLike`] representation from this Java object.
    pub fn as_rust_handle<'env>(
        &self,
        env: &mut JNIEnv<'env>,
    ) -> jni::errors::Result<V1AdvertisementBuilder> {
        let handle_id = self.get_handle_id(env)?;
        Ok(V1AdvertisementBuilder::from_handle(Handle::from_id(handle_id as u64)))
    }

    /// Get `long handleId` from the Java object
    fn get_handle_id<'env_local>(
        &self,
        env: &mut JNIEnv<'env_local>,
    ) -> jni::errors::Result<jlong> {
        use V1_BUILDER_HANDLE_CLASS as CLS;
        pourover::call_method!(env, &CLS, "getId", "()J", self.as_obj())
    }
}

static V1_SECTION_BUILDER: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/V1SectionBuilder");

/// Rust representation of `class V1SectionBuilder`.
#[repr(transparent)]
pub struct V1SectionBuilder<Obj>(pub Obj);

impl<'local> V1SectionBuilder<JObject<'local>> {
    /// Create a new V1SectionBuilder instance.
    pub fn construct<'h>(
        env: &mut JNIEnv<'local>,
        handle: V1BuilderHandle<impl AsRef<JObject<'h>>>,
        section: jint,
    ) -> jni::errors::Result<Self> {
        pourover::call_constructor!(
            env,
            &V1_SECTION_BUILDER,
            "(Lcom/google/android/nearby/presence/rust/V1AdvertisementBuilder$V1BuilderHandle;I)V",
            handle.as_obj(),
            section,
        )
        .map(Self)
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V1AdvertisementBuilder.V1BuilderHandle"
)]
extern "system" fn allocate<'local>(
    mut env: JNIEnv<'local>,
    _cls: JClass<'local>,
    encrypted: jboolean,
) -> jlong {
    let kind = if encrypted == JNI_TRUE {
        AdvertisementBuilderKind::Encrypted
    } else {
        AdvertisementBuilderKind::Public
    };
    match create_v1_advertisement_builder(kind) {
        CreateV1AdvertisementBuilderResult::Success(builder) => {
            builder.get_as_handle().get_id() as jlong
        }
        CreateV1AdvertisementBuilderResult::NoSpaceLeft => {
            let _ = NoSpaceLeftException::throw_new(&mut env);
            0
        }
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V1AdvertisementBuilder.V1BuilderHandle"
)]
extern "system" fn nativeAddPublicSection<'local>(
    mut env: JNIEnv<'local>,
    this: V1BuilderHandle<JObject<'local>>,
) -> JObject<'local> {
    let Ok(handle) = this.as_rust_handle(&mut env) else {
        return JObject::null();
    };

    let builder = match handle.public_section_builder() {
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
        V1SectionBuilder::construct(&mut env, this, builder.section_index.into())
    else {
        return JObject::null();
    };

    java_builder.0
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V1AdvertisementBuilder.V1BuilderHandle"
)]
extern "system" fn nativeAddEncryptedSection<'local>(
    mut env: JNIEnv<'local>,
    this: V1BuilderHandle<JObject<'local>>,
    credential: V1BroadcastCredential<JObject<'local>>,
    verification_mode: jint,
) -> JObject<'local> {
    let Ok(handle) = this.as_rust_handle(&mut env) else {
        return JObject::null();
    };

    let Ok(credential) = credential.get_as_core(&mut env) else {
        return JObject::null();
    };

    let verification_mode = match VerificationMode::from_java(&mut env, verification_mode) {
        Ok(Some(verification_mode)) => verification_mode.into(),
        Ok(None) => {
            let _ = env
                .throw_new("java/lang/IllegalArgumentException", "Invalid @VerificationMode value");
            return JObject::null();
        }
        Err(_) => return JObject::null(),
    };

    let builder = match handle.encrypted_section_builder(credential, verification_mode) {
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
        V1SectionBuilder::construct(&mut env, this, builder.section_index.into())
    else {
        return JObject::null();
    };

    java_builder.0
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V1AdvertisementBuilder.V1BuilderHandle"
)]
extern "system" fn nativeAddGenericDataElement<'local>(
    mut env: JNIEnv<'local>,
    this: V1BuilderHandle<JObject<'local>>,
    section_index: jint,
    generic_de: Generic<JObject<'local>>,
) {
    let Ok(adv_builder) = this.as_rust_handle(&mut env) else {
        return;
    };

    let Ok(section_index) = u8::try_from(section_index) else {
        return;
    };

    let section_builder = v1::V1SectionBuilder { adv_builder, section_index };

    let de = match generic_de.as_core_byte_buffer_de(&mut env) {
        Ok(Some(de)) => de,
        Ok(None) => {
            let _ = env
                .new_string("Data is too long")
                .map(|s| env.auto_local(s))
                .and_then(|reason| InvalidDataElementException::throw_new(&mut env, &reason));
            return;
        }
        Err(_jni_err) => {
            return;
        }
    };

    match section_builder.add_127_byte_buffer_de(de) {
        AddV1DEResult::Success => {}
        AddV1DEResult::InvalidSectionHandle => {
            let _ = InvalidHandleException::throw_new(&mut env);
        }
        AddV1DEResult::InsufficientSectionSpace => {
            let _ = InsufficientSpaceException::throw_new(&mut env);
        }
        AddV1DEResult::InvalidDataElement => {
            let _ = env
                .new_string("Invalid generic data element")
                .map(|string| env.auto_local(string))
                .and_then(|reason| InvalidDataElementException::throw_new(&mut env, &reason));
        }
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V1AdvertisementBuilder.V1BuilderHandle"
)]
extern "system" fn nativeFinishSection<'local>(
    mut env: JNIEnv<'local>,
    this: V1BuilderHandle<JObject<'local>>,
    section_index: jint,
) {
    let Ok(adv_builder) = this.as_rust_handle(&mut env) else {
        return;
    };

    let Ok(section_index) = u8::try_from(section_index) else {
        return;
    };

    let section_builder = v1::V1SectionBuilder { adv_builder, section_index };

    match section_builder.add_to_advertisement() {
        AddV1SectionToAdvertisementResult::Success => {
            // It worked.
        }
        AddV1SectionToAdvertisementResult::Error => {
            // This case covers:
            //  * The handle is invalid
            //  * The handle is in an incorrect state (no open section)
            //  * The section builder is not for the currently opened section
            let _ = InvalidHandleException::throw_new(&mut env);
        }
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V1AdvertisementBuilder.V1BuilderHandle"
)]
extern "system" fn nativeBuild<'local>(
    mut env: JNIEnv<'local>,
    this: V1BuilderHandle<JObject<'local>>,
) -> JObject<'local> {
    let Ok(handle) = this.as_rust_handle(&mut env) else {
        return JObject::null();
    };

    match handle.into_advertisement() {
        SerializeV1AdvertisementResult::Success(bytes) => {
            #[allow(clippy::expect_used)]
            let adv_bytes = bytes.as_slice().expect("should never be malformed from core");
            env.byte_array_from_slice(adv_bytes).map_or(JObject::null(), JObject::from)
        }
        SerializeV1AdvertisementResult::InvalidBuilderState => {
            let _ = UnclosedActiveSectionException::throw_new(&mut env);
            JObject::null()
        }
        SerializeV1AdvertisementResult::InvalidAdvertisementBuilderHandle => {
            let _ = InvalidHandleException::throw_new(&mut env);
            JObject::null()
        }
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V1AdvertisementBuilder.V1BuilderHandle"
)]
extern "system" fn deallocate<'local>(
    _env: JNIEnv<'local>,
    _cls: JClass<'local>,
    handle_id: jlong,
) {
    let handle = V1AdvertisementBuilder::from_handle(Handle::from_id(handle_id as u64));
    let _ = handle.deallocate();
}
