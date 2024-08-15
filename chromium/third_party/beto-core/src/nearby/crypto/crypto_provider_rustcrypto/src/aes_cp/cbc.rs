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

#[cfg(feature = "alloc")]
extern crate alloc;

use aes::cipher::{block_padding::Pkcs7, BlockDecryptMut, BlockEncryptMut, KeyIvInit};
use aes::Aes256;
#[cfg(feature = "alloc")]
use alloc::vec::Vec;
use crypto_provider::{
    aes::{
        cbc::{AesCbcIv, DecryptionError, EncryptionError},
        Aes256Key, AesKey,
    },
    tinyvec::SliceVec,
};

/// RustCrypto implementation of AES-CBC with PKCS7 padding
pub enum AesCbcPkcs7Padded {}
impl crypto_provider::aes::cbc::AesCbcPkcs7Padded for AesCbcPkcs7Padded {
    #[cfg(feature = "alloc")]
    fn encrypt(key: &Aes256Key, iv: &AesCbcIv, message: &[u8]) -> Vec<u8> {
        let encryptor = cbc::Encryptor::<Aes256>::new(key.as_array().into(), iv.into());
        encryptor.encrypt_padded_vec_mut::<Pkcs7>(message)
    }

    fn encrypt_in_place(
        key: &Aes256Key,
        iv: &AesCbcIv,
        message: &mut SliceVec<u8>,
    ) -> Result<(), EncryptionError> {
        let encryptor = cbc::Encryptor::<Aes256>::new(key.as_array().into(), iv.into());
        let message_len = message.len();
        // Set the length so encrypt_padded_mut can write using the full capacity
        // (Unlike `Vec.set_len`, `SliceVec.set_len` is safe and won't panic if len <= capacity)
        message.set_len(message.capacity());
        encryptor
            .encrypt_padded_mut::<Pkcs7>(message, message_len)
            .map(|result| result.len())
            // `SliceVec.set_len` is safe, and won't panic because `encrypt_padded_mut` never
            // returns a slice longer than the given buffer.
            .map(|new_len| message.set_len(new_len))
            .map_err(|_| {
                message.set_len(message_len); // Set the buffer back to its original length
                EncryptionError::PaddingFailed
            })
    }

    #[cfg(feature = "alloc")]
    fn decrypt(
        key: &Aes256Key,
        iv: &AesCbcIv,
        ciphertext: &[u8],
    ) -> Result<Vec<u8>, DecryptionError> {
        cbc::Decryptor::<Aes256>::new(key.as_array().into(), iv.into())
            .decrypt_padded_vec_mut::<Pkcs7>(ciphertext)
            .map_err(|_| DecryptionError::BadPadding)
    }

    fn decrypt_in_place(
        key: &Aes256Key,
        iv: &AesCbcIv,
        ciphertext: &mut SliceVec<u8>,
    ) -> Result<(), DecryptionError> {
        // Decrypted size is always smaller than the input size because of padding, so we don't need
        // to set the length to the full capacity.
        cbc::Decryptor::<Aes256>::new(key.as_array().into(), iv.into())
            .decrypt_padded_mut::<Pkcs7>(ciphertext)
            .map(|result| result.len())
            // `SliceVec.set_len` is safe, and won't panic because decrypted result length is always
            // smaller than the input size.
            .map(|new_len| ciphertext.set_len(new_len))
            .map_err(|_| {
                ciphertext.as_mut().fill(0);
                DecryptionError::BadPadding
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
