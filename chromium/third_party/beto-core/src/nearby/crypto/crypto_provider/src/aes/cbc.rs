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

//! Traits for AES-CBC 256 with PKCS7 padding.

#[cfg(feature = "alloc")]
extern crate alloc;
use crate::tinyvec::SliceVec;
#[cfg(feature = "alloc")]
use alloc::vec::Vec;

use super::Aes256Key;

/// Type of the initialization vector for AES-CBC
pub type AesCbcIv = [u8; 16];

/// Trait for implementing AES-CBC with PKCS7 padding.
pub trait AesCbcPkcs7Padded {
    /// Calculate the padded output length (e.g. output of `encrypt`) from the unpadded length (e.g.
    /// input message of `encrypt`).
    #[allow(clippy::expect_used)]
    fn padded_output_len(unpadded_len: usize) -> usize {
        (unpadded_len - (unpadded_len % 16))
            .checked_add(16)
            .expect("Padded output length is larger than usize::MAX")
    }

    /// Encrypt message using `key` and `iv`, returning a ciphertext.
    #[cfg(feature = "alloc")]
    fn encrypt(key: &Aes256Key, iv: &AesCbcIv, message: &[u8]) -> Vec<u8>;

    /// Encrypt message using `key` and `iv` in-place in the given `message` vec. The given slice
    /// vec should have enough capacity to contain both the ciphertext and the padding (which can be
    /// calculated from `padded_output_len()`). If it doesn't have enough capacity, an error will be
    /// returned. The contents of the input `message` buffer is undefined in that case.
    fn encrypt_in_place(
        key: &Aes256Key,
        iv: &AesCbcIv,
        message: &mut SliceVec<u8>,
    ) -> Result<(), EncryptionError>;

    /// Decrypt ciphertext using `key` and `iv`, returning the original message if `Ok()` otherwise
    /// a `DecryptionError` indicating the type of error that occurred while decrypting.
    #[cfg(feature = "alloc")]
    fn decrypt(
        key: &Aes256Key,
        iv: &AesCbcIv,
        ciphertext: &[u8],
    ) -> Result<Vec<u8>, DecryptionError>;

    /// Decrypt ciphertext using `key` and `iv` and unpad it in-place. Returning the original
    /// message if `Ok()` otherwise a `DecryptionError` indicating the type of error that occurred
    /// while decrypting. In that case, the contents of the `ciphertext` buffer is undefined.
    fn decrypt_in_place(
        key: &Aes256Key,
        iv: &AesCbcIv,
        ciphertext: &mut SliceVec<u8>,
    ) -> Result<(), DecryptionError>;
}

/// Error type for describing what went wrong encrypting a message.
#[derive(Debug, PartialEq, Eq)]
pub enum EncryptionError {
    /// Failed to add PKCS7 padding to the output when encrypting a message. This typically happens
    /// when the given output buffer does not have enough capacity to append the padding.
    PaddingFailed,
}

/// Error type for describing what went wrong decrypting a ciphertext.
#[derive(Debug, PartialEq, Eq)]
pub enum DecryptionError {
    /// Invalid padding, the input ciphertext does not have valid PKCS7 padding. If you get this
    /// error, check the encryption side generating this data to make sure it is adding the padding
    /// correctly. Exposing padding errors can cause a padding oracle vulnerability.
    BadPadding,
}

#[cfg(test)]
mod test {
    #[cfg(feature = "alloc")]
    extern crate alloc;
    #[cfg(feature = "alloc")]
    use alloc::vec::Vec;

    use crate::aes::Aes256Key;
    use crate::tinyvec::SliceVec;

    use super::{AesCbcIv, AesCbcPkcs7Padded, DecryptionError, EncryptionError};

    #[test]
    fn test_padded_output_len() {
        assert_eq!(AesCbcPkcs7PaddedStub::padded_output_len(0), 16);
        assert_eq!(AesCbcPkcs7PaddedStub::padded_output_len(15), 16);
        assert_eq!(AesCbcPkcs7PaddedStub::padded_output_len(16), 32);
        assert_eq!(AesCbcPkcs7PaddedStub::padded_output_len(30), 32);
        assert_eq!(AesCbcPkcs7PaddedStub::padded_output_len(32), 48);
    }

    #[test]
    #[should_panic]
    fn test_padded_output_len_overflow() {
        let _ = AesCbcPkcs7PaddedStub::padded_output_len(usize::MAX);
    }

    struct AesCbcPkcs7PaddedStub;

    impl AesCbcPkcs7Padded for AesCbcPkcs7PaddedStub {
        #[cfg(feature = "alloc")]
        fn encrypt(_key: &Aes256Key, _iv: &AesCbcIv, _message: &[u8]) -> Vec<u8> {
            unimplemented!()
        }

        fn encrypt_in_place(
            _key: &Aes256Key,
            _iv: &AesCbcIv,
            _message: &mut SliceVec<u8>,
        ) -> Result<(), EncryptionError> {
            unimplemented!()
        }

        #[cfg(feature = "alloc")]
        fn decrypt(
            _key: &Aes256Key,
            _iv: &AesCbcIv,
            _ciphertext: &[u8],
        ) -> Result<Vec<u8>, DecryptionError> {
            unimplemented!()
        }

        fn decrypt_in_place(
            _key: &Aes256Key,
            _iv: &AesCbcIv,
            _ciphertext: &mut SliceVec<u8>,
        ) -> Result<(), DecryptionError> {
            unimplemented!()
        }
    }
}
