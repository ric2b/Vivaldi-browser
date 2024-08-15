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

use crypto_provider::aes::{Aes, BLOCK_SIZE};
use crypto_provider::CryptoProvider;
use crypto_provider_default::CryptoProviderImpl;
use ldt_tbc::{TweakableBlockCipherDecrypter, TweakableBlockCipherEncrypter};
use xts_aes::{XtsAes128Key, XtsAes256Key, XtsDecrypter, XtsEncrypter, XtsError, XtsKey};

const MAX_XTS_SIZE: usize = (1 << 20) * BLOCK_SIZE;

#[test]
fn too_small_payload() {
    let key = [0_u8; 32];
    let (enc, dec) = build_ciphers::<<CryptoProviderImpl as CryptoProvider>::Aes128, _>(
        &XtsAes128Key::from(&key),
    );
    let tweak = [0u8; 16];
    let mut payload = [0u8; BLOCK_SIZE - 1];
    assert_eq!(enc.encrypt_data_unit(tweak.into(), &mut payload), Err(XtsError::DataTooShort));
    assert_eq!(dec.decrypt_data_unit(tweak.into(), &mut payload), Err(XtsError::DataTooShort));

    let key = [0u8; 64];
    let (enc, dec) = build_ciphers::<<CryptoProviderImpl as CryptoProvider>::Aes256, _>(
        &XtsAes256Key::from(&key),
    );
    assert_eq!(enc.encrypt_data_unit(tweak.into(), &mut payload), Err(XtsError::DataTooShort));
    assert_eq!(dec.decrypt_data_unit(tweak.into(), &mut payload), Err(XtsError::DataTooShort));
}

#[test]
fn block_size_payload() {
    let key = [0_u8; 32];
    let (enc, dec) = build_ciphers::<<CryptoProviderImpl as CryptoProvider>::Aes128, _>(
        &XtsAes128Key::from(&key),
    );
    let tweak = [0u8; 16];
    let mut payload = [0u8; BLOCK_SIZE];
    assert!(enc.encrypt_data_unit(tweak.into(), &mut payload).is_ok());
    assert!(dec.decrypt_data_unit(tweak.into(), &mut payload).is_ok());

    let key = [0u8; 64];
    let (enc, dec) = build_ciphers::<<CryptoProviderImpl as CryptoProvider>::Aes256, _>(
        &XtsAes256Key::from(&key),
    );
    assert!(enc.encrypt_data_unit(tweak.into(), &mut payload).is_ok());
    assert!(dec.decrypt_data_unit(tweak.into(), &mut payload).is_ok());
}

#[test]
fn max_xts_sized_payload() {
    let key = [0_u8; 32];
    let (enc, dec) = build_ciphers::<<CryptoProviderImpl as CryptoProvider>::Aes128, _>(
        &XtsAes128Key::from(&key),
    );
    let tweak = [0u8; 16];
    let mut payload = vec![0u8; MAX_XTS_SIZE];
    assert!(enc.encrypt_data_unit(tweak.into(), payload.as_mut_slice()).is_ok());
    assert!(dec.decrypt_data_unit(tweak.into(), payload.as_mut_slice()).is_ok());

    let key = [0u8; 64];
    let (enc, dec) = build_ciphers::<<CryptoProviderImpl as CryptoProvider>::Aes256, _>(
        &XtsAes256Key::from(&key),
    );
    assert!(enc.encrypt_data_unit(tweak.into(), payload.as_mut_slice()).is_ok());
    assert!(dec.decrypt_data_unit(tweak.into(), payload.as_mut_slice()).is_ok());
}

#[test]
fn too_large_payload() {
    let key = [0_u8; 32];
    let (enc, dec) = build_ciphers::<<CryptoProviderImpl as CryptoProvider>::Aes128, _>(
        &XtsAes128Key::from(&key),
    );
    let tweak = [0u8; 16];
    let mut payload = vec![0u8; MAX_XTS_SIZE + 1];
    assert_eq!(
        enc.encrypt_data_unit(tweak.into(), payload.as_mut_slice()),
        Err(XtsError::DataTooLong)
    );
    assert_eq!(
        dec.decrypt_data_unit(tweak.into(), payload.as_mut_slice()),
        Err(XtsError::DataTooLong)
    );

    let key = [0u8; 64];
    let (enc, dec) = build_ciphers::<<CryptoProviderImpl as CryptoProvider>::Aes256, _>(
        &XtsAes256Key::from(&key),
    );
    assert_eq!(
        enc.encrypt_data_unit(tweak.into(), payload.as_mut_slice()),
        Err(XtsError::DataTooLong)
    );
    assert_eq!(
        dec.decrypt_data_unit(tweak.into(), payload.as_mut_slice()),
        Err(XtsError::DataTooLong)
    );
}

fn build_ciphers<A: Aes<Key = K::BlockCipherKey>, K: XtsKey + ldt_tbc::TweakableBlockCipherKey>(
    key: &K,
) -> (XtsEncrypter<A, K>, XtsDecrypter<A, K>) {
    let enc = XtsEncrypter::<A, _>::new(key);
    let dec = XtsDecrypter::<A, _>::new(key);
    (enc, dec)
}
