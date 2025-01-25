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
    objects::{JObject, JString, JThrowable},
    JNIEnv,
};
use pourover::desc::ClassDesc;

static INVALID_DATA_ELEMENT_EXCEPTION: ClassDesc = ClassDesc::new(
    "com/google/android/nearby/presence/rust/SerializationException$InvalidDataElementException",
);

/// Rust representation of `class SerializationException.InvalidDataElementException`.
#[repr(transparent)]
pub struct InvalidDataElementException<Obj>(pub Obj);

impl<'local> InvalidDataElementException<JObject<'local>> {
    /// Create a new instance.
    pub fn construct<'s>(
        env: &mut JNIEnv<'local>,
        reason: &JString<'s>,
    ) -> jni::errors::Result<Self> {
        pourover::call_constructor!(
            env,
            &INVALID_DATA_ELEMENT_EXCEPTION,
            "(Ljava/lang/String;)V",
            reason
        )
        .map(Self)
    }

    /// Create a new instance and throw it.
    pub fn throw_new<'s>(
        env: &mut JNIEnv<'local>,
        reason: &JString<'s>,
    ) -> jni::errors::Result<()> {
        Self::construct(env, reason)?.throw(env)
    }
}

impl<'local, Obj: AsRef<JObject<'local>>> InvalidDataElementException<Obj> {
    /// Throw this exception.
    pub fn throw<'env>(&self, env: &mut JNIEnv<'env>) -> jni::errors::Result<()> {
        env.throw(<&JThrowable>::from(self.0.as_ref()))
    }
}

/// Helper to generate wrapper types for exception classes with no-arg constructors
macro_rules! exception_wrapper {
    ($(#[$docs:meta])* $name:ident, $cls:literal) => {
        $(#[$docs])*
        #[repr(transparent)]
        pub struct $name<Obj>(pub Obj);

        impl<'local> $name<JObject<'local>> {
            /// Create a new instance.
            pub fn construct(env: &mut JNIEnv<'local>) -> jni::errors::Result<Self> {
                static CLS: ClassDesc = ClassDesc::new($cls);
                pourover::call_constructor!(env, &CLS, "()V",).map(Self)
            }

            /// Create a new instance and throw it.
            pub fn throw_new(env: &mut JNIEnv<'local>) -> jni::errors::Result<()> {
                Self::construct(env)?.throw(env)
            }
        }

        impl<'local, Obj: AsRef<JObject<'local>>> $name<Obj> {
            /// Throw this exception.
            pub fn throw<'env>(&self, env: &mut JNIEnv<'env>) -> jni::errors::Result<()> {
                env.throw(<&JThrowable>::from(self.0.as_ref()))
            }
        }

    }
}

exception_wrapper!(
    /// Rust representation of `class SerializationException.UnclosedActiveSectionException`.
    UnclosedActiveSectionException, "com/google/android/nearby/presence/rust/SerializationException$UnclosedActiveSectionException");

exception_wrapper!(
    /// Rust representation of `class SerializationException.InvalidSectionKindException`.
    InvalidSectionKindException, "com/google/android/nearby/presence/rust/SerializationException$InvalidSectionKindException");

exception_wrapper!(
    /// Rust representation of `class SerializationException.InsufficientSpaceException`.
    InsufficientSpaceException, "com/google/android/nearby/presence/rust/SerializationException$InsufficientSpaceException");

exception_wrapper!(
    /// Rust representation of `class SerializationException.LdtEncryptionException`.
    LdtEncryptionException, "com/google/android/nearby/presence/rust/SerializationException$LdtEncryptionException");

exception_wrapper!(
    /// Rust representation of `class SerializationException.UnencryptedSizeException`.
    UnencryptedSizeException, "com/google/android/nearby/presence/rust/SerializationException$UnencryptedSizeException");
