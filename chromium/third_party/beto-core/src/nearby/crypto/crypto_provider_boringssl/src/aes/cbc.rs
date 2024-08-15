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
use bssl_crypto::cipher::BlockCipher;
use crypto_provider::{
    aes::{
        cbc::{AesCbcIv, DecryptionError, EncryptionError},
        Aes256Key, AesKey,
    },
    tinyvec::SliceVec,
};

/// BoringSSL implementation of AES-CBC with PKCS7 padding
pub enum AesCbcPkcs7Padded {}
impl crypto_provider::aes::cbc::AesCbcPkcs7Padded for AesCbcPkcs7Padded {
    #[allow(clippy::expect_used)]
    fn encrypt(key: &Aes256Key, iv: &AesCbcIv, message: &[u8]) -> Vec<u8> {
        let encryptor = bssl_crypto::cipher::aes_cbc::Aes256Cbc::new_encrypt(key.as_array(), iv);
        encryptor.encrypt_padded(message).expect("Encrypting AES-CBC should be infallible")
    }

    fn encrypt_in_place(
        key: &Aes256Key,
        iv: &AesCbcIv,
        message: &mut SliceVec<u8>,
    ) -> Result<(), EncryptionError> {
        let result = Self::encrypt(key, iv, message);
        if message.capacity() < result.len() {
            return Err(EncryptionError::PaddingFailed);
        }
        message.clear();
        message.extend_from_slice(&result);
        Ok(())
    }

    fn decrypt(
        key: &Aes256Key,
        iv: &AesCbcIv,
        ciphertext: &[u8],
    ) -> Result<Vec<u8>, DecryptionError> {
        let decryptor = bssl_crypto::cipher::aes_cbc::Aes256Cbc::new_decrypt(key.as_array(), iv);
        decryptor.decrypt_padded(ciphertext).map_err(|_| DecryptionError::BadPadding)
    }

    fn decrypt_in_place(
        key: &Aes256Key,
        iv: &AesCbcIv,
        ciphertext: &mut SliceVec<u8>,
    ) -> Result<(), DecryptionError> {
        Self::decrypt(key, iv, ciphertext).map(|result| {
            ciphertext.clear();
            ciphertext.extend_from_slice(&result);
        })
    }
}

#[cfg(test)]
mod tests {
    use super::AesCbcPkcs7Padded;
    use core::marker::PhantomData;
    use crypto_provider_test::aes::cbc::*;

    #[apply(aes_256_cbc_test_cases)]
    fn aes_256_cbc_test(testcase: CryptoProviderTestCase<AesCbcPkcs7Padded>) {
        testcase(PhantomData);
    }
}
