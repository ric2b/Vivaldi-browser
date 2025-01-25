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

use array_view::ArrayView;
use jni::{
    objects::{JByteArray, JObject},
    sys::jlong,
    JNIEnv,
};
use np_ffi_core::{common::ByteBuffer, serialize::v1::V1DE127ByteBuffer};
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

    /// Get a reference to the inner `jni` crate [`JObject`].
    pub fn as_obj(&self) -> &JObject<'local> {
        self.0.as_ref()
    }

    /// Get the data element's type
    pub fn get_type<'env>(&self, env: &mut JNIEnv<'env>) -> jni::errors::Result<jlong> {
        pourover::call_method!(env, &GENERIC_CLASS, "getType", "()J", self.as_obj())
    }

    /// Get the data element's data. Returns `None` if the data is too long.
    pub fn get_data<'env>(
        &self,
        env: &mut JNIEnv<'env>,
    ) -> jni::errors::Result<Option<ByteBuffer<127>>> {
        let data = pourover::call_method!(env, &GENERIC_CLASS, "getData", "()[B", self.as_obj())?;
        let len = env.get_array_length(&data)? as usize;

        if len > 127 {
            return Ok(None);
        }

        // Length is validated above
        #[allow(clippy::unwrap_used)]
        {
            let mut buffer = [0; 127];
            env.get_byte_array_region(&data, 0, buffer.get_mut(..len).unwrap())?;
            let buffer = buffer.map(|b| b as u8);
            Ok(Some(ByteBuffer::from_array_view(ArrayView::try_from_array(buffer, len).unwrap())))
        }
    }

    /// Get the data element as a `np_ffi_core` byte buffer. Returns `None` if the data element is
    /// not valid.
    pub fn as_core_byte_buffer_de<'env>(
        &self,
        env: &mut JNIEnv<'env>,
    ) -> jni::errors::Result<Option<V1DE127ByteBuffer>> {
        let Some(payload) = self.get_data(env)? else {
            return Ok(None);
        };
        Ok(Some(V1DE127ByteBuffer { de_type: self.get_type(env)? as u32, payload }))
    }
}
