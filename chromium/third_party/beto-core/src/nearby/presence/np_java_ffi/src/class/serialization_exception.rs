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

static INSUFFICIENT_SPACE_EXCEPTION: ClassDesc = ClassDesc::new(
    "com/google/android/nearby/presence/rust/SerializationException$InsufficientSpaceException",
);

/// Rust representation of `class SerializationException.InsufficientSpaceException`.
#[repr(transparent)]
pub struct InsufficientSpaceException<Obj>(pub Obj);

impl<'local> InsufficientSpaceException<JObject<'local>> {
    /// Create a new instance.
    pub fn construct(env: &mut JNIEnv<'local>) -> jni::errors::Result<Self> {
        pourover::call_constructor!(env, &INSUFFICIENT_SPACE_EXCEPTION, "()V",).map(Self)
    }

    /// Create a new instance and throw it.
    pub fn throw_new(env: &mut JNIEnv<'local>) -> jni::errors::Result<()> {
        Self::construct(env)?.throw(env)
    }
}

impl<'local, Obj: AsRef<JObject<'local>>> InsufficientSpaceException<Obj> {
    /// Throw this exception.
    pub fn throw<'env>(&self, env: &mut JNIEnv<'env>) -> jni::errors::Result<()> {
        env.throw(<&JThrowable>::from(self.0.as_ref()))
    }
}

static LDT_ENCRYPTION_EXCEPTION_CLASS: ClassDesc = ClassDesc::new(
    "com/google/android/nearby/presence/rust/SerializationException$LdtEncryptionException",
);

/// Rust representation of `class SerializationException.LdtEncryptionException`.
#[repr(transparent)]
pub struct LdtEncryptionException<Obj>(pub Obj);

impl<'local> LdtEncryptionException<JObject<'local>> {
    /// Create a new instance.
    pub fn construct(env: &mut JNIEnv<'local>) -> jni::errors::Result<Self> {
        pourover::call_constructor!(env, &LDT_ENCRYPTION_EXCEPTION_CLASS, "()V").map(Self)
    }

    /// Create a new instance and throw it.
    pub fn throw_new(env: &mut JNIEnv<'local>) -> jni::errors::Result<()> {
        Self::construct(env)?.throw(env)
    }
}

impl<'local, Obj: AsRef<JObject<'local>>> LdtEncryptionException<Obj> {
    /// Throw this exception.
    pub fn throw<'env>(&self, env: &mut JNIEnv<'env>) -> jni::errors::Result<()> {
        env.throw(<&JThrowable>::from(self.0.as_ref()))
    }
}

static UNENCRYPTED_SIZE_EXCEPTION_CLASS: ClassDesc = ClassDesc::new(
    "com/google/android/nearby/presence/rust/SerializationException$UnencryptedSizeException",
);

/// Rust representation of `class SerializationException.UnencryptedSizeException`.
#[repr(transparent)]
pub struct UnencryptedSizeException<Obj>(pub Obj);

impl<'local> UnencryptedSizeException<JObject<'local>> {
    /// Create a new instance.
    pub fn construct(env: &mut JNIEnv<'local>) -> jni::errors::Result<Self> {
        pourover::call_constructor!(env, &UNENCRYPTED_SIZE_EXCEPTION_CLASS, "()V").map(Self)
    }

    /// Create a new instance and throw it.
    pub fn throw_new(env: &mut JNIEnv<'local>) -> jni::errors::Result<()> {
        Self::construct(env)?.throw(env)
    }
}

impl<'local, Obj: AsRef<JObject<'local>>> UnencryptedSizeException<Obj> {
    /// Throw this exception.
    pub fn throw<'env>(&self, env: &mut JNIEnv<'env>) -> jni::errors::Result<()> {
        env.throw(<&JThrowable>::from(self.0.as_ref()))
    }
}
