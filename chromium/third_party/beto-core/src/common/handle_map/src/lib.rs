// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! A thread-safe implementation of a map for managing object handles,
//! a safer alternative to raw pointers for FFI interop.

use core::fmt::Debug;
use std::sync::atomic::{AtomicU32, AtomicU64, Ordering};

pub mod declare_handle_map;
mod guard;
pub(crate) mod shard;

#[cfg(test)]
mod tests;

pub use guard::{ObjectReadGuardImpl, ObjectReadWriteGuardImpl};

use shard::{HandleMapShard, ShardAllocationError};

#[doc(hidden)]
pub mod reexport {
    pub use lazy_static;
}

/// An individual handle to be given out by a [`HandleMap`].
/// This representation is untyped, and just a wrapper
/// around a handle-id, in contrast to implementors of `HandleLike`.
#[derive(Clone, Copy, PartialEq, Eq, Hash)]
pub struct Handle {
    handle_id: u64,
}

impl From<&Handle> for Handle {
    fn from(handle: &Handle) -> Self {
        *handle
    }
}

impl Handle {
    /// Constructs a handle wrapping the given ID.
    ///
    /// No validity checks are done on the wrapped ID
    /// to ensure that the given ID is active in
    /// any specific handle-map, and the type
    /// of the handle is not represented.
    ///
    /// As a result, this method is only useful for
    /// allowing access to handles from an FFI layer.
    pub fn from_id(handle_id: u64) -> Self {
        Self { handle_id }
    }

    /// Gets the ID for this handle.
    ///
    /// Since the underlying handle is un-typed,`
    /// this method is only suitable for
    /// transmitting handles across an FFI layer.
    pub fn get_id(&self) -> u64 {
        self.handle_id
    }

    /// Derives the shard index from the handle id
    fn get_shard_index(&self, num_shards: u8) -> usize {
        (self.handle_id % (num_shards as u64)) as usize
    }
}

/// Error raised when attempting to allocate into a full handle-map.
#[derive(Debug)]
pub struct HandleMapFullError;

/// Error raised when the entry for a given [`Handle`] doesn't exist.
#[derive(Debug)]
pub struct HandleNotPresentError;

/// Errors which may be raised while attempting to allocate
/// a handle from contents given by a (fallible) value-provider.
#[derive(Debug)]
pub enum HandleMapTryAllocateError<E: Debug> {
    /// The call to the value-provider for the allocation failed.
    ValueProviderFailed(E),
    /// We couldn't reserve a spot for the allocation, because
    /// the handle-map was full.
    HandleMapFull,
}

/// FFI-transmissible structure expressing the dimensions
/// (max # of allocatable slots, number of shards) of a handle-map
/// to be used upon initialization.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct HandleMapDimensions {
    /// The number of shards which are employed
    /// by the associated handle-map.
    pub num_shards: u8,
    /// The maximum number of active handles which may be
    /// stored within the associated handle-map.
    pub max_active_handles: u32,
}

/// A thread-safe mapping from "handle"s [like pointers, but safer]
/// to underlying structures, supporting allocations, reads, writes,
/// and deallocations of objects behind handles.
pub struct HandleMap<T: Send + Sync> {
    /// The dimensions of this handle-map
    dimensions: HandleMapDimensions,

    /// The individually-lockable "shards" of the handle-map,
    /// among which the keys will be roughly uniformly-distributed.
    handle_map_shards: Box<[HandleMapShard<T>]>,

    /// An atomically-incrementing counter which tracks the
    /// next handle ID which allocations will attempt to use.
    new_handle_id_counter: AtomicU64,

    /// An atomic integer roughly tracking the number of
    /// currently-outstanding allocated entries in this
    /// handle-map among all [`HandleMapShard`]s.
    outstanding_allocations_counter: AtomicU32,
}

impl<T: Send + Sync> HandleMap<T> {
    /// Creates a new handle-map with the given `HandleMapDimensions`.
    pub fn with_dimensions(dimensions: HandleMapDimensions) -> Self {
        let mut handle_map_shards = Vec::with_capacity(dimensions.num_shards as usize);
        for _ in 0..dimensions.num_shards {
            handle_map_shards.push(HandleMapShard::default());
        }
        let handle_map_shards = handle_map_shards.into_boxed_slice();
        Self {
            dimensions,
            handle_map_shards,
            new_handle_id_counter: AtomicU64::new(0),
            outstanding_allocations_counter: AtomicU32::new(0),
        }
    }
}

impl<T: Send + Sync> HandleMap<T> {
    /// Allocates a new object within the given handle-map, returning
    /// a handle to the location it was stored at. This operation
    /// may fail if attempting to allocate over the `dimensions.max_active_handles`
    /// limit imposed on the handle-map, in which case this method
    /// will return a `HandleMapFullError`.
    ///
    /// If you want the passed closure to be able to possibly fail, see
    /// [`Self::try_allocate`] instead.
    pub fn allocate(
        &self,
        initial_value_provider: impl FnOnce() -> T,
    ) -> Result<Handle, HandleMapFullError> {
        let wrapped_value_provider = move || Ok(initial_value_provider());
        self.try_allocate::<core::convert::Infallible>(wrapped_value_provider)
            .map_err(|e| match e {
                HandleMapTryAllocateError::ValueProviderFailed(never) => match never {},
                HandleMapTryAllocateError::HandleMapFull => HandleMapFullError,
            })
    }

    /// Attempts to allocate a new object within the given handle-map, returning
    /// a handle to the location it was stored at.
    ///
    /// This operation may fail if attempting to allocate over the `dimensions.max_active_handles`
    /// limit imposed on the handle-map, in which case this method will return a
    /// `HandleMapTryAllocateError::HandleMapFull` and the given `initial_value_provider` will not
    /// be run.
    ///
    /// The passed initial-value provider may also fail, in which case this will return the error
    /// wrapped in `HandleMapTryAllocateError::ValueProviderFailed`.
    ///
    /// If your initial-value provider is infallible, see [`Self::allocate`] instead.
    pub fn try_allocate<E: Debug>(
        &self,
        initial_value_provider: impl FnOnce() -> Result<T, E>,
    ) -> Result<Handle, HandleMapTryAllocateError<E>> {
        let mut initial_value_provider = initial_value_provider;
        loop {
            // Increment the new-handle-ID counter using relaxed memory ordering,
            // since the only invariant that we want to enforce is that concurrently-running
            // threads always get distinct new handle-ids.
            let new_handle_id = self.new_handle_id_counter.fetch_add(1, Ordering::Relaxed);
            let new_handle = Handle::from_id(new_handle_id);
            let shard_index = new_handle.get_shard_index(self.dimensions.num_shards);

            // Now, check the shard to see if we can actually allocate into it.
            #[allow(clippy::expect_used)]
            let shard_allocate_result = self
                .handle_map_shards
                .get(shard_index)
                .expect("Shard index is always within range")
                .try_allocate(
                    new_handle,
                    initial_value_provider,
                    &self.outstanding_allocations_counter,
                    self.dimensions.max_active_handles,
                );
            match shard_allocate_result {
                Ok(_) => {
                    return Ok(new_handle);
                }
                Err(ShardAllocationError::ValueProviderFailed(e)) => {
                    return Err(HandleMapTryAllocateError::ValueProviderFailed(e))
                }
                Err(ShardAllocationError::ExceedsAllocationLimit) => {
                    return Err(HandleMapTryAllocateError::HandleMapFull);
                }
                Err(ShardAllocationError::EntryOccupied(thrown_back_provider)) => {
                    // We need to do the whole thing again with a new ID
                    initial_value_provider = thrown_back_provider;
                }
            }
        }
    }

    /// Gets a read-only reference to an object within the given handle-map,
    /// if the given handle is present. Otherwise, returns [`HandleNotPresentError`].
    pub fn get(&self, handle: Handle) -> Result<ObjectReadGuardImpl<T>, HandleNotPresentError> {
        let shard_index = handle.get_shard_index(self.dimensions.num_shards);
        #[allow(clippy::expect_used)]
        self.handle_map_shards
            .get(shard_index)
            .expect("shard index is always within range")
            .get(handle)
    }

    /// Gets a read+write reference to an object within the given handle-map,
    /// if the given handle is present. Otherwise, returns [`HandleNotPresentError`].
    pub fn get_mut(
        &self,
        handle: Handle,
    ) -> Result<ObjectReadWriteGuardImpl<T>, HandleNotPresentError> {
        let shard_index = handle.get_shard_index(self.dimensions.num_shards);
        #[allow(clippy::expect_used)]
        self.handle_map_shards
            .get(shard_index)
            .expect("shard_index is always in range")
            .get_mut(handle)
    }

    /// Removes the object pointed to by the given handle in
    /// the handle-map, returning the removed object if it
    /// exists. Otherwise, returns [`HandleNotPresentError`].
    pub fn deallocate(&self, handle: Handle) -> Result<T, HandleNotPresentError> {
        let shard_index = handle.get_shard_index(self.dimensions.num_shards);
        #[allow(clippy::expect_used)]
        self.handle_map_shards
            .get(shard_index)
            .expect("shard index is always in range")
            .deallocate(handle, &self.outstanding_allocations_counter)
    }

    /// Gets the actual number of elements stored in the entire map.
    pub fn get_current_allocation_count(&self) -> u32 {
        self.outstanding_allocations_counter.load(Ordering::Relaxed)
    }

    /// Sets the new-handle-id counter to the given value.
    /// Only suitable for tests.
    #[cfg(test)]
    pub(crate) fn set_new_handle_id_counter(&mut self, value: u64) {
        self.new_handle_id_counter = AtomicU64::new(value);
    }
}

/// Externally-facing trait for things which behave like handle-map handles
/// with a globally-defined handle-map for the type.
pub trait HandleLike: Sized {
    /// The underlying object type pointed-to by this handle
    type Object: Send + Sync;

    /// Tries to allocate a new handle using the given (fallible)
    /// provider to construct the underlying stored object as
    /// a new entry into the global handle table for this type.
    fn try_allocate<E: Debug>(
        initial_value_provider: impl FnOnce() -> Result<Self::Object, E>,
    ) -> Result<Self, HandleMapTryAllocateError<E>>;

    /// Tries to allocate a new handle using the given (infallible)
    /// provider to construct the underlying stored object as
    /// a new entry into the global handle table for this type.
    fn allocate(
        initial_value_provider: impl FnOnce() -> Self::Object,
    ) -> Result<Self, HandleMapFullError>;

    /// Gets a RAII read-guard on the contents behind this handle.
    fn get(&self) -> Result<ObjectReadGuardImpl<Self::Object>, HandleNotPresentError>;

    /// Gets a RAII read-write guard on the contents behind this handle.
    fn get_mut(&self) -> Result<ObjectReadWriteGuardImpl<Self::Object>, HandleNotPresentError>;

    /// Deallocates the contents behind this handle.
    fn deallocate(self) -> Result<Self::Object, HandleNotPresentError>;
}
