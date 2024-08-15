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

use crate::aes::Aes256Key;
pub use crate::prelude::*;
use core::marker::PhantomData;
use crypto_provider::{
    aes::cbc::{AesCbcIv, AesCbcPkcs7Padded},
    tinyvec::SliceVec,
};
use hex_literal::hex;
use rstest_reuse::template;

/// Tests for AES-256-CBC encryption
pub fn aes_256_cbc_test_encrypt<A: AesCbcPkcs7Padded>(_marker: PhantomData<A>) {
    // https://github.com/google/wycheproof/blob/b063b4a/testvectors/aes_cbc_pkcs5_test.json#L1492
    // tcId: 132
    let key: Aes256Key =
        hex!("665a02bc265a66d01775091da56726b6668bfd903cb7af66fb1b78a8a062e43c").into();
    let iv: AesCbcIv = hex!("3fb0d5ecd06c71150748b599595833cb");
    let msg = hex!("3f56935def3f");
    let expected_ciphertext = hex!("3f3f39697bd7e88d85a14132be1cbc48");
    assert_eq!(A::encrypt(&key, &iv, &msg), expected_ciphertext);
}

/// Tests for AES-256-CBC in-place encryption
pub fn aes_256_cbc_test_encrypt_in_place<A: AesCbcPkcs7Padded>(_marker: PhantomData<A>) {
    // https://github.com/google/wycheproof/blob/b063b4a/testvectors/aes_cbc_pkcs5_test.json#L1492
    // tcId: 132
    let key: Aes256Key =
        hex!("665a02bc265a66d01775091da56726b6668bfd903cb7af66fb1b78a8a062e43c").into();
    let iv: AesCbcIv = hex!("3fb0d5ecd06c71150748b599595833cb");
    let msg = hex!("3f56935def3f");
    let expected_ciphertext = hex!("3f3f39697bd7e88d85a14132be1cbc48");
    let mut msg_buffer_backing = [0_u8; 16];
    let mut msg_buffer = SliceVec::from_slice_len(&mut msg_buffer_backing, 0);
    msg_buffer.extend_from_slice(&msg);
    A::encrypt_in_place(&key, &iv, &mut msg_buffer).unwrap();
    assert_eq!(msg_buffer.as_slice(), &expected_ciphertext);
}

/// Tests for AES-256-CBC encryption, where the given buffer `SliceVec` is too short to contain the
/// output.
pub fn aes_256_cbc_test_encrypt_in_place_too_short<A: AesCbcPkcs7Padded>(_marker: PhantomData<A>) {
    // https://github.com/google/wycheproof/blob/b063b4a/testvectors/aes_cbc_pkcs5_test.json#L1612
    // tcId: 144
    let key: Aes256Key =
        hex!("4f097858a1aec62cf18f0966b2b120783aa4ae9149d3213109740506ae47adfe").into();
    let iv: AesCbcIv = hex!("400aab92803bcbb44a96ef789655b34e");
    let msg = hex!("ee53d8e5039e82d9fcca114e375a014febfea117a7e709d9008d43858e3660");
    let mut msg_buffer_backing = [0_u8; 31];
    let mut msg_buffer = SliceVec::from_slice_len(&mut msg_buffer_backing, 0);
    msg_buffer.extend_from_slice(&msg);
    A::encrypt_in_place(&key, &iv, &mut msg_buffer)
        .expect_err("Encrypting AES with 15-byte buffer should fail");
    // Buffer content is undefined, but test to make sure it doesn't contain half-decrypted data
    assert!(
        msg_buffer.as_slice() == [0_u8; 32] || msg_buffer.as_slice() == msg,
        "Unrecognized content in buffer after decryption failure"
    )
}

/// Tests for AES-256-CBC decryption
pub fn aes_256_cbc_test_decrypt<A: AesCbcPkcs7Padded>(_marker: PhantomData<A>) {
    // https://github.com/google/wycheproof/blob/b063b4a/testvectors/aes_cbc_pkcs5_test.json#L1492
    // tcId: 132
    let key: Aes256Key =
        hex!("665a02bc265a66d01775091da56726b6668bfd903cb7af66fb1b78a8a062e43c").into();
    let iv: AesCbcIv = hex!("3fb0d5ecd06c71150748b599595833cb");
    let ciphertext = hex!("3f3f39697bd7e88d85a14132be1cbc48");
    let expected_msg = hex!("3f56935def3f");
    assert_eq!(A::decrypt(&key, &iv, &ciphertext).unwrap(), expected_msg);
}

/// Tests for AES-256-CBC decryption with bad padding
pub fn aes_256_cbc_test_decrypt_bad_padding<A: AesCbcPkcs7Padded>(_marker: PhantomData<A>) {
    // https://github.com/google/wycheproof/blob/b063b4a/testvectors/aes_cbc_pkcs5_test.json#L1690
    // tcId: 151
    let key: Aes256Key =
        hex!("7c78f34dbce8f0557d43630266f59babd1cb92ba624bd1a8f45a2a91c84a804a").into();
    let iv: AesCbcIv = hex!("f010f61c31c9aa8fa0d5be5f6b0f2f70");
    let ciphertext = hex!("8881e9e02fa9e3037b397957ba1fb7ce64679a46621b792f643542a735f0bbbf");
    A::decrypt(&key, &iv, &ciphertext).expect_err("Decryption with bad padding should fail");
}

/// Tests for AES-256-CBC in-place decryption
pub fn aes_256_cbc_test_decrypt_in_place<A: AesCbcPkcs7Padded>(_marker: PhantomData<A>) {
    // https://github.com/google/wycheproof/blob/b063b4a/testvectors/aes_cbc_pkcs5_test.json#L1492
    // tcId: 132
    let key: Aes256Key =
        hex!("665a02bc265a66d01775091da56726b6668bfd903cb7af66fb1b78a8a062e43c").into();
    let iv: AesCbcIv = hex!("3fb0d5ecd06c71150748b599595833cb");
    let mut ciphertext = hex!("3f3f39697bd7e88d85a14132be1cbc48");
    let expected_msg = hex!("3f56935def3f");
    let mut msg_buffer = SliceVec::from(&mut ciphertext);
    A::decrypt_in_place(&key, &iv, &mut msg_buffer).unwrap();
    assert_eq!(msg_buffer.as_slice(), expected_msg);
}

/// Tests for AES-256-CBC in-place decryption with bad padding
pub fn aes_256_cbc_test_decrypt_in_place_bad_padding<A: AesCbcPkcs7Padded>(
    _marker: PhantomData<A>,
) {
    // https://github.com/google/wycheproof/blob/b063b4a/testvectors/aes_cbc_pkcs5_test.json#L1690
    // tcId: 151
    let key: Aes256Key =
        hex!("7c78f34dbce8f0557d43630266f59babd1cb92ba624bd1a8f45a2a91c84a804a").into();
    let iv: AesCbcIv = hex!("f010f61c31c9aa8fa0d5be5f6b0f2f70");
    let ciphertext = hex!("8881e9e02fa9e3037b397957ba1fb7ce64679a46621b792f643542a735f0bbbf");
    let mut msg_buffer_backing = ciphertext;
    let mut msg_buffer = SliceVec::from(&mut msg_buffer_backing);
    A::decrypt_in_place(&key, &iv, &mut msg_buffer)
        .expect_err("Decryption with bad padding should fail");
    // Buffer content is undefined, but test to make sure it doesn't contain half-decrypted data
    assert!(
        msg_buffer.as_slice() == [0_u8; 32] || msg_buffer.as_slice() == ciphertext,
        "Unrecognized content in buffer after decryption failure"
    )
}

/// Generates the test cases to validate the AES-256-CBC implementation.
/// For example, to test `MyAesCbc256Impl`:
///
/// ```
/// use crypto_provider::aes::cbc::testing::*;
///
/// mod tests {
///     #[apply(aes_256_cbc_test_cases)]
///     fn aes_256_cbc_tests(
///             testcase: CryptoProviderTestCases<PhantomData<MyAesCbc256Impl>>) {
///         testcase(PhantomData);
///     }
/// }
/// ```
#[template]
#[export]
#[rstest]
#[case::encrypt(aes_256_cbc_test_encrypt)]
#[case::encrypt_in_place(aes_256_cbc_test_encrypt_in_place)]
#[case::encrypt_in_place_too_short(aes_256_cbc_test_encrypt_in_place_too_short)]
#[case::decrypt(aes_256_cbc_test_decrypt)]
#[case::decrypt_bad_padding(aes_256_cbc_test_decrypt_bad_padding)]
#[case::decrypt_in_place_bad_padding(aes_256_cbc_test_decrypt_in_place_bad_padding)]
fn aes_256_cbc_test_cases<A: AesCbcPkcs7Padded>(#[case] testcase: CryptoProviderTestCases<F>) {}
