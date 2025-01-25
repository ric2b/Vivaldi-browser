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

use crate::Handle;
use core::ops::{Deref, DerefMut};
use lock_adapter::stdlib::RwMapping;
use std::collections::HashMap;
use std::marker::PhantomData;

/// A RAII read lock guard for an object in a [`HandleMap`](crate::HandleMap)
/// pointed-to by a given [`Handle`]. When this struct is
/// dropped, the underlying read lock on the associated
/// shard will be dropped.
pub struct ObjectReadGuardImpl<'a, T: 'a> {
    pub(crate) guard: lock_adapter::stdlib::MappedRwLockReadGuard<
        'a,
        <Self as ObjectReadGuard>::Arg,
        <Self as ObjectReadGuard>::Ret,
        <Self as ObjectReadGuard>::Mapping,
    >,
}

/// Trait implemented for an ObjectReadGuard which defines the associated types of the guard and a
/// mapping for how the guard is retrieved from its parent object
pub trait ObjectReadGuard: Deref<Target = Self::Ret> {
    /// The mapping which defines how a guard is retrieved for `Self::Ret` from `Self::Arg`
    type Mapping: RwMapping<Arg = Self::Arg, Ret = Self::Ret>;
    /// The argument type input to the mapping functions
    type Arg;
    /// The Return type of the mapping functions
    type Ret;
}

impl<'a, T> Deref for ObjectReadGuardImpl<'a, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.guard.deref()
    }
}

pub struct ObjectReadGuardMapping<'a, T> {
    pub(crate) handle: Handle,
    pub(crate) _marker: PhantomData<&'a T>,
}

impl<'a, T> RwMapping for ObjectReadGuardMapping<'a, T> {
    type Arg = HashMap<Handle, T>;
    type Ret = T;

    fn map<'b>(&self, arg: &'b Self::Arg) -> &'b Self::Ret {
        #[allow(clippy::expect_used)]
        arg.get(&self.handle).expect("We know that the entry exists, since we've locked the shard and already checked that it exists prior to handing out this new, mapped read-lock.")
    }

    fn map_mut<'b>(&self, arg: &'b mut Self::Arg) -> &'b mut Self::Ret {
        #[allow(clippy::expect_used)]
        arg.get_mut(&self.handle).expect("We know that the entry exists, since we've locked the shard and already checked that it exists prior to handing out this new, mapped read-lock.")
    }
}

impl<'a, T> ObjectReadGuard for ObjectReadGuardImpl<'a, T> {
    type Mapping = ObjectReadGuardMapping<'a, T>;
    type Arg = HashMap<Handle, T>;
    type Ret = T;
}

/// A RAII read-write lock guard for an object in a [`HandleMap`](crate::HandleMap)
/// pointed-to by a given [`Handle`]. When this struct is
/// dropped, the underlying read-write lock on the associated
/// shard will be dropped.
pub struct ObjectReadWriteGuardImpl<'a, T: 'a> {
    pub(crate) guard: lock_adapter::stdlib::MappedRwLockWriteGuard<
        'a,
        <Self as ObjectReadWriteGuard>::Arg,
        <Self as ObjectReadWriteGuard>::Ret,
        <Self as ObjectReadWriteGuard>::Mapping,
    >,
}

/// Trait implemented for an object read guard which defines the associated types of the guard and a
/// mapping for how the guard is retrieved from its parent object
pub trait ObjectReadWriteGuard: Deref<Target = Self::Ret> + DerefMut<Target = Self::Ret> {
    /// The mapping which defines how a guard is retrieved for `Ret` from `Arg`
    type Mapping: RwMapping<Arg = Self::Arg, Ret = Self::Ret>;
    /// The argument type input to the mapping functions
    type Arg;
    /// The Return type of the mapping functions
    type Ret;
}

pub struct ObjectReadWriteGuardMapping<'a, T> {
    pub(crate) handle: Handle,
    pub(crate) _marker: PhantomData<&'a T>,
}

impl<'a, T> Deref for ObjectReadWriteGuardImpl<'a, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.guard.deref()
    }
}

impl<'a, T> DerefMut for ObjectReadWriteGuardImpl<'a, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.guard.deref_mut()
    }
}

impl<'a, T> ObjectReadWriteGuard for ObjectReadWriteGuardImpl<'a, T> {
    type Mapping = ObjectReadWriteGuardMapping<'a, T>;
    type Arg = HashMap<Handle, T>;
    type Ret = T;
}

impl<'a, T> RwMapping for ObjectReadWriteGuardMapping<'a, T> {
    type Arg = HashMap<Handle, T>;
    type Ret = T;

    fn map<'b>(&self, arg: &'b Self::Arg) -> &'b Self::Ret {
        #[allow(clippy::expect_used)]
        arg.get(&self.handle)
            .expect("Caller must verify that provided hande exists")
    }

    fn map_mut<'b>(&self, arg: &'b mut Self::Arg) -> &'b mut Self::Ret {
        #[allow(clippy::expect_used)]
        arg.get_mut(&self.handle)
            .expect("Caller must verify that provided hande exists")
    }
}
