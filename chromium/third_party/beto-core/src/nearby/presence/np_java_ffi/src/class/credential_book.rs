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
    objects::{JClass, JObject},
    sys::jlong,
    JNIEnv,
};

use crate::class::{InvalidHandleException, NoSpaceLeftException};
use handle_map::{Handle, HandleLike};
use np_ffi_core::credentials::{
    create_credential_book_from_slab, deallocate_credential_book, CreateCredentialBookResult,
    CredentialBook as CredentialBookHandle, CredentialSlab,
};
use pourover::{desc::ClassDesc, jni_method};

static CREDENTIAL_BOOK_CLASS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/credential/CredentialBook");

/// Rust representation of `class CredentialBook`.
#[repr(transparent)]
pub struct CredentialBook<Obj>(pub Obj);

impl<'local, Obj: AsRef<JObject<'local>>> CredentialBook<Obj> {
    /// Get the raw handle id
    pub fn get_id<'env>(&self, env: &mut JNIEnv<'env>) -> jni::errors::Result<jlong> {
        pourover::call_method!(env, &CREDENTIAL_BOOK_CLASS, "getId", "()J", self.as_obj(),)
    }

    /// Get the associated Rust handle from this Java handle object
    pub fn get_handle<'env>(
        &self,
        env: &mut JNIEnv<'env>,
    ) -> jni::errors::Result<CredentialBookHandle> {
        self.get_id(env).map(|id| Handle::from_id(id as u64)).map(CredentialBookHandle::from_handle)
    }

    /// Get a reference to the inner `jni` crate [`JObject`].
    pub fn as_obj(&self) -> &JObject<'local> {
        self.0.as_ref()
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust.credential",
    class = "CredentialBook",
    method_name = "allocate"
)]
extern "system" fn allocate_book<'local>(
    mut env: JNIEnv<'local>,
    _cls: JClass<'local>,
    slab_handle_id: jlong,
) -> jlong {
    let slab = CredentialSlab::from_handle(Handle::from_id(slab_handle_id as u64));

    match create_credential_book_from_slab(slab) {
        CreateCredentialBookResult::Success(handle) => handle.get_as_handle().get_id() as jlong,
        CreateCredentialBookResult::InvalidSlabHandle => {
            let _ = InvalidHandleException::throw_new(&mut env);
            0
        }
        CreateCredentialBookResult::NoSpaceLeft => {
            // Make sure slab is consumed.
            let _ = slab.deallocate();
            let _ = NoSpaceLeftException::throw_new(&mut env);
            0
        }
    }
}

#[jni_method(
    package = "com.google.android.nearby.presence.rust.credential",
    class = "CredentialBook",
    method_name = "deallocate"
)]
extern "system" fn deallocate_book<'local>(
    _env: JNIEnv<'local>,
    _cls: JClass<'local>,
    handle_id: jlong,
) {
    // Swallow errors here since there's nothing meaningful to do.
    let _ = deallocate_credential_book(CredentialBookHandle::from_handle(Handle::from_id(
        handle_id as u64,
    )));
}
