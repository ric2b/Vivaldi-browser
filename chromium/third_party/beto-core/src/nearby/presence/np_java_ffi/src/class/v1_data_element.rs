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

//! Data Elements for v1 advertisements. See `class V1DataElement`.

use jni::{
    objects::{JByteArray, JObject},
    sys::jlong,
    JNIEnv,
};
use pourover::desc::ClassDesc;

static GENERIC_CLASS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/V1DataElement$Generic");

/// Rust representation of `class V1DataElement.Generic`.
#[repr(transparent)]
pub struct Generic<Obj>(pub Obj);

impl<'local> Generic<JObject<'local>> {
    /// Create a new Java instance from the given data element info.
    pub fn construct<'data>(
        env: &mut JNIEnv<'local>,
        de_type: jlong,
        data: JByteArray<'data>,
    ) -> jni::errors::Result<Self> {
        pourover::call_constructor!(env, &GENERIC_CLASS, "(J[B)V", de_type, data).map(Self)
    }
}

impl<'local, Obj: AsRef<JObject<'local>>> Generic<Obj> {
    /// Cast the given Java object to `Generic` if it is an instance of the type. Returns `None` if
    /// the object's type does not match.
    pub fn checked_cast<'other_local>(
        env: &mut JNIEnv<'other_local>,
        obj: Obj,
    ) -> jni::errors::Result<Option<Self>> {
        Ok(env.is_instance_of(obj.as_ref(), &GENERIC_CLASS)?.then(|| Self(obj)))
    }
}
