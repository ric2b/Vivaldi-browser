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

//! An abstraction layer for Rust synchronization primitives which provides both no_std and std library
//! based implementations

#![no_std]

#[cfg(feature = "std")]
extern crate std;

/// A Spinlock-based implementation of Mutex using the `spin` crate that can be used in `no_std`
/// environments.
///
/// Available with the feature `spin`.
#[cfg(feature = "spin")]
pub mod spin;

/// A `std` implementation that yields to the operating system while waiting for the lock.
///
/// Available with the feature `std`.
#[cfg(feature = "std")]
pub mod stdlib;

/// A trait for mutex implementations that doesn't support poisoning. If the thread panicked while
/// holding the mutex, the data will be released normally (as spin::Mutex would).
pub trait NoPoisonMutex<T> {
    /// An RAII guard for the mutex value that is returned when the lock is successfully obtained.
    type MutexGuard<'a>
    where
        Self: 'a;

    /// Acquires a mutex, blocking the current thread until it is able to do so.
    ///
    /// This function will block the local thread until it is available to acquire the mutex. Upon
    /// returning, the thread is the only thread with the lock held. An RAII guard is returned to
    /// allow scoped unlock of the lock. When the guard goes out of scope, the mutex will be
    /// unlocked.
    ///
    /// The exact behavior on locking a mutex in the thread which already holds the lock is left
    /// unspecified. However, this function will not return on the second call (it might panic or
    /// deadlock, for example).
    fn lock(&self) -> Self::MutexGuard<'_>;

    /// Attempts to acquire this lock.
    ///
    /// If the lock could not be acquired at this time, then `None` is returned. Otherwise, an RAII
    /// `MutexGuard` is returned. The lock will be unlocked when the guard is dropped.
    ///
    /// This function does not block.
    fn try_lock(&self) -> Option<Self::MutexGuard<'_>>;

    /// Creates a new mutex in an unlocked state ready for use.
    fn new(value: T) -> Self;
}

/// A reader-writer lock. This type of lock allows a number of readers or at most one writer at
/// any point in time.
pub trait RwLock<T> {
    /// RAII structure used to release the shared read access of a lock when dropped.
    type RwLockReadGuard<'a>
    where
        Self: 'a;

    /// RAII structure used to release the exclusive write access of a lock when dropped.
    type RwLockWriteGuard<'a>
    where
        Self: 'a;

    /// Locks this RwLock with shared read access, blocking the current thread until it can be acquired.
    fn read(&self) -> Self::RwLockReadGuard<'_>;

    /// Locks this RwLock with exclusive write access, blocking the current thread until it can be acquired.
    fn write(&self) -> Self::RwLockWriteGuard<'_>;
}
