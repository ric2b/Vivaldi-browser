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

pub struct AesGcm(bssl_crypto::aead::AesGcm);

impl AeadInit<Aes128Key> for AesGcm {
    fn new(key: &Aes128Key) -> Self {
        Self(bssl_crypto::aead::new_aes_128_gcm(key.as_array()))
    }
}

impl AeadInit<Aes256Key> for AesGcm {
    fn new(key: &Aes256Key) -> Self {
        Self(bssl_crypto::aead::new_aes_256_gcm(key.as_array()))
    }
}

impl crypto_provider::aead::AesGcm for AesGcm {}

impl Aead for AesGcm {
    const TAG_SIZE: usize = 16;
    type Nonce = [u8; 12];
    type Tag = [u8; 16];

    fn encrypt(&self, msg: &[u8], aad: &[u8], nonce: &Self::Nonce) -> Result<Vec<u8>, AeadError> {
        self.0.encrypt(msg, aad, nonce).map_err(|_| AeadError)
    }

    fn encrypt_detached(
        &self,
        _msg: &mut [u8],
        _aad: &[u8],
        _nonce: &Self::Nonce,
    ) -> Result<Self::Tag, AeadError> {
        unimplemented!("Not yet supported by boringssl")
    }

    fn decrypt(&self, msg: &[u8], aad: &[u8], nonce: &Self::Nonce) -> Result<Vec<u8>, AeadError> {
        self.0.decrypt(msg, aad, nonce).map_err(|_| AeadError)
    }

    fn decrypt_detached(
        &self,
        _msg: &mut [u8],
        _aad: &[u8],
        _nonce: &Self::Nonce,
        _tag: &Self::Tag,
    ) -> Result<(), AeadError> {
        unimplemented!("Not yet supported by boringssl")
    }
}

#[cfg(test)]
mod tests {
    use core::marker::PhantomData;

    use crypto_provider_test::aead::aes_gcm::*;
    use crypto_provider_test::aes::*;

    use super::*;

    #[apply(aes_128_gcm_test_cases)]
    fn aes_gcm_128_test(testcase: CryptoProviderTestCase<AesGcm>) {
        testcase(PhantomData);
    }

    #[apply(aes_256_gcm_test_cases)]
    fn aes_gcm_256_test(testcase: CryptoProviderTestCase<AesGcm>) {
        testcase(PhantomData);
    }
}
