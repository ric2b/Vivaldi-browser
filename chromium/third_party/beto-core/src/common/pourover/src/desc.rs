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

//! Cacheable descriptors. These `jni` crate compatible descriptors will cache their value on first
//! lookup. They are meant to be combined with static variables in order to globally cache their
//! associated id values.
//!
//! ### Example
//!
//! ```java
//! package com.example;
//!
//! public class MyClass {
//!     public int foo;
//!     public int getBar() { /* ... */ }
//! }
//! ```
//!
//! ```rust
//! use pourover::desc::*;
//!
//! static MY_CLASS_DESC: ClassDesc = ClassDesc::new("com/example/MyClass");
//! static MY_CLASS_FOO_FIELD: FieldDesc = MY_CLASS_DESC.field("foo", "I");
//! static MY_CLASS_GET_BAR_METHOD: MethodDesc = MY_CLASS_DESC.method("getBar", "I()");
//! ```

#![allow(unsafe_code)]

use jni::{
    descriptors::Desc,
    objects::{GlobalRef, JClass, JFieldID, JMethodID, JObject, JStaticFieldID, JStaticMethodID},
    JNIEnv,
};
use std::sync::{LockResult, RwLock, RwLockReadGuard};

/// JNI descriptor that caches a Java class.
pub struct ClassDesc {
    /// The JNI descriptor for this class.
    descriptor: &'static str,
    /// The cached class
    ///
    /// The implementation assumes that `None` is only written to the lock when `&mut self` is
    /// held. Only `Some` is valid to write to this lock while `&self` is held.
    cls: RwLock<Option<GlobalRef>>,
}

impl ClassDesc {
    /// Create a new descriptor with the given JNI descriptor string.
    pub const fn new(descriptor: &'static str) -> Self {
        Self {
            descriptor,
            cls: RwLock::new(None),
        }
    }

    /// Create a new descriptor for a field member of this class.
    pub const fn field<'cls>(&'cls self, name: &'static str, sig: &'static str) -> FieldDesc<'cls> {
        FieldDesc::new(self, name, sig)
    }

    /// Create a new descriptor for a static field member of this class.
    pub const fn static_field<'cls>(
        &'cls self,
        name: &'static str,
        sig: &'static str,
    ) -> StaticFieldDesc<'cls> {
        StaticFieldDesc::new(self, name, sig)
    }

    /// Create a new descriptor for a method member of this class.
    pub const fn method<'cls>(
        &'cls self,
        name: &'static str,
        sig: &'static str,
    ) -> MethodDesc<'cls> {
        MethodDesc::new(self, name, sig)
    }

    /// Create a new descriptor for a constructor for this class.
    pub const fn constructor<'cls>(&'cls self, sig: &'static str) -> MethodDesc<'cls> {
        MethodDesc::new(self, "<init>", sig)
    }

    /// Create a new descriptor for a static method member of this class.
    pub const fn static_method<'cls>(
        &'cls self,
        name: &'static str,
        sig: &'static str,
    ) -> StaticMethodDesc<'cls> {
        StaticMethodDesc::new(self, name, sig)
    }

    /// Free the cached GlobalRef to the class object. This will happen automatically on drop, but
    /// this method is provided to allow the value to be dropped early. This can be used to perform
    /// cleanup on a thread that is already attached to the JVM.
    pub fn free(&mut self) {
        // Get a mutable reference ignoring poison state
        let mut guard = self.cls.write().ignore_poison();
        let global = guard.take();
        // Drop the guard before global in case it panics. We don't want to poison the lock.
        core::mem::drop(guard);
        // Drop the GlobalRef value to cleanup
        core::mem::drop(global);
    }

    fn get_cached(&self) -> Option<CachedClass<'_>> {
        CachedClass::from_lock(&self.cls)
    }
}

/// Wrapper to allow RwLock references to be returned from Desc. Use the `AsRef` impl to get the
/// associated `JClass` reference. The inner `Option` must always be `Some`. This is enfocred by
/// the `from_lock` constructor.
pub struct CachedClass<'lock>(RwLockReadGuard<'lock, Option<GlobalRef>>);

impl<'lock> CachedClass<'lock> {
    /// Read from the given lock and create a `CachedClass` instance if the lock contains a cached
    /// class value. The given lock must have valid data even if it is poisoned.
    fn from_lock(lock: &'lock RwLock<Option<GlobalRef>>) -> Option<CachedClass<'lock>> {
        let guard = lock.read().ignore_poison();

        // Validate that there is a GlobalRef value already, otherwise avoid constructing `Self`.
        if guard.is_some() {
            Some(Self(guard))
        } else {
            None
        }
    }
}

// Implement AsRef so that we can use this type as `Desc::Output` in [`ClassDesc`].
impl<'lock> AsRef<JClass<'static>> for CachedClass<'lock> {
    fn as_ref(&self) -> &JClass<'static> {
        // `unwrap` is valid since we checked for `Some` in the constructor.
        #[allow(clippy::expect_used)]
        let global = self
            .0
            .as_ref()
            .expect("Created CachedClass in an invalid state");
        // No direct conversion to JClass, so let's go through JObject first.
        let obj: &JObject<'static> = global.as_ref();
        // This assumes our object is a class object.
        let cls: &JClass<'static> = From::from(obj);
        cls
    }
}

/// # Safety
///
/// This returns the correct class instance via `JNIEnv::find_class`. The cached class is held in a
/// [`GlobalRef`] so that it cannot be unloaded.
unsafe impl<'a, 'local> Desc<'local, JClass<'static>> for &'a ClassDesc {
    type Output = CachedClass<'a>;

    fn lookup(self, env: &mut JNIEnv<'local>) -> jni::errors::Result<Self::Output> {
        // Check the cache
        if let Some(cls) = self.get_cached() {
            return Ok(cls);
        }

        {
            // Ignoring poison is fine because we only write fully-constructed values.
            let mut guard = self.cls.write().ignore_poison();

            // Multiple threads could have hit this block at the same time. Only allocate the
            // `GlobalRef` if it was not already allocated.
            if guard.is_none() {
                let cls = env.find_class(self.descriptor)?;
                let global = env.new_global_ref(cls)?;

                // Only directly assign valid values. That way poison state can't have broken
                // invariants. If the above panicked then it will poison without changing the
                // lock's data, and everything will still be fine albeit with a sprinkle of
                // possibly-leaked memory.
                *guard = Some(global);
            }
        }

        // Safe to unwrap since we just set `self.cls` to `Some`. `ClassDesc::free` can't be called
        // before this point because it takes a mutable reference to `*self`.
        #[allow(clippy::unwrap_used)]
        Ok(self.get_cached().unwrap())
    }
}

/// A descriptor for a class member. `Id` is expected to implement the [`MemberId`] trait.
///
/// See [`FieldDesc`], [`StaticFieldDesc`], [`MethodDesc`], and [`StaticMethodDesc`] aliases.
pub struct MemberDesc<'cls, Id> {
    cls: &'cls ClassDesc,
    name: &'static str,
    sig: &'static str,
    id: RwLock<Option<Id>>,
}

/// Descriptor for a field.
pub type FieldDesc<'cls> = MemberDesc<'cls, JFieldID>;
/// Descriptor for a static field.
pub type StaticFieldDesc<'cls> = MemberDesc<'cls, JStaticFieldID>;
/// Descriptor for a method.
pub type MethodDesc<'cls> = MemberDesc<'cls, JMethodID>;
/// Descriptor for a static method.
pub type StaticMethodDesc<'cls> = MemberDesc<'cls, JStaticMethodID>;

impl<'cls, Id: MemberId> MemberDesc<'cls, Id> {
    /// Create a new descriptor for a member of the given class with the given name and signature.
    ///
    /// Please use the helpers on [`ClassDesc`] instead of directly calling this method.
    pub const fn new(cls: &'cls ClassDesc, name: &'static str, sig: &'static str) -> Self {
        Self {
            cls,
            name,
            sig,
            id: RwLock::new(None),
        }
    }

    /// Get the class descriptor that this member is associated to.
    pub const fn cls(&self) -> &'cls ClassDesc {
        self.cls
    }
}

/// # Safety
///
/// This returns the correct id. It is the same id obtained from the JNI. This id can be a pointer
/// in some JVM implementations. See trait [`MemberId`].
unsafe impl<'cls, 'local, Id: MemberId> Desc<'local, Id> for &MemberDesc<'cls, Id> {
    type Output = Id;

    fn lookup(self, env: &mut JNIEnv<'local>) -> jni::errors::Result<Self::Output> {
        // Check the cache.
        if let Some(id) = *self.id.read().ignore_poison() {
            return Ok(id);
        }

        {
            // Ignoring poison is fine because we only write valid values.
            let mut guard = self.id.write().ignore_poison();

            // Multiple threads could have hit this block at the same time. Only lookup the id if
            // the lookup was not already performed.
            if guard.is_none() {
                let id = Id::lookup(env, self)?;

                // Only directly assign valid values. That way poison state can't have broken
                // invariants. If the above panicked then it will poison without changing the
                // lock's data and everything will still be fine.
                *guard = Some(id);

                Ok(id)
            } else {
                // Can unwrap since we just checked for `None`.
                #[allow(clippy::unwrap_used)]
                Ok(*guard.as_ref().unwrap())
            }
        }
    }
}

/// Helper trait that calls into `jni` to lookup the id values. This is specialized on the id's
/// type to call the correct lookup function.
///
/// # Safety
///
/// Implementers must be an ID returned from the JNI. `lookup` must be implemented such that the
/// returned ID matches the JNI descriptor given. All values must be sourced from the JNI APIs.
/// See [`::jni::descriptors::Desc`].
pub unsafe trait MemberId: Sized + Copy + AsRef<Self> {
    /// Lookup the id of the given descriptor via the given environment.
    fn lookup(env: &mut JNIEnv, desc: &MemberDesc<Self>) -> jni::errors::Result<Self>;
}

/// # Safety
///
/// This fetches the matching ID from the JNI APIs.
unsafe impl MemberId for JFieldID {
    fn lookup(env: &mut JNIEnv, desc: &MemberDesc<Self>) -> jni::errors::Result<Self> {
        env.get_field_id(desc.cls(), desc.name, desc.sig)
    }
}

/// # Safety
///
/// This fetches the matching ID from the JNI APIs.
unsafe impl MemberId for JStaticFieldID {
    fn lookup(env: &mut JNIEnv, desc: &MemberDesc<Self>) -> jni::errors::Result<Self> {
        env.get_static_field_id(desc.cls(), desc.name, desc.sig)
    }
}

/// # Safety
///
/// This fetches the matching ID from the JNI APIs.
unsafe impl MemberId for JMethodID {
    fn lookup(env: &mut JNIEnv, desc: &MemberDesc<Self>) -> jni::errors::Result<Self> {
        env.get_method_id(desc.cls(), desc.name, desc.sig)
    }
}

/// # Safety
///
/// This fetches the matching ID from the JNI APIs.
unsafe impl MemberId for JStaticMethodID {
    fn lookup(env: &mut JNIEnv, desc: &MemberDesc<Self>) -> jni::errors::Result<Self> {
        env.get_static_method_id(desc.cls(), desc.name, desc.sig)
    }
}

/// Internal helper to ignore the poison state of `LockResult`.
///
/// The poison state occurs when a panic occurs during the lock's critical section. This means that
/// the invariants of the locked data that were protected by the lock may be broken. When this
/// trait is used below, it is used in scenarios where the locked data does not have invariants
/// that can be broken in this way. In these cases, the possibly-poisoned lock is being used purely
/// for synchronization, so the poison state may be ignored.
trait IgnoreLockPoison {
    type Guard;

    /// Extract the inner `Guard` of this `LockResult`. This ignores whether the lock state is
    /// poisoned or not.
    fn ignore_poison(self) -> Self::Guard;
}

impl<G> IgnoreLockPoison for LockResult<G> {
    type Guard = G;
    fn ignore_poison(self) -> Self::Guard {
        self.unwrap_or_else(|poison| poison.into_inner())
    }
}

#[cfg(test)]
mod test {
    use super::*;

    const DESC: &str = "com/example/Foo";
    static FOO: ClassDesc = ClassDesc::new(DESC);

    static FIELD: FieldDesc = FOO.field("foo", "I");
    static STATIC_FIELD: StaticFieldDesc = FOO.static_field("sfoo", "J");
    static CONSTRUCTOR: MethodDesc = FOO.constructor("(I)V");
    static METHOD: MethodDesc = FOO.method("mfoo", "()Z");
    static STATIC_METHOD: StaticMethodDesc = FOO.static_method("smfoo", "()I");

    #[test]
    fn class_desc_created_properly() {
        assert_eq!(DESC, FOO.descriptor);
        assert!(FOO.cls.read().ignore_poison().is_none());
    }

    #[test]
    fn field_desc_created_properly() {
        assert!(std::ptr::eq(&FOO, FIELD.cls()));
        assert_eq!("foo", FIELD.name);
        assert_eq!("I", FIELD.sig);
    }

    #[test]
    fn static_field_desc_created_properly() {
        assert!(std::ptr::eq(&FOO, STATIC_FIELD.cls()));
        assert_eq!("sfoo", STATIC_FIELD.name);
        assert_eq!("J", STATIC_FIELD.sig);
    }

    #[test]
    fn constructor_desc_created_properly() {
        assert!(std::ptr::eq(&FOO, CONSTRUCTOR.cls()));
        assert_eq!("<init>", CONSTRUCTOR.name);
        assert_eq!("(I)V", CONSTRUCTOR.sig);
    }

    #[test]
    fn method_desc_created_properly() {
        assert!(std::ptr::eq(&FOO, METHOD.cls()));
        assert_eq!("mfoo", METHOD.name);
        assert_eq!("()Z", METHOD.sig);
    }

    #[test]
    fn static_method_desc_created_properly() {
        assert!(std::ptr::eq(&FOO, STATIC_METHOD.cls()));
        assert_eq!("smfoo", STATIC_METHOD.name);
        assert_eq!("()I", STATIC_METHOD.sig);
    }
}
