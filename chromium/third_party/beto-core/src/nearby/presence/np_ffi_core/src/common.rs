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
//! Common externally-accessible FFI constructs which are needed
//! in order to define the interfaces in this crate's various modules.

use array_view::ArrayView;
use crypto_provider::{CryptoProvider, CryptoRng};
use crypto_provider_default::CryptoProviderImpl;
use handle_map::HandleNotPresentError;
use lock_adapter::stdlib::{RwLock, RwLockWriteGuard};
use lock_adapter::RwLock as _;

const MAX_HANDLES: u32 = u32::MAX - 1;

/// Configuration for top-level constants to be used
/// by the rest of the FFI which are independent of
/// the programming language which we ultimately
/// interface with at a higher level.
#[repr(C)]
pub struct CommonConfig {
    /// The number of shards to employ in all handle-maps,
    /// or zero if we want to use the default.
    ///
    /// The default number of shards will depend on whether
    /// this crate was compiled with the `std` feature or not:
    /// - If compiled with the `std` feature, the default number
    ///   of shards will be set to
    ///   `min(16, std::thread::available_parallelism().unwrap())`,
    ///   assuming that that call completes successfully.
    /// - In all other cases, 16 shards will be used by default.
    num_shards: u8,
}

impl Default for CommonConfig {
    fn default() -> Self {
        Self::new()
    }
}

impl CommonConfig {
    pub(crate) const fn new() -> Self {
        Self { num_shards: 0 }
    }
    #[cfg(feature = "std")]
    pub(crate) fn num_shards(&self) -> u8 {
        if self.num_shards == 0 {
            match std::thread::available_parallelism() {
                Ok(parallelism) => 16u8.min(parallelism),
                Err(_) => 16u8,
            }
        } else {
            self.num_shards
        }
    }
    #[cfg(not(feature = "std"))]
    pub(crate) fn num_shards(&self) -> u8 {
        if self.num_shards == 0 {
            16u8
        } else {
            self.num_shards
        }
    }
    pub(crate) fn set_num_shards(&mut self, num_shards: u8) {
        self.num_shards = num_shards
    }
}

pub(crate) fn default_handle_map_dimensions() -> handle_map::HandleMapDimensions {
    handle_map::HandleMapDimensions {
        num_shards: global_num_shards(),
        max_active_handles: MAX_HANDLES,
    }
}

static COMMON_CONFIG: RwLock<CommonConfig> = RwLock::new(CommonConfig::new());

fn global_num_shards() -> u8 {
    COMMON_CONFIG.read().num_shards()
}

/// Sets an override to the number of shards to employ in the NP FFI's
/// internal handle-maps, which places an upper bound on the number
/// of writing threads which may make progress at any one time
/// when concurrently accessing handles of the same type.
///
/// By default, this value will be set to 16, or in `std` environments,
/// the minimum of 16 and the number of available hardware threads.
/// A shard value override of zero will be interpreted the same
/// as this default.
///
/// Setting this value will have no effect if the handle-maps for the
/// API have already begun being used by the client code, and any
/// values set will take effect upon the first usage of _any_ non-`global_config_set`
/// API call.
pub fn global_config_set_num_shards(num_shards: u8) {
    let mut config = COMMON_CONFIG.write();
    config.set_num_shards(num_shards);
}

/// Holds the count of handles currently allocated for each handle type
#[repr(C)]
pub struct CurrentHandleAllocations {
    cred_book: u32,
    cred_slab: u32,
    decrypted_metadata: u32,
    v0_payload: u32,
    legible_v1_sections: u32,
    v0_advertisement_builder: u32,
    v1_advertisement_builder: u32,
}

/// Returns the count of currently allocated handle types being held by the rust layer. Useful
/// for debugging, logging, and testing.
pub fn global_config_get_current_allocation_count() -> CurrentHandleAllocations {
    CurrentHandleAllocations {
        cred_book: crate::credentials::credential_book::get_current_allocation_count(),
        cred_slab: crate::credentials::credential_slab::get_current_allocation_count(),
        decrypted_metadata: crate::deserialize::decrypted_metadata::get_current_allocation_count(),
        v0_payload: crate::deserialize::v0::v0_payload::get_current_allocation_count(),
        legible_v1_sections:
            crate::deserialize::v1::legible_v1_sections::get_current_allocation_count(),
        v0_advertisement_builder:
            crate::serialize::v0::advertisement_builder::get_current_allocation_count(),
        v1_advertisement_builder:
            crate::serialize::v1::advertisement_builder::get_current_allocation_count(),
    }
}

/// A result-type enum which tells the caller whether/not a deallocation
/// succeeded or failed due to the requested handle not being present.
#[repr(C)]
pub enum DeallocateResult {
    /// The requested handle to deallocate was not present in the map
    NotPresent = 1,
    /// The object behind the handle was successfully deallocated
    Success = 2,
}

impl From<Result<(), HandleNotPresentError>> for DeallocateResult {
    fn from(result: Result<(), HandleNotPresentError>) -> Self {
        match result {
            Ok(_) => DeallocateResult::Success,
            Err(_) => DeallocateResult::NotPresent,
        }
    }
}

/// Represents the raw contents of the service payload data
/// under the Nearby Presence service UUID
#[repr(C)]
pub struct RawAdvertisementPayload {
    bytes: ByteBuffer<255>,
}

impl RawAdvertisementPayload {
    /// Yields a slice of the bytes in this raw advertisement payload.
    #[allow(clippy::unwrap_used)]
    pub fn as_slice(&self) -> &[u8] {
        // The unwrapping here will never trigger a panic,
        // because the byte-buffer is 255 bytes, the byte-length
        // of which is the maximum value storable in a u8.
        self.bytes.as_slice().unwrap()
    }
}

/// A byte-string with a maximum size of N,
/// where only the first `len` bytes are considered
/// to contain the actual payload. N is only
/// permitted to be between 0 and 255.
#[derive(Clone)]
#[repr(C)]
// TODO: Once generic const exprs are stabilized,
// we could instead make N into a compile-time u8.
pub struct ByteBuffer<const N: usize> {
    len: u8,
    bytes: [u8; N],
}

/// A FFI safe wrapper of a fixed size array
#[derive(Clone)]
#[repr(C)]
pub struct FixedSizeArray<const N: usize>([u8; N]);

impl<const N: usize> FixedSizeArray<N> {
    /// Constructs a byte-buffer from a Rust-side-derived owned array
    pub(crate) fn from_array(bytes: [u8; N]) -> Self {
        Self(bytes)
    }
    /// Yields a slice of the bytes
    pub fn as_slice(&self) -> &[u8] {
        self.0.as_slice()
    }
    /// De-structures this FFI-compatible fixed-size array
    /// into a bare Rust fixed size array.
    pub fn into_array(self) -> [u8; N] {
        self.0
    }
}

impl<const N: usize> From<[u8; N]> for FixedSizeArray<N> {
    fn from(arr: [u8; N]) -> Self {
        Self(arr)
    }
}

impl<const N: usize> ByteBuffer<N> {
    /// Constructs a byte-buffer from a Rust-side-derived
    /// ArrayView, which is assumed to be trusted to be
    /// properly initialized, and with a size-bound
    /// under 255 bytes.
    pub fn from_array_view(array_view: ArrayView<u8, N>) -> Self {
        let (len, bytes) = array_view.into_raw_parts();
        let len = len as u8;
        Self { len, bytes }
    }
    /// Yields a slice of the first `self.len` bytes of `self.bytes`.
    pub fn as_slice(&self) -> Option<&[u8]> {
        if self.len as usize <= N {
            Some(&self.bytes[..(self.len as usize)])
        } else {
            None
        }
    }
}

pub(crate) type CryptoRngImpl = <CryptoProviderImpl as CryptoProvider>::CryptoRng;

pub(crate) struct LazyInitCryptoRng {
    maybe_rng: Option<CryptoRngImpl>,
}

impl LazyInitCryptoRng {
    const fn new() -> Self {
        Self { maybe_rng: None }
    }
    pub(crate) fn get_rng(&mut self) -> &mut CryptoRngImpl {
        self.maybe_rng.get_or_insert_with(CryptoRngImpl::new)
    }
}

/// Shared, lazily-initialized cryptographically-secure
/// RNG for all operations in the FFI core.
static CRYPTO_RNG: RwLock<LazyInitCryptoRng> = RwLock::new(LazyInitCryptoRng::new());

/// Gets a write guard to the (lazily-init) library-global crypto rng.
pub(crate) fn get_global_crypto_rng() -> RwLockWriteGuard<'static, LazyInitCryptoRng> {
    CRYPTO_RNG.write()
}

/// Error returned if the bit representation of a supposedly-Rust-constructed
/// -and-validated type actually doesn't correspond to the format of the
/// data structure expected on the Rust side of the boundary, and performing
/// further operations on the structure would yield unintended behavior.
/// If this kind of error is being raised, the foreign lang code must
/// be messing with stack-allocated data structures for this library
/// in an entirely unexpected way.
#[derive(Debug)]
pub struct InvalidStackDataStructure;
