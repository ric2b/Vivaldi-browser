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
#![no_std]
#![forbid(unsafe_code)]
#![deny(
    missing_docs,
    clippy::indexing_slicing,
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used
)]

//! Crate which provides impls for CryptoProvider backed by BoringSSL
//!
use bssl_crypto::rand_bytes;
use crypto_provider::{CryptoProvider, CryptoRng};

/// Implementation of `crypto_provider::aes` types using BoringSSL
pub mod aes;

/// Implementations of crypto_provider::hkdf traits backed by BoringSSL
pub mod hkdf;

/// Implementations of crypto_provider::hmac traits backed by BoringSSL
pub mod hmac;

/// Implementations of crypto_provider::ed25519 traits backed by BoringSSL
mod ed25519;

/// Implementations of crypto_provider::aead traits backed by BoringSSL
mod aead;

/// Implementations of crypto_provider::p256 traits backed by BoringSSL
mod p256;

/// Implementations of crypto_provider::x25519 traits backed by BoringSSL
mod x25519;

/// Implementations of crypto_provider::sha2 traits backed by BoringSSL
mod sha2;

/// The BoringSSL backed struct which implements CryptoProvider
#[derive(Default, Clone, Debug, PartialEq, Eq)]
pub struct Boringssl;

impl CryptoProvider for Boringssl {
    type HkdfSha256 = hkdf::Hkdf<bssl_crypto::digest::Sha256>;
    type HmacSha256 = hmac::HmacSha256;
    type HkdfSha512 = hkdf::Hkdf<bssl_crypto::digest::Sha512>;
    type HmacSha512 = hmac::HmacSha512;
    type AesCbcPkcs7Padded = aes::cbc::AesCbcPkcs7Padded;
    type X25519 = x25519::X25519Ecdh;
    type P256 = p256::P256Ecdh;
    type Sha256 = sha2::Sha256;
    type Sha512 = sha2::Sha512;
    type Aes128 = aes::Aes128;
    type Aes256 = aes::Aes256;
    type AesCtr128 = aes::ctr::AesCtr128;
    type AesCtr256 = aes::ctr::AesCtr256;
    type Ed25519 = ed25519::Ed25519;
    type Aes128GcmSiv = aead::aes_gcm_siv::AesGcmSiv128;
    type Aes256GcmSiv = aead::aes_gcm_siv::AesGcmSiv256;
    type Aes128Gcm = aead::aes_gcm::AesGcm128;
    type Aes256Gcm = aead::aes_gcm::AesGcm256;
    type CryptoRng = BoringSslRng;

    fn constant_time_eq(a: &[u8], b: &[u8]) -> bool {
        bssl_crypto::constant_time_compare(a, b)
    }
}

/// BoringSSL implemented random number generator
pub struct BoringSslRng;

impl CryptoRng for BoringSslRng {
    fn new() -> Self {
        BoringSslRng {}
    }

    fn next_u64(&mut self) -> u64 {
        let mut buf = [0; 8];
        rand_bytes(&mut buf);
        u64::from_be_bytes(buf)
    }

    fn fill(&mut self, dest: &mut [u8]) {
        rand_bytes(dest)
    }
}

#[cfg(test)]
mod tests {
    use core::marker::PhantomData;
    use crypto_provider_test::prelude::*;
    use crypto_provider_test::sha2::*;

    use crate::Boringssl;

    #[apply(sha2_test_cases)]
    fn sha2_tests(testcase: CryptoProviderTestCase<Boringssl>) {
        testcase(PhantomData);
    }
}
