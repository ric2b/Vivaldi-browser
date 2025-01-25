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

#![allow(clippy::unwrap_used, clippy::expect_used, clippy::indexing_slicing)]

use crypto_provider::CryptoProvider;
use crypto_provider_default::CryptoProviderImpl;
use ldt_tbc::TweakableBlockCipherDecrypter;
use ldt_tbc::TweakableBlockCipherEncrypter;
use ldt_tbc::TweakableBlockCipherKey;
use wycheproof::cipher::TestGroup;
use xts_aes::{XtsAes128Key, XtsAes256Key, XtsDecrypter, XtsEncrypter, XtsKey};

#[test]
fn run_wycheproof_vectors() {
    run_test_vectors();
}

fn run_test_vectors() {
    let test_set = wycheproof::cipher::TestSet::load(wycheproof::cipher::TestName::AesXts)
        .expect("Should be able to load test set");

    for test_group in test_set.test_groups {
        match test_group.key_size {
            256 => run_tests::<XtsAes128Key, <CryptoProviderImpl as CryptoProvider>::Aes128>(
                test_group,
            ),
            512 => run_tests::<XtsAes256Key, <CryptoProviderImpl as CryptoProvider>::Aes256>(
                test_group,
            ),
            _ => {}
        }
    }
}

fn run_tests<K, A>(test_group: TestGroup)
where
    K: XtsKey + TweakableBlockCipherKey,
    A: crypto_provider::aes::Aes<Key = K::BlockCipherKey>,
{
    let mut buf = Vec::new();
    for test in test_group.tests {
        buf.clear();
        let key = test.key.as_slice();
        let iv = test.nonce.as_slice();
        let msg = test.pt.as_slice();
        let ct = test.ct.as_slice();

        let mut block = [0u8; 16];
        block[..iv.len()].copy_from_slice(iv);
        let tweak = xts_aes::Tweak::from(block);

        // test encryption
        let xts_enc = XtsEncrypter::<A, _>::new(&K::try_from(key).unwrap());
        buf.extend_from_slice(msg);
        xts_enc.encrypt_data_unit(tweak.clone(), &mut buf).unwrap();
        assert_eq!(ct, buf, "Test case id: {}", test.tc_id);

        // test decryption
        buf.clear();
        let xts_dec = XtsDecrypter::<A, _>::new(&K::try_from(key).unwrap());
        buf.extend_from_slice(ct);
        xts_dec.decrypt_data_unit(tweak, &mut buf).unwrap();
        assert_eq!(msg, buf, "Test case id: {}", test.tc_id);
    }
}
