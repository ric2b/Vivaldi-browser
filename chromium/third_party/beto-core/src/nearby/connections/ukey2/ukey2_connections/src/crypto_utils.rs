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

use crate::d2d_connection_context_v1::{Aes256Key as RawAes256Key, AesCbcIv};
use crypto_provider::aead::AeadError;
use crypto_provider::aes::cbc::DecryptionError;

/// Encrypt message of length N with AES-CBC-256
pub(crate) fn encrypt_cbc<
    R: rand::Rng + rand::CryptoRng,
    A: crypto_provider::aes::cbc::AesCbcPkcs7Padded,
>(
    key: &RawAes256Key,
    message: &[u8],
    rng: &mut R,
) -> (Vec<u8>, AesCbcIv) {
    let iv: AesCbcIv = rng.gen();
    let ciphertext = A::encrypt(&key[..].try_into().unwrap(), &iv, message);
    (ciphertext, iv)
}

/// Decrypt message of length N with AES-CBC-256
pub(crate) fn decrypt_cbc<A: crypto_provider::aes::cbc::AesCbcPkcs7Padded>(
    key: &RawAes256Key,
    ciphertext: &[u8],
    iv: &AesCbcIv,
) -> Result<Vec<u8>, DecryptionError> {
    A::decrypt(&key[..].try_into().unwrap(), iv, ciphertext)
}

// TODO: Implement caching of these ciphers per connection so we don't recreate on each computation.
pub(crate) fn encrypt_gcm_siv<
    A: crypto_provider::aead::AesGcmSiv
        + crypto_provider::aead::AeadInit<crypto_provider::aes::Aes256Key>,
>(
    key: &RawAes256Key,
    plaintext: &[u8],
    aad: &[u8],
    nonce: &A::Nonce,
) -> Result<Vec<u8>, AeadError> {
    let converted_key = key.as_slice().try_into().unwrap();
    let encrypter = A::new(&converted_key);
    encrypter.encrypt(plaintext, aad, nonce)
}

pub(crate) fn decrypt_gcm_siv<
    A: crypto_provider::aead::AesGcmSiv
        + crypto_provider::aead::AeadInit<crypto_provider::aes::Aes256Key>,
>(
    key: &RawAes256Key,
    ciphertext: &[u8],
    aad: &[u8],
    nonce: &A::Nonce,
) -> Result<Vec<u8>, AeadError> {
    let converted_key = key.as_slice().try_into().unwrap();
    let decrypter = A::new(&converted_key);
    decrypter.decrypt(ciphertext, aad, nonce)
}
