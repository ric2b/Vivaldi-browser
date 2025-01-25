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

//! Types for creating arenas used in deserialization of np_adv. This implementation is purpose-made
//! for deserializing in `np_adv` and is not intended for general use as an arena.

use crate::extended::BLE_5_ADV_SVC_MAX_CONTENT_LEN;

/// Create a [`DeserializationArena`] suitable for use with deserializing an advertisement.
#[macro_export]
macro_rules! deserialization_arena {
    // Trick borrowed from `pin!`: In an argument to a braced constructor, if we take a reference to
    // a temporary value, that value will be upgraded to live for the scope of the enclosing block,
    // avoiding another `let` binding for the buffer which is normally required to keep the buffer
    // on the stack.
    // Reference: https://doc.rust-lang.org/reference/destructors.html#temporary-lifetime-extension
    () => {
        $crate::deserialization_arena::DeserializationArena {
            buffer: &mut $crate::deserialization_arena::DeserializationArena::new_buffer(),
        }
    };
}

/// A simple allocator that simply keeps splitting the given slice on allocation. Use the
/// [`deserialization_arena!`][crate::deserialization_arena!] macro to create an instance.
pub(crate) struct DeserializationArenaAllocator<'a> {
    #[doc(hidden)]
    slice: &'a mut [u8],
}

impl<'a> DeserializationArenaAllocator<'a> {
    /// Allocates `len` bytes from the slice given upon construction. In the expected use case, the
    /// allocated slice should be written to with actual data, overwriting what's contained in
    /// there. While reading from the allocated slice without first writing to it is safe in the
    /// Rust memory-safety sense, the returned slice contains "garbage" data from the slice given
    /// during construction.
    ///
    /// Returns an error with [`ArenaOutOfSpace`] if the remaining slice is not long enough to
    /// allocate `len` bytes.
    pub fn allocate(&mut self, len: u8) -> Result<&'a mut [u8], ArenaOutOfSpace> {
        if usize::from(len) > self.slice.len() {
            return Err(ArenaOutOfSpace);
        }
        let (allocated, remaining) = core::mem::take(&mut self.slice).split_at_mut(len.into());
        self.slice = remaining;
        // Note: the returned data is logically garbage. While it's deterministic (not UB),
        // semantically this should be treated as a write only slice.
        Ok(allocated)
    }
}

/// A simple allocator that simply keeps splitting the given slice on allocation. Use the
/// [`deserialization_arena!`][crate::deserialization_arena!] macro to create an instance.
pub struct DeserializationArena<'a> {
    #[doc(hidden)] // Exposed for use by `deserialization_arena!` only.
    pub buffer: &'a mut [u8; BLE_5_ADV_SVC_MAX_CONTENT_LEN],
}

impl<'a> DeserializationArena<'a> {
    #[doc(hidden)] // Exposed for use by `deserialization_arena!` only.
    pub fn new_buffer() -> [u8; BLE_5_ADV_SVC_MAX_CONTENT_LEN] {
        [0; BLE_5_ADV_SVC_MAX_CONTENT_LEN]
    }

    /// Convert this arena into an allocator that can start allocating.
    pub(crate) fn into_allocator(self) -> DeserializationArenaAllocator<'a> {
        DeserializationArenaAllocator { slice: self.buffer }
    }
}

/// Error indicating that the arena has ran out of space, and deserialization cannot proceed. This
/// should never happen if the arena is created with [`crate::deserialization_arena!`], since the
/// total size of decrypted sections should be less than the size of the incoming BLE advertisement.
#[derive(Debug, PartialEq, Eq)]
pub(crate) struct ArenaOutOfSpace;

#[cfg(test)]
mod test {
    use crate::{deserialization_arena::ArenaOutOfSpace, extended::BLE_5_ADV_SVC_MAX_CONTENT_LEN};

    #[test]
    fn test_creation() {
        assert_eq!(BLE_5_ADV_SVC_MAX_CONTENT_LEN, deserialization_arena!().buffer.len());
    }

    #[test]
    fn test_allocation() {
        let arena = deserialization_arena!();
        let mut allocator = arena.into_allocator();
        assert_eq!(Ok(&mut [0_u8; 100][..]), allocator.allocate(100));
        assert_eq!(BLE_5_ADV_SVC_MAX_CONTENT_LEN - 100, allocator.slice.len());
    }

    #[test]
    fn test_allocation_overflow() {
        let arena = deserialization_arena!();
        let mut allocator = arena.into_allocator();
        assert_eq!(Err(ArenaOutOfSpace), allocator.allocate(u8::MAX));
    }

    #[test]
    fn test_allocation_twice_overflow() {
        let arena = deserialization_arena!();
        let mut allocator = arena.into_allocator();
        assert_eq!(Ok(&mut [0_u8; 128][..]), allocator.allocate(128));
        assert_eq!(Err(ArenaOutOfSpace), allocator.allocate(128));
    }
}
