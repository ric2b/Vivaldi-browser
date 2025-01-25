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

/// A mutual exclusion primitive useful for protecting shared data
pub struct Mutex<T>(spin::Mutex<T>);

impl<T> NoPoisonMutex<T> for Mutex<T> {
    type MutexGuard<'a> = spin::MutexGuard<'a, T> where T: 'a;

    fn lock(&self) -> Self::MutexGuard<'_> {
        self.0.lock()
    }

    fn try_lock(&self) -> Option<Self::MutexGuard<'_>> {
        self.0.try_lock()
    }

    fn new(value: T) -> Self {
        Self(spin::Mutex::new(value))
    }
}

/// A reader-writer lock
/// This type of lock allows a number of readers or at most one writer at any point in time.
/// The write portion of this lock typically allows modification of the underlying data (exclusive access)
/// and the read portion of this lock typically allows for read-only access (shared access).
pub struct RwLock<T>(spin::RwLock<T>);

impl<T> RwLock<T> {
    /// Creates a new instance of an `RwLock<T>` which is unlocked.
    pub const fn new(inner: T) -> Self {
        Self(spin::RwLock::new(inner))
    }
}

impl<T> crate::RwLock<T> for RwLock<T> {
    type RwLockReadGuard<'a> = spin::RwLockReadGuard<'a, T> where T: 'a;
    type RwLockWriteGuard<'a> = spin::RwLockWriteGuard<'a, T> where T: 'a;

    fn read(&self) -> Self::RwLockReadGuard<'_> {
        self.0.read()
    }

    fn write(&self) -> Self::RwLockWriteGuard<'_> {
        self.0.write()
    }
}
