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
    objects::{JObject, JThrowable},
    JNIEnv,
};
use pourover::desc::ClassDesc;

static INVALID_HANDLE_EXCEPTION_CLASS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/Handle$InvalidHandleException");

/// Rust representation of `class InvalidHandleException`.
#[repr(transparent)]
pub struct InvalidHandleException<Obj>(pub Obj);

impl<'local> InvalidHandleException<JObject<'local>> {
    /// Create a new instance.
    pub fn construct(env: &mut JNIEnv<'local>) -> jni::errors::Result<Self> {
        pourover::call_constructor!(env, &INVALID_HANDLE_EXCEPTION_CLASS, "()V").map(Self)
    }

    /// Create a new instance and throw it.
    pub fn throw_new(env: &mut JNIEnv<'local>) -> jni::errors::Result<()> {
        Self::construct(env)?.throw(env)
    }
}

impl<'local, Obj: AsRef<JObject<'local>>> InvalidHandleException<Obj> {
    /// Throw this exception.
    pub fn throw<'env>(&self, env: &mut JNIEnv<'env>) -> jni::errors::Result<()> {
        env.throw(<&JThrowable>::from(self.0.as_ref()))
    }
}
