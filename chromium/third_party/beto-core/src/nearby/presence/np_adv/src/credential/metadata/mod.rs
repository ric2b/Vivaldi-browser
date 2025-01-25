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

//! A parking ground for the semi-dormant metadata code.
//!
//! Eventually, when we have a clearer idea of how metadat encryption and identity providers
//! interact, this is likely to change aggressively.

use alloc::vec::Vec;
use crypto_provider::{
    aead::{Aead, AeadInit},
    aes::{self},
    CryptoProvider,
};

use crate::credential::{MetadataDecryptionError, ProtocolVersion};

#[cfg(test)]
mod tests;

/// Encrypts the given plaintext metadata bytes to allow that metadata
/// to be shared with receiving devices.
pub fn encrypt_metadata<C: CryptoProvider, V: ProtocolVersion>(
    hkdf: &np_hkdf::NpKeySeedHkdf<C>,
    identity_token: V::IdentityToken,
    plaintext_metadata: &[u8],
) -> Vec<u8> {
    let aead = <<C as CryptoProvider>::Aes128Gcm as AeadInit<aes::Aes128Key>>::new(
        &V::extract_metadata_key::<C>(identity_token),
    );
    // No additional authenticated data for encrypted metadata.
    aead.encrypt(plaintext_metadata, &[], &V::metadata_nonce_from_key_seed(hkdf))
        .expect("Metadata encryption should be infallible")
}

/// Decrypt the given metadata using the given hkdf and version-specific
/// identity token. Returns [`MetadataDecryptionError`] in the case that
/// the decryption operation failed.
///
/// See also [decrypt_metadata_with_nonce].
pub fn decrypt_metadata<C: CryptoProvider, V: ProtocolVersion>(
    hkdf: &np_hkdf::NpKeySeedHkdf<C>,
    identity_token: V::IdentityToken,
    encrypted_metadata: &[u8],
) -> Result<Vec<u8>, MetadataDecryptionError> {
    decrypt_metadata_with_nonce::<C, V>(
        V::metadata_nonce_from_key_seed(hkdf),
        identity_token,
        encrypted_metadata,
    )
}

/// Decrypt the given metadata using the given hkdf and version-specific
/// identity token. Returns [`MetadataDecryptionError`] in the case that
/// the decryption operation failed.
///
/// See also [decrypt_metadata].
pub fn decrypt_metadata_with_nonce<C: CryptoProvider, V: ProtocolVersion>(
    nonce: <C::Aes128Gcm as Aead>::Nonce,
    identity_token: V::IdentityToken,
    encrypted_metadata: &[u8],
) -> Result<Vec<u8>, MetadataDecryptionError> {
    // No additional authenticated data for encrypted metadata.
    let metadata_key = V::extract_metadata_key::<C>(identity_token);
    <<C as CryptoProvider>::Aes128Gcm as AeadInit<aes::Aes128Key>>::new(&metadata_key)
        .decrypt(encrypted_metadata, &[], &nonce)
        .map_err(|_| MetadataDecryptionError)
}
