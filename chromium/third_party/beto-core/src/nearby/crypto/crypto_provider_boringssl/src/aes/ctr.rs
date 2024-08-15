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

use bssl_crypto::cipher::StreamCipher;
use crypto_provider::aes::{ctr::NonceAndCounter, Aes128Key, Aes256Key, AesKey};

/// BoringSSL implementation of AES-CTR 128.
pub struct AesCtr128(bssl_crypto::cipher::aes_ctr::Aes128Ctr);

impl crypto_provider::aes::ctr::AesCtr for AesCtr128 {
    type Key = Aes128Key;

    fn new(key: &Self::Key, nonce_and_counter: NonceAndCounter) -> Self {
        Self(bssl_crypto::cipher::aes_ctr::Aes128Ctr::new(
            key.as_array(),
            &nonce_and_counter.as_block_array(),
        ))
    }

    #[allow(clippy::expect_used)]
    fn apply_keystream(&mut self, data: &mut [u8]) {
        assert!(data.len() < i32::MAX as usize);
        self.0.apply_keystream(data).expect("Data length should fit inside of a i32")
    }
}

/// BoringSSL implementation of AES-CTR 256.
pub struct AesCtr256(bssl_crypto::cipher::aes_ctr::Aes256Ctr);

impl crypto_provider::aes::ctr::AesCtr for AesCtr256 {
    type Key = Aes256Key;

    fn new(key: &Self::Key, nonce_and_counter: NonceAndCounter) -> Self {
        Self(bssl_crypto::cipher::aes_ctr::Aes256Ctr::new(
            key.as_array(),
            &nonce_and_counter.as_block_array(),
        ))
    }

    #[allow(clippy::expect_used)]
    fn apply_keystream(&mut self, data: &mut [u8]) {
        assert!(data.len() < i32::MAX as usize);
        self.0.apply_keystream(data).expect("Data length should fit inside of a i32")
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::marker::PhantomData;
    use crypto_provider_test::aes::ctr::*;
    use crypto_provider_test::aes::*;

    #[apply(aes_128_ctr_test_cases)]
    fn aes_128_ctr_test(testcase: CryptoProviderTestCase<AesCtr128>) {
        testcase(PhantomData);
    }

    #[apply(aes_256_ctr_test_cases)]
    fn aes_256_ctr_test(testcase: CryptoProviderTestCase<AesCtr256>) {
        testcase(PhantomData);
    }
}
