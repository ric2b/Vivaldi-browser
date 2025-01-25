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
use crypto_provider::aead::{AeadInit, AesGcm};

/// Test AES-GCM-128 encryption
pub fn aes_128_gcm_test_encrypt<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcm<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes128Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_test.json
    // TC4
    let test_key = hex!("bedcfb5a011ebc84600fcb296c15af0d");
    let nonce = hex!("438a547a94ea88dce46c6c85");
    let aes = A::new(&test_key.into());
    let msg = hex!("");
    let tag = hex!("960247ba5cde02e41a313c4c0136edc3");
    let result = aes.encrypt(&msg, b"", &nonce).expect("Should succeed");
    assert_eq!(&result[..], &tag);
}

pub fn aes_128_gcm_test_encrypt_detached<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcm<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes128Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_test.json
    // TC4
    let test_key = hex!("bedcfb5a011ebc84600fcb296c15af0d");
    let nonce = hex!("438a547a94ea88dce46c6c85");
    let aes = A::new(&test_key.into());
    let msg = hex!("");
    let tag = hex!("960247ba5cde02e41a313c4c0136edc3");
    let mut buf = Vec::from(msg.as_slice());
    let actual_tag: [u8; 16] = aes.encrypt_detached(&mut buf, b"", &nonce).unwrap();
    assert_eq!(&buf, &[0_u8; 0]);
    assert_eq!(&actual_tag, &tag);
}

/// Test AES-GCM-128 decryption
pub fn aes_128_gcm_test_decrypt<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcm<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes128Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_test.json
    // TC2
    let test_key = hex!("5b9604fe14eadba931b0ccf34843dab9");
    let nonce = hex!("921d2507fa8007b7bd067d34");
    let aes = A::new(&test_key.into());
    let msg = hex!("001d0c231287c1182784554ca3a21908");
    let ct = hex!("49d8b9783e911913d87094d1f63cc765");
    let ad = hex!("00112233445566778899aabbccddeeff");
    let tag = hex!("1e348ba07cca2cf04c618cb4d43a5b92");
    let result = aes.encrypt(&msg, &ad, &nonce).expect("should succeed");
    assert_eq!(&result[..16], &ct);
    assert_eq!(&result[16..], &tag);
    assert_eq!(A::TAG_SIZE, result[16..].len());
    let result = aes.decrypt(&result[..], &ad, &nonce).expect("should succeed");
    assert_eq!(&result[..], &msg);
}

/// Test AES-GCM-128 decryption
pub fn aes_128_gcm_test_decrypt_detached<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcm<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes128Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_test.json
    // TC2
    let test_key = hex!("5b9604fe14eadba931b0ccf34843dab9");
    let nonce = hex!("921d2507fa8007b7bd067d34");
    let aes = A::new(&test_key.into());
    let msg = hex!("001d0c231287c1182784554ca3a21908");
    let ct = hex!("49d8b9783e911913d87094d1f63cc765");
    let ad = hex!("00112233445566778899aabbccddeeff");
    let tag = hex!("1e348ba07cca2cf04c618cb4d43a5b92");
    let mut buf = Vec::from(msg.as_slice());
    let actual_tag = aes.encrypt_detached(&mut buf, &ad, &nonce).unwrap();
    assert_eq!(&buf, &ct);
    assert_eq!(actual_tag, tag);
    assert!(aes.decrypt_detached(&mut buf, &ad, &nonce, &tag).is_ok());
    assert_eq!(&buf[..], &msg);
}

/// Test AES-GCM-128 decryption
pub fn aes_128_gcm_test_decrypt_detached_bad_tag<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcm<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes128Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_test.json
    // TC23
    let test_key = hex!("000102030405060708090a0b0c0d0e0f");
    let nonce = hex!("505152535455565758595a5b");
    let aes = A::new(&test_key.into());
    let ct = hex!("eb156d081ed6b6b55f4612f021d87b39");
    let mut buf = Vec::from(ct.as_slice());
    let bad_tag = hex!("d9847dbc326a06e988c77ad3863e6083");
    aes.decrypt_detached(&mut buf, b"", &nonce, &bad_tag)
        .expect_err("decryption tag verification should fail");
    // assert that the buffer does not change if tag verification fails
    assert_eq!(buf, ct);
}

/// Test AES-256-GCM encryption/decryption
pub fn aes_256_gcm_test_tc74<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcm<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes256Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_test.json
    // TC74
    let test_key = hex!("29d3a44f8723dc640239100c365423a312934ac80239212ac3df3421a2098123");
    let nonce = hex!("00112233445566778899aabb");
    let aes = A::new(&test_key.into());
    let msg = hex!("");
    let ad = hex!("aabbccddeeff");
    let tag = hex!("2a7d77fa526b8250cb296078926b5020");
    let result = aes.encrypt(&msg, &ad, &nonce).expect("should succeed");
    assert_eq!(&result[..], &tag);
    assert_eq!(A::TAG_SIZE, result.len());
    let result = aes.decrypt(&result, &ad, &nonce).expect("should succeed");
    assert_eq!(&result[..], &msg);
}

/// Test AES-256-GCM encryption/decryption
pub fn aes_256_gcm_test_tc74_detached<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcm<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes256Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_test.json
    // TC74
    let test_key = hex!("29d3a44f8723dc640239100c365423a312934ac80239212ac3df3421a2098123");
    let nonce = hex!("00112233445566778899aabb");
    let aes = A::new(&test_key.into());
    let msg = hex!("");
    let ad = hex!("aabbccddeeff");
    let ct = hex!("");
    let tag = hex!("2a7d77fa526b8250cb296078926b5020");
    let mut buf = Vec::new();
    buf.extend_from_slice(&msg);
    let actual_tag = aes.encrypt_detached(&mut buf, &ad, &nonce).unwrap();
    assert_eq!(&buf, &ct);
    assert_eq!(&actual_tag, &tag);
    assert_eq!(A::TAG_SIZE, tag.len());
    assert!(aes.decrypt_detached(&mut buf, &ad, &nonce, &actual_tag).is_ok());
    assert_eq!(&buf, &msg);
}

/// Test AES-256-GCM encryption/decryption
pub fn aes_256_gcm_test_tc79<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcm<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes256Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_test.json
    // TC79
    let test_key = hex!("59d4eafb4de0cfc7d3db99a8f54b15d7b39f0acc8da69763b019c1699f87674a");
    let nonce = hex!("2fcb1b38a99e71b84740ad9b");
    let aes = A::new(&test_key.into());
    let msg = hex!("549b365af913f3b081131ccb6b825588");
    let ct = hex!("f58c16690122d75356907fd96b570fca");
    let tag = hex!("28752c20153092818faba2a334640d6e");
    let result = aes.encrypt(&msg, b"", &nonce).expect("should succeed");
    assert_eq!(&result[..16], &ct);
    assert_eq!(&result[16..], &tag);
    let result = aes.decrypt(&result[..], b"", &nonce).expect("should succeed");
    assert_eq!(&result[..], &msg);
}

/// Test AES-256-GCM encryption/decryption
pub fn aes_256_gcm_test_tc79_detached<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcm<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes256Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_test.json
    // TC79
    let test_key = hex!("59d4eafb4de0cfc7d3db99a8f54b15d7b39f0acc8da69763b019c1699f87674a");
    let nonce = hex!("2fcb1b38a99e71b84740ad9b");
    let aes = A::new(&test_key.into());
    let msg = hex!("549b365af913f3b081131ccb6b825588");
    let ct = hex!("f58c16690122d75356907fd96b570fca");
    let tag = hex!("28752c20153092818faba2a334640d6e");
    let mut buf = Vec::from(msg.as_slice());
    let actual_tag = aes.encrypt_detached(&mut buf, b"", &nonce).unwrap();
    assert_eq!(&buf, &ct);
    assert_eq!(&actual_tag, &tag);
    assert!(aes.decrypt_detached(&mut buf, b"", &nonce, &tag).is_ok());
    assert_eq!(&buf, &msg);
}

/// Test AES-256-GCM encryption/decryption where the tag given to decryption doesn't match.
pub fn aes_256_gcm_test_decrypt_detached_bad_tag<A>(_marker: marker::PhantomData<A>)
where
    A: AesGcm<Tag = [u8; 16]> + AeadInit<crypto_provider::aes::Aes256Key>,
{
    // https://github.com/google/wycheproof/blob/master/testvectors/aes_gcm_test.json
    // TC94
    let test_key = hex!("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    let nonce = hex!("505152535455565758595a5b");
    let aes = A::new(&test_key.into());
    let aad = hex!("");
    let ct = hex!("b2061457c0759fc1749f174ee1ccadfa");
    let bad_tag = hex!("9de8fef6d8ab1bf1bf887232eab590dd");
    let mut buf = Vec::from(ct.as_slice());
    aes.decrypt_detached(&mut buf, &aad, &nonce, &bad_tag)
        .expect_err("Decrypting with bad tag should fail");
    // assert that the buffer does not change if tag verification fails
    assert_eq!(buf, ct);
}

/// Generates the test cases to validate the AES-128-GCM implementation.
/// For example, to test `MyAesGcm128Impl`:
///
/// ```
/// use crypto_provider::aes::aes_gcm::testing::*;
///
/// mod tests {
///     #[apply(aes_128_gcm_test_cases)]
///     fn aes_128_gcm_tests(testcase: CryptoProviderTestCase<MyAesGcmImpl>) {
///         testcase(MyAesGcm128Impl);
///     }
/// }
/// ```
#[template]
#[export]
#[rstest]
#[case::encrypt(aes_128_gcm_test_encrypt)]
#[case::decrypt(aes_128_gcm_test_decrypt)]
fn aes_128_gcm_test_cases<F: AesGcmFactory<Key = Aes128Key>>(
    #[case] testcase: CryptoProviderTestCase<F>,
) {
}

/// Generates the test cases to validate the AES-128-GCM implementation.
/// For example, to test `MyAesGcm128Impl`:
///
/// ```
/// use crypto_provider::aes::aes_gcm::testing::*;
///
/// mod tests {
///     #[apply(aes_128_gcm_test_cases_detached)]
///     fn aes_128_gcm_tests(testcase: CryptoProviderTestCase<MyAesGcmImpl>) {
///         testcase(MyAesGcm128Impl);
///     }
/// }
/// ```
#[template]
#[export]
#[rstest]
#[case::encrypt_detached(aes_128_gcm_test_encrypt_detached)]
#[case::decrypt_detached(aes_128_gcm_test_decrypt_detached)]
#[case::decrypt_detached_bad_tag(aes_128_gcm_test_decrypt_detached_bad_tag)]
fn aes_128_gcm_test_cases_detached<F: AesGcmFactory<Key = Aes128Key>>(
    #[case] testcase: CryptoProviderTestCase<F>,
) {
}

/// Generates the test cases to validate the AES-256-GCM implementation.
/// For example, to test `MyAesGcm256Impl`:
///
/// ```
/// use crypto_provider::aes::aes_gcm::testing::*;
///
/// mod tests {
///     #[apply(aes_256_gcm_test_cases)]
///     fn aes_256_gcm_tests(testcase: CryptoProviderTestCase<MyAesGcm256Impl>) {
///         testcase(MyAesGcm256Impl);
///     }
/// }
/// ```
#[template]
#[export]
#[rstest]
#[case::tc74(aes_256_gcm_test_tc74)]
#[case::tc79(aes_256_gcm_test_tc79)]
fn aes_256_gcm_test_cases<F: AesGcmFactory<Key = Aes256Key>>(
    #[case] testcase: CryptoProviderTestCase<F>,
) {
}

/// Generates the test cases to validate the AES-256-GCM implementation.
/// For example, to test `MyAesGcm256Impl`:
///
/// ```
/// use crypto_provider::aes::aes_gcm::testing::*;
///
/// mod tests {
///     #[apply(aes_256_gcm_test_cases_detached)]
///     fn aes_256_gcm_tests(testcase: CryptoProviderTestCase<MyAesGcm256Impl>) {
///         testcase(MyAesGcm256Impl);
///     }
/// }
/// ```
#[template]
#[export]
#[rstest]
#[case::tc74_detached(aes_256_gcm_test_tc74_detached)]
#[case::tc79_detached(aes_256_gcm_test_tc79_detached)]
#[case::decrypt_detached_bad_tag(aes_256_gcm_test_decrypt_detached_bad_tag)]
fn aes_256_gcm_test_cases_detached<F: AesGcmFactory<Key = Aes256Key>>(
    #[case] testcase: CryptoProviderTestCase<F>,
) {
}
