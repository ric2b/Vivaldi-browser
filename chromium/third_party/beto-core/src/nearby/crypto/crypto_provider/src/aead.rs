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

#[cfg(feature = "alloc")]
extern crate alloc;
#[cfg(feature = "alloc")]
use alloc::vec::Vec;

/// An implementation of AES-GCM-SIV.
///
/// An AesGcmSiv impl may be used for encryption and decryption.
pub trait AesGcmSiv: Aead<Nonce = [u8; 12]> {}

/// An implementation of AES-GCM.
///
/// An AesGcm impl may be used for encryption and decryption.
pub trait AesGcm: Aead<Nonce = [u8; 12]> {}

/// Error returned on unsuccessful AEAD operation.
#[derive(Debug)]
pub struct AeadError;

/// Initializes an AEAD
pub trait AeadInit<K: crate::aes::AesKey> {
    /// Instantiates a new instance of the AEAD from key material.
    fn new(key: &K) -> Self;
}

/// Authenticated Encryption with Associated Data (AEAD) algorithm, where `N` is the size of the
/// Nonce. Encrypts and decrypts buffers in-place.
pub trait Aead {
    /// The size of the authentication tag, this is appended to the message on the encrypt operation
    /// and truncated from the plaintext after decrypting.
    const TAG_SIZE: usize;

    /// The cryptographic nonce used by the AEAD. The nonce must be unique for all messages with
    /// the same key. This is critically important - nonce reuse may completely undermine the
    /// security of the AEAD. Nonces may be predictable and public, so long as they are unique.
    type Nonce: AsRef<[u8]>;

    /// The type of the tag, which should always be [u8; Self::TAG_SIZE].
    type Tag: AsRef<[u8]>;

    /// Encrypt the given buffer containing a plaintext message. On success returns the encrypted
    /// `msg` and appended auth tag, which will result in a Vec which is  `Self::TAG_SIZE` bytes
    /// greater than the initial message.
    #[cfg(feature = "alloc")]
    fn encrypt(&self, msg: &[u8], aad: &[u8], nonce: &Self::Nonce) -> Result<Vec<u8>, AeadError>;

    /// Encrypt the given buffer containing a plaintext message in-place, and returns the tag in the
    /// result value.
    fn encrypt_detached(
        &self,
        msg: &mut [u8],
        aad: &[u8],
        nonce: &Self::Nonce,
    ) -> Result<Self::Tag, AeadError>;

    /// Decrypt the message, returning the decrypted plaintext or an error in the event the
    /// provided authentication tag does not match the given ciphertext. On success the returned
    /// Vec will only contain the plaintext and so will be `Self::TAG_SIZE` bytes less than the
    /// initial message.
    #[cfg(feature = "alloc")]
    fn decrypt(&self, msg: &[u8], aad: &[u8], nonce: &Self::Nonce) -> Result<Vec<u8>, AeadError>;

    /// Decrypt the message in-place, returning an error and leaving the input `msg` unchanged in
    /// the event the provided authentication tag does not match the given ciphertext.
    fn decrypt_detached(
        &self,
        msg: &mut [u8],
        aad: &[u8],
        nonce: &Self::Nonce,
        tag: &Self::Tag,
    ) -> Result<(), AeadError>;
}
