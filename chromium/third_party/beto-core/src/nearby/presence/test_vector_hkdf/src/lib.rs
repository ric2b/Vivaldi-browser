// Copyright 2022 Google LLC
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

//! Tools for generating test vector data in a repeatable way.
//!
//! The common approach of just using an RNG means that even a small change to how test vectors are
//! created will regenerate everything. Instead, by using a predictable seed and deriving data from
//! that with per-datum names, incremental changes can be made without regenerating everything.
//!
//! # Examples
//!
//! Generating 100 groups of data:
//!
//! ```
//! use crypto_provider_default::CryptoProviderImpl;
//! use test_vector_hkdf::TestVectorHkdf;
//!
//! for i in 0_u32..100 {
//!     let hkdf = TestVectorHkdf::<CryptoProviderImpl>::new(
//!         "Spiffy test vectors - random seed: hunter2",
//!          i.to_be_bytes().as_slice());
//!
//!     let vec = hkdf.derive_vec(
//!         "fun data",
//!         hkdf.derive_range_element("fun data length", 3..=18).try_into().unwrap());
//!     let array = hkdf.derive_array::<16>("array data");
//!     // store the generated data somewhere
//! }
//! ```

#![allow(clippy::unwrap_used)]

use crypto_provider::hkdf::Hkdf;
use crypto_provider::CryptoProvider;
use crypto_provider_default::CryptoProviderImpl;
use std::ops;

/// Typical usage would generate one instance per loop, and derive all data needed for that loop
/// iteration.
///
/// The `description` passed to all `derive_` calls should be distinct, unless identical data is
/// desired.
pub struct TestVectorHkdf<C: CryptoProvider = CryptoProviderImpl> {
    hkdf: C::HkdfSha256,
}

impl<C: CryptoProvider> TestVectorHkdf<C> {
    /// Create a new instance for the provided namespace and iteration.
    ///
    /// The namespace should contain a blob of randomly generated data. If reshuffling the generated
    /// data is desired, just change the blob of random data.
    pub fn new(namespace: &str, iteration: &[u8]) -> Self {
        Self { hkdf: C::HkdfSha256::new(Some(namespace.as_bytes()), iteration) }
    }

    /// Derive an array of `N` bytes.
    pub fn derive_array<const N: usize>(&self, description: &str) -> [u8; N] {
        let mut arr = [0; N];
        self.hkdf.expand(description.as_bytes(), &mut arr).unwrap();
        arr
    }

    /// Derive a Vec of the specified `len`.
    ///
    /// # Panics
    ///
    /// Panics if `len` is too long for an HKDF output.
    pub fn derive_vec(&self, description: &str, len: usize) -> Vec<u8> {
        let mut vec = vec![0; len];
        self.hkdf.expand(description.as_bytes(), &mut vec).unwrap();
        vec
    }

    /// Generated a biased element in a range using a sloppy sampling technique.
    /// Will be increasingly biased as the range gets bigger.
    pub fn derive_range_element(&self, description: &str, range: ops::RangeInclusive<u64>) -> u64 {
        let num = u64::from_be_bytes(self.derive_array(description));
        num % (range.end() - range.start() + 1) + range.start()
    }
}
