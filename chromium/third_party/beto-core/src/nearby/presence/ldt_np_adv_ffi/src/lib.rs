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

#![deny(
    missing_docs,
    clippy::indexing_slicing,
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used
)]

//! Rust ffi wrapper of ldt_np_adv, can be called from C/C++ Clients

extern crate alloc;

use core::slice;
use crypto_provider_default::CryptoProviderImpl;
use ldt::LdtCipher;
use ldt_np_adv::{
    build_np_adv_decrypter_from_key_seed, salt_padder, AuthenticatedNpLdtDecryptCipher,
    LdtAdvDecryptError, NpLdtEncryptCipher, V0Salt, V0_IDENTITY_TOKEN_LEN,
};
use np_hkdf::NpKeySeedHkdf;

mod handle_map;

pub(crate) type LdtAdvDecrypter = AuthenticatedNpLdtDecryptCipher<CryptoProviderImpl>;
pub(crate) type LdtAdvEncrypter = NpLdtEncryptCipher<CryptoProviderImpl>;

const SUCCESS: i32 = 0;

#[repr(C)]
struct NpLdtKeySeed {
    bytes: [u8; 32],
}

#[repr(C)]
struct NpMetadataKeyHmac {
    bytes: [u8; 32],
}

#[repr(C)]
struct NpLdtSalt {
    bytes: [u8; 2],
}

#[repr(C)]
struct NpLdtEncryptHandle {
    handle: u64,
}

#[repr(C)]
struct NpLdtDecryptHandle {
    handle: u64,
}

#[no_mangle]
extern "C" fn NpLdtDecryptCreate(
    key_seed: NpLdtKeySeed,
    metadata_key_hmac: NpMetadataKeyHmac,
) -> NpLdtDecryptHandle {
    let cipher = build_np_adv_decrypter_from_key_seed(
        &NpKeySeedHkdf::new(&key_seed.bytes),
        metadata_key_hmac.bytes,
    );
    let handle = handle_map::get_dec_handle_map().insert::<CryptoProviderImpl>(Box::new(cipher));
    NpLdtDecryptHandle { handle }
}

#[no_mangle]
extern "C" fn NpLdtEncryptCreate(key_seed: NpLdtKeySeed) -> NpLdtEncryptHandle {
    let cipher = LdtAdvEncrypter::new(
        &NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed.bytes).v0_ldt_key(),
    );
    let handle = handle_map::get_enc_handle_map().insert::<CryptoProviderImpl>(Box::new(cipher));
    NpLdtEncryptHandle { handle }
}

#[no_mangle]
extern "C" fn NpLdtEncryptClose(handle: NpLdtEncryptHandle) -> i32 {
    map_to_error_code(|| {
        handle_map::get_enc_handle_map()
            .remove(&handle.handle)
            .ok_or(CloseCipherError::InvalidHandle)
            .map(|_| SUCCESS)
    })
}

#[no_mangle]
extern "C" fn NpLdtDecryptClose(handle: NpLdtDecryptHandle) -> i32 {
    map_to_error_code(|| {
        handle_map::get_dec_handle_map()
            .remove(&handle.handle)
            .ok_or(CloseCipherError::InvalidHandle)
            .map(|_| SUCCESS)
    })
}

#[no_mangle]
extern "C" fn NpLdtEncrypt(
    handle: NpLdtEncryptHandle,
    buffer: *mut u8,
    buffer_len: usize,
    salt: NpLdtSalt,
) -> i32 {
    map_to_error_code(|| {
        let data = unsafe { slice::from_raw_parts_mut(buffer, buffer_len) };
        let padder = salt_padder::<CryptoProviderImpl>(V0Salt::from(salt.bytes));
        handle_map::get_enc_handle_map()
            .get(&handle.handle)
            .map(|cipher| {
                cipher.encrypt(data, &padder).map(|_| 0).map_err(|e| match e {
                    ldt::LdtError::InvalidLength(_) => EncryptError::InvalidLength,
                })
            })
            .unwrap_or(Err(EncryptError::InvalidHandle))
    })
}

#[no_mangle]
extern "C" fn NpLdtDecryptAndVerify(
    handle: NpLdtDecryptHandle,
    buffer: *mut u8,
    buffer_len: usize,
    salt: NpLdtSalt,
) -> i32 {
    map_to_error_code(|| {
        let data = unsafe { slice::from_raw_parts_mut(buffer, buffer_len) };
        let padder = salt_padder::<CryptoProviderImpl>(V0Salt::from(salt.bytes));

        #[allow(clippy::indexing_slicing)]
        handle_map::get_dec_handle_map()
            .get(&handle.handle)
            .map(|cipher| {
                cipher
                    .decrypt_and_verify(data, &padder)
                    .map_err(|e| match e {
                        LdtAdvDecryptError::InvalidLength(_) => DecryptError::InvalidLength,
                        LdtAdvDecryptError::MacMismatch => DecryptError::HmacDoesntMatch,
                    })
                    .map(|(token, plaintext)| {
                        // slicing is safe: token and plaintext sum to data's len
                        data[..V0_IDENTITY_TOKEN_LEN].copy_from_slice(token.as_slice());
                        data[V0_IDENTITY_TOKEN_LEN..].copy_from_slice(plaintext.as_slice());
                        SUCCESS
                    })
            })
            .unwrap_or(Err(DecryptError::InvalidHandle))
    })
}

fn map_to_error_code<T, E: ErrorEnum<T>, F: Fn() -> Result<T, E>>(f: F) -> T {
    f().unwrap_or_else(|e| e.to_error_code())
}

trait ErrorEnum<C> {
    fn to_error_code(&self) -> C;
}

enum CloseCipherError {
    InvalidHandle,
}

impl ErrorEnum<i32> for CloseCipherError {
    fn to_error_code(&self) -> i32 {
        match self {
            Self::InvalidHandle => -3,
        }
    }
}

enum EncryptError {
    InvalidLength,
    InvalidHandle,
}

impl ErrorEnum<i32> for EncryptError {
    fn to_error_code(&self) -> i32 {
        match self {
            Self::InvalidLength => -1,
            Self::InvalidHandle => -3,
        }
    }
}

enum DecryptError {
    HmacDoesntMatch,
    InvalidLength,
    InvalidHandle,
}

impl ErrorEnum<i32> for DecryptError {
    fn to_error_code(&self) -> i32 {
        match self {
            Self::InvalidLength => -1,
            Self::HmacDoesntMatch => -2,
            Self::InvalidHandle => -3,
        }
    }
}
