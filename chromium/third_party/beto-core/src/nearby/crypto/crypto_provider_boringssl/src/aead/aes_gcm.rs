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

extern crate alloc;

use alloc::vec::Vec;
use bssl_crypto::aead::Aead as _;
use crypto_provider::aead::{Aead, AeadError, AeadInit};
use crypto_provider::aes::{Aes128Key, Aes256Key, AesKey};

pub struct AesGcm128(bssl_crypto::aead::Aes128Gcm);

pub struct AesGcm256(bssl_crypto::aead::Aes256Gcm);

impl AeadInit<Aes128Key> for AesGcm128 {
    fn new(key: &Aes128Key) -> Self {
        Self(bssl_crypto::aead::Aes128Gcm::new(key.as_array()))
    }
}

impl crypto_provider::aead::AesGcm for AesGcm128 {}

impl Aead for AesGcm128 {
    const TAG_SIZE: usize = 16;
    type Nonce = [u8; 12];
    type Tag = [u8; 16];

    fn encrypt(&self, msg: &[u8], aad: &[u8], nonce: &Self::Nonce) -> Result<Vec<u8>, AeadError> {
        Ok(self.0.seal(nonce, msg, aad))
    }

    fn encrypt_detached(
        &self,
        msg: &mut [u8],
        aad: &[u8],
        nonce: &Self::Nonce,
    ) -> Result<Self::Tag, AeadError> {
        Ok(self.0.seal_in_place(nonce, msg, aad))
    }

    fn decrypt(&self, msg: &[u8], aad: &[u8], nonce: &Self::Nonce) -> Result<Vec<u8>, AeadError> {
        self.0.open(nonce, msg, aad).ok_or(AeadError)
    }

    fn decrypt_detached(
        &self,
        msg: &mut [u8],
        aad: &[u8],
        nonce: &Self::Nonce,
        tag: &Self::Tag,
    ) -> Result<(), AeadError> {
        self.0.open_in_place(nonce, msg, tag, aad).map_err(|_| AeadError)
    }
}

impl AeadInit<Aes256Key> for AesGcm256 {
    fn new(key: &Aes256Key) -> Self {
        Self(bssl_crypto::aead::Aes256Gcm::new(key.as_array()))
    }
}

impl crypto_provider::aead::AesGcm for AesGcm256 {}

impl Aead for AesGcm256 {
    const TAG_SIZE: usize = 16;
    type Nonce = [u8; 12];
    type Tag = [u8; 16];

    fn encrypt(&self, msg: &[u8], aad: &[u8], nonce: &Self::Nonce) -> Result<Vec<u8>, AeadError> {
        Ok(self.0.seal(nonce, msg, aad))
    }

    fn encrypt_detached(
        &self,
        msg: &mut [u8],
        aad: &[u8],
        nonce: &Self::Nonce,
    ) -> Result<Self::Tag, AeadError> {
        Ok(self.0.seal_in_place(nonce, msg, aad))
    }

    fn decrypt(&self, msg: &[u8], aad: &[u8], nonce: &Self::Nonce) -> Result<Vec<u8>, AeadError> {
        self.0.open(nonce, msg, aad).ok_or(AeadError)
    }

    fn decrypt_detached(
        &self,
        msg: &mut [u8],
        aad: &[u8],
        nonce: &Self::Nonce,
        tag: &Self::Tag,
    ) -> Result<(), AeadError> {
        self.0.open_in_place(nonce, msg, tag, aad).map_err(|_| AeadError)
    }
}

#[cfg(test)]
mod tests {
    use core::marker::PhantomData;

    use crypto_provider_test::aead::aes_gcm::*;
    use crypto_provider_test::aes::*;

    use super::*;

    #[apply(aes_128_gcm_test_cases)]
    fn aes_gcm_128_test(testcase: CryptoProviderTestCase<AesGcm128>) {
        testcase(PhantomData);
    }

    #[apply(aes_256_gcm_test_cases)]
    fn aes_gcm_256_test(testcase: CryptoProviderTestCase<AesGcm256>) {
        testcase(PhantomData);
    }
}
