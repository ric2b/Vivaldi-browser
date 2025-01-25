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

use crate::NoPoisonMutex;
use std::ops::{Deref, DerefMut};

/// A mutual exclusion primitive useful for protecting shared data
pub struct Mutex<T>(std::sync::Mutex<T>);

impl<T> NoPoisonMutex<T> for Mutex<T> {
    type MutexGuard<'a> = std::sync::MutexGuard<'a, T> where T: 'a;

    fn lock(&self) -> Self::MutexGuard<'_> {
        self.0.lock().unwrap_or_else(|poison| poison.into_inner())
    }

    fn try_lock(&self) -> Option<Self::MutexGuard<'_>> {
        match self.0.try_lock() {
            Ok(guard) => Some(guard),
            Err(std::sync::TryLockError::Poisoned(guard)) => Some(guard.into_inner()),
            Err(std::sync::TryLockError::WouldBlock) => None,
        }
    }

    fn new(value: T) -> Self {
        Self(std::sync::Mutex::new(value))
    }
}

/// A reader-writer lock
/// This type of lock allows a number of readers or at most one writer at any point in time.
/// The write portion of this lock typically allows modification of the underlying data (exclusive access)
/// and the read portion of this lock typically allows for read-only access (shared access).
pub struct RwLock<T>(std::sync::RwLock<T>);

impl<T> RwLock<T> {
    /// Creates a new instance of an `RwLock<T>` which is unlocked.
    pub const fn new(value: T) -> Self {
        Self(std::sync::RwLock::new(value))
    }
}

impl<T> crate::RwLock<T> for RwLock<T> {
    type RwLockReadGuard<'a> = RwLockReadGuard<'a, T> where T: 'a;

    type RwLockWriteGuard<'a> = RwLockWriteGuard<'a, T> where T: 'a;

    fn read(&self) -> Self::RwLockReadGuard<'_> {
        RwLockReadGuard(self.0.read().unwrap_or_else(|e| e.into_inner()))
    }

    fn write(&self) -> Self::RwLockWriteGuard<'_> {
        RwLockWriteGuard(self.0.write().unwrap_or_else(|e| e.into_inner()))
    }
}

/// RAII structure used to release the shared read access of a lock when dropped.
pub struct RwLockReadGuard<'a, T>(std::sync::RwLockReadGuard<'a, T>);

impl<'a, T> RwLockReadGuard<'a, T> {
    /// Make a new MappedRwLockReadGuard for a component of the locked data.
    pub fn map<U, M>(s: Self, mapping: M) -> MappedRwLockReadGuard<'a, T, U, M>
    where
        M: RwMapping<Arg = T, Ret = U>,
    {
        MappedRwLockReadGuard { mapping, guard: s }
    }
}

impl<'a, T> Deref for RwLockReadGuard<'a, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.0.deref()
    }
}

/// An RAII read lock guard returned by RwLockReadGuard::map, which can point to a subfield of the protected data.
pub struct MappedRwLockReadGuard<'a, T, U, M>
where
    M: RwMapping<Arg = T, Ret = U>,
{
    mapping: M,
    guard: RwLockReadGuard<'a, T>,
}

impl<'a, T, U, M> Deref for MappedRwLockReadGuard<'a, T, U, M>
where
    M: RwMapping<Arg = T, Ret = U>,
{
    type Target = U;

    fn deref(&self) -> &Self::Target {
        self.mapping.map(&*self.guard)
    }
}

/// RAII structure used to release the exclusive write access of a lock when dropped.
pub struct RwLockWriteGuard<'a, T>(std::sync::RwLockWriteGuard<'a, T>);

impl<'a, T> RwLockWriteGuard<'a, T> {
    /// Make a new MappedRwLockWriteGuard for a component of the locked data.
    pub fn map<U, M>(s: Self, mapping: M) -> MappedRwLockWriteGuard<'a, T, U, M>
    where
        M: RwMapping<Arg = T, Ret = U>,
    {
        MappedRwLockWriteGuard { mapping, guard: s }
    }
}

impl<'a, T> Deref for RwLockWriteGuard<'a, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.0.deref()
    }
}

impl<'a, T> DerefMut for RwLockWriteGuard<'a, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.0.deref_mut()
    }
}

/// An RAII read lock guard returned by RwLockWriteGuard::map, which can point to a subfield of the protected data.
pub struct MappedRwLockWriteGuard<'a, T, U, M>
where
    M: RwMapping<Arg = T, Ret = U>,
{
    mapping: M,
    guard: RwLockWriteGuard<'a, T>,
}

impl<'a, P, T, M> Deref for MappedRwLockWriteGuard<'a, P, T, M>
where
    M: RwMapping<Arg = P, Ret = T>,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.mapping.map(&*self.guard)
    }
}

impl<'a, P, T, M> DerefMut for MappedRwLockWriteGuard<'a, P, T, M>
where
    M: RwMapping<Arg = P, Ret = T>,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.mapping.map_mut(&mut *self.guard)
    }
}

/// Mapping functions which define how to map from one locked data type to a component of that locked data
pub trait RwMapping {
    /// The original locked data type
    type Arg;
    /// The returned mapped locked data type which is a component of the original locked data
    type Ret;
    /// Maps from Arg into Ret
    fn map<'a>(&self, arg: &'a Self::Arg) -> &'a Self::Ret;
    /// Mutably maps from Arg into Ret
    fn map_mut<'a>(&self, arg: &'a mut Self::Arg) -> &'a mut Self::Ret;
}
