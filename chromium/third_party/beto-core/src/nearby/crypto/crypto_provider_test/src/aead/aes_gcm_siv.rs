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
use core::marker;

use hex_literal::hex;
use rstest_reuse::template;

pub use crate::prelude;
use crypto_provider::aead::{AeadInit, AesGcmSiv};

/// Test AES-GCM-SIV-128 encryption
pub fn aes_128_gcm_siv_test_encrypt<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcmSiv<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes128Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_siv_test.json
    // TC1
    let test_key = hex!("01000000000000000000000000000000");
    let nonce = hex!("030000000000000000000000");
    let aes = A::new(&test_key.into());
    let msg = hex!("");
    let tag = hex!("dc20e2d83f25705bb49e439eca56de25");
    let result = aes.encrypt(&msg, b"", &nonce).expect("Should succeed");
    assert_eq!(&result[..], &tag);
}

pub fn aes_128_gcm_siv_test_encrypt_detached<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcmSiv<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes128Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_siv_test.json
    // TC1
    let test_key = hex!("01000000000000000000000000000000");
    let nonce = hex!("030000000000000000000000");
    let aes = A::new(&test_key.into());
    let msg = hex!("");
    let mut buf = Vec::from(msg.as_slice());
    let tag = hex!("dc20e2d83f25705bb49e439eca56de25");
    let actual_tag: [u8; 16] = aes.encrypt_detached(&mut buf, b"", &nonce).unwrap();
    assert_eq!(&buf, &[0_u8; 0]);
    assert_eq!(&actual_tag, &tag);
}

/// Test AES-GCM-SIV-128 decryption
pub fn aes_128_gcm_siv_test_decrypt<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcmSiv<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes128Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_siv_test.json
    // TC2
    let test_key = hex!("01000000000000000000000000000000");
    let nonce = hex!("030000000000000000000000");
    let aes = A::new(&test_key.into());
    let msg = hex!("0100000000000000");
    let ct = hex!("b5d839330ac7b786");
    let tag = hex!("578782fff6013b815b287c22493a364c");
    let result = aes.encrypt(&msg, b"", &nonce).expect("should succeed");
    assert_eq!(&result[..8], &ct);
    assert_eq!(&result[8..], &tag);
    assert_eq!(A::TAG_SIZE, result[8..].len());
    let result = aes.decrypt(&result[..], b"", &nonce).expect("should succeed");
    assert_eq!(&result[..], &msg);
}

/// Test AES-GCM-SIV-128 decryption
pub fn aes_128_gcm_siv_test_decrypt_detached<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcmSiv<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes128Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_siv_test.json
    // TC2
    let test_key = hex!("01000000000000000000000000000000");
    let nonce = hex!("030000000000000000000000");
    let aes = A::new(&test_key.into());
    let msg = hex!("0100000000000000");
    let ct = hex!("b5d839330ac7b786");
    let tag = hex!("578782fff6013b815b287c22493a364c");
    let mut buf = Vec::from(msg.as_slice());
    let actual_tag = aes.encrypt_detached(&mut buf, b"", &nonce).unwrap();
    assert_eq!(&buf, &ct);
    assert_eq!(actual_tag, tag);
    assert!(aes.decrypt_detached(&mut buf, b"", &nonce, &tag).is_ok());
    assert_eq!(&buf[..], &msg);
}

/// Test AES-GCM-SIV-128 decryption where the tag given to decryption doesn't match
pub fn aes_128_gcm_siv_test_decrypt_detached_bad_tag<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcmSiv<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes128Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_siv_test.json
    // TC45
    let test_key = hex!("00112233445566778899aabbccddeeff");
    let nonce = hex!("000000000000000000000000");
    let aad = hex!("9ea3371e258288d5a01b15384e2c99ee");
    let aes = A::new(&test_key.into());
    // Use a longer ciphertext as the test case to make sure it's not unchanged only for the most
    // recent block.
    let ct = hex!(
        "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
    );
    let bad_tag = hex!("13a1883272188b4c8d2727178198fe95");
    let mut buf = Vec::from(ct.as_slice());
    aes.decrypt_detached(&mut buf, &aad, &nonce, &bad_tag).expect_err("Decryption should fail");
    assert_eq!(&buf, &ct); // Buffer should be unchanged if decryption failed
}

/// Test AES-256-GCM-SIV encryption/decryption
pub fn aes_256_gcm_siv_test_tc77<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcmSiv<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes256Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_siv_test.json
    // TC77
    let test_key = hex!("0100000000000000000000000000000000000000000000000000000000000000");
    let nonce = hex!("030000000000000000000000");
    let aes = A::new(&test_key.into());
    let msg = hex!("0100000000000000");
    let ct = hex!("c2ef328e5c71c83b");
    let tag = hex!("843122130f7364b761e0b97427e3df28");
    let result = aes.encrypt(&msg, b"", &nonce).expect("should succeed");
    assert_eq!(&result[..8], &ct);
    assert_eq!(&result[8..], &tag);
    assert_eq!(A::TAG_SIZE, result[8..].len());
    let result = aes.decrypt(&result[..], b"", &nonce).expect("should succeed");
    assert_eq!(&result[..], &msg);
}

/// Test AES-256-GCM-SIV encryption/decryption
pub fn aes_256_gcm_siv_test_tc77_detached<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcmSiv<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes256Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_siv_test.json
    // TC77
    let test_key = hex!("0100000000000000000000000000000000000000000000000000000000000000");
    let nonce = hex!("030000000000000000000000");
    let aes = A::new(&test_key.into());
    let msg = hex!("0100000000000000");
    let mut buf = Vec::new();
    buf.extend_from_slice(&msg);
    let ct = hex!("c2ef328e5c71c83b");
    let tag = hex!("843122130f7364b761e0b97427e3df28");
    let actual_tag = aes.encrypt_detached(&mut buf, b"", &nonce).unwrap();
    assert_eq!(&buf, &ct);
    assert_eq!(&actual_tag, &tag);
    assert_eq!(A::TAG_SIZE, tag.len());
    assert!(aes.decrypt_detached(&mut buf, b"", &nonce, &actual_tag).is_ok());
    assert_eq!(&buf, &msg);
}

/// Test AES-256-GCM-SIV encryption/decryption
pub fn aes_256_gcm_siv_test_tc78<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcmSiv<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes256Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_siv_test.json
    // TC78
    let test_key = hex!("0100000000000000000000000000000000000000000000000000000000000000");
    let nonce = hex!("030000000000000000000000");
    let aes = A::new(&test_key.into());
    let msg = hex!("010000000000000000000000");
    let ct = hex!("9aab2aeb3faa0a34aea8e2b1");
    let tag = hex!("8ca50da9ae6559e48fd10f6e5c9ca17e");
    let result = aes.encrypt(&msg, b"", &nonce).expect("should succeed");
    assert_eq!(&result[..12], &ct);
    assert_eq!(&result[12..], &tag);
    let result = aes.decrypt(&result[..], b"", &nonce).expect("should succeed");
    assert_eq!(&result[..], &msg);
}

/// Test AES-256-GCM-SIV encryption/decryption
pub fn aes_256_gcm_siv_test_tc78_detached<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcmSiv<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes256Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_siv_test.json
    // TC78
    let test_key = hex!("0100000000000000000000000000000000000000000000000000000000000000");
    let nonce = hex!("030000000000000000000000");
    let aes = A::new(&test_key.into());
    let msg = hex!("010000000000000000000000");
    let ct = hex!("9aab2aeb3faa0a34aea8e2b1");
    let tag = hex!("8ca50da9ae6559e48fd10f6e5c9ca17e");
    let mut buf = Vec::from(msg.as_slice());
    let actual_tag = aes.encrypt_detached(&mut buf, b"", &nonce).unwrap();
    assert_eq!(&buf, &ct);
    assert_eq!(&actual_tag, &tag);
    assert!(aes.decrypt_detached(&mut buf, b"", &nonce, &tag).is_ok());
    assert_eq!(&buf, &msg);
}

/// Test AES-256-GCM-SIV encryption/decryption where the tag given to decryption doesn't match.
pub fn aes_256_gcm_siv_test_decrypt_detached_bad_tag<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcmSiv<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes256Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_siv_test.json
    // TC122
    let test_key = hex!("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");
    let nonce = hex!("000000000000000000000000");
    let aes = A::new(&test_key.into());
    let aad = hex!("0289eaa93eb084107d2088435ef2a0cd");
    let ct = hex!("ffffffffffffffff");
    let bad_tag = hex!("ffffffffffffffffffffffffffffffff");
    let mut buf = Vec::from(ct.as_slice());
    aes.decrypt_detached(&mut buf, &aad, &nonce, &bad_tag)
        .expect_err("Decrypting with bad tag should fail");
    assert_eq!(&buf, &ct); // The buffer should be unchanged if the decryption failed
}

/// Generates the test cases to validate the AES-128-GCM-SIV implementation.
/// For example, to test `MyAesGcmSiv128Impl`:
///
/// ```
/// use crypto_provider::aes::aes_gcm_siv::testing::*;
///
/// mod tests {
///     #[apply(aes_128_gcm_siv_test_cases)]
///     fn aes_128_gcm_siv_tests(testcase: CryptoProviderTestCase<MyAesGcmSivImpl>) {
///         testcase(MyAesGcmSiv128Impl);
///     }
/// }
/// ```
#[template]
#[export]
#[rstest]
#[case::encrypt(aes_128_gcm_siv_test_encrypt)]
#[case::decrypt(aes_128_gcm_siv_test_decrypt)]
fn aes_128_gcm_siv_test_cases<F: AesGcmSivFactory<Key = Aes128Key>>(
    #[case] testcase: CryptoProviderTestCase<F>,
) {
}

/// Generates the test cases to validate the AES-128-GCM-SIV implementation.
/// For example, to test `MyAesGcmSiv128Impl`:
///
/// ```
/// use crypto_provider::aes::aes_gcm_siv::testing::*;
///
/// mod tests {
///     #[apply(aes_128_gcm_siv_test_cases_detached)]
///     fn aes_128_gcm_siv_tests(testcase: CryptoProviderTestCase<MyAesGcmSivImpl>) {
///         testcase(MyAesGcmSiv128Impl);
///     }
/// }
/// ```
#[template]
#[export]
#[rstest]
#[case::encrypt_detached(aes_128_gcm_siv_test_encrypt_detached)]
#[case::decrypt_detached(aes_128_gcm_siv_test_decrypt_detached)]
#[case::decrypt_detached_bad_tag(aes_128_gcm_siv_test_decrypt_detached_bad_tag)]
fn aes_128_gcm_siv_test_cases_detached<F: AesGcmSivFactory<Key = Aes128Key>>(
    #[case] testcase: CryptoProviderTestCase<F>,
) {
}

/// Generates the test cases to validate the AES-256-GCM-SIV implementation.
/// For example, to test `MyAesGcmSiv256Impl`:
///
/// ```
/// use crypto_provider::aes::aes_gcm_siv::testing::*;
///
/// mod tests {
///     #[apply(aes_256_gcm_siv_test_cases)]
///     fn aes_256_gcm_siv_tests(testcase: CryptoProviderTestCase<MyAesGcmSiv256Impl>) {
///         testcase(MyAesGcmSiv256Impl);
///     }
/// }
/// ```
#[template]
#[export]
#[rstest]
#[case::tc77(aes_256_gcm_siv_test_tc77)]
#[case::tc78(aes_256_gcm_siv_test_tc78)]
fn aes_256_gcm_siv_test_cases<F: AesGcmSivFactory<Key = Aes256Key>>(
    #[case] testcase: CryptoProviderTestCase<F>,
) {
}

/// Generates the test cases to validate the AES-256-GCM-SIV implementation.
/// For example, to test `MyAesGcmSiv256Impl`:
///
/// ```
/// use crypto_provider::aes::aes_gcm_siv::testing::*;
///
/// mod tests {
///     #[apply(aes_256_gcm_siv_test_cases_detached)]
///     fn aes_256_gcm_siv_tests(testcase: CryptoProviderTestCase<MyAesGcmSiv256Impl>) {
///         testcase(MyAesGcmSiv256Impl);
///     }
/// }
/// ```
#[template]
#[export]
#[rstest]
#[case::tc77_detached(aes_256_gcm_siv_test_tc77_detached)]
#[case::tc78_detached(aes_256_gcm_siv_test_tc78_detached)]
#[case::decrypt_detached_bad_tag(aes_256_gcm_siv_test_decrypt_detached_bad_tag)]
fn aes_256_gcm_siv_test_cases_detached<F: AesGcmSivFactory<Key = Aes256Key>>(
    #[case] testcase: CryptoProviderTestCase<F>,
) {
}
