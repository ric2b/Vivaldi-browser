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
#![allow(clippy::indexing_slicing, clippy::unwrap_used, clippy::panic, clippy::expect_used)]

extern crate alloc;

use crate::{
    build_np_adv_decrypter_from_key_seed, salt_padder, AuthenticatedNpLdtDecryptCipher,
    LdtAdvDecryptError, NpLdtDecryptCipher, NpLdtEncryptCipher, V0IdentityToken, V0Salt,
    LDT_XTS_AES_MAX_LEN, V0_IDENTITY_TOKEN_LEN,
};
use alloc::vec::Vec;
use crypto_provider::{CryptoProvider, CryptoRng};
use crypto_provider_default::CryptoProviderImpl;
use ldt::{DefaultPadder, LdtCipher, LdtError, LdtKey, XorPadder};
use np_hkdf::NpKeySeedHkdf;
use rand::Rng;
use rand_ext::{random_bytes, random_vec, seeded_rng};

extern crate std;

use crypto_provider::aes::BLOCK_SIZE;
use std::vec;

#[test]
fn decrypt_matches_correct_ciphertext() {
    let mut rng = CryptoRng::new();
    for _ in 0..1_000 {
        let test_state = make_test_components::<CryptoProviderImpl>(&mut rng);

        let cipher = build_np_adv_decrypter_from_key_seed(&test_state.hkdf, test_state.hmac);
        let (token, plaintext) =
            cipher.decrypt_and_verify(&test_state.ciphertext, &test_state.padder).unwrap();

        assert_eq!(test_state.identity_token, token);
        assert_eq!(&test_state.remaining_plaintext, plaintext.as_ref());
    }
}

#[test]
fn decrypt_doesnt_match_when_ciphertext_mangled() {
    let mut rng = CryptoRng::new();
    for _ in 0..1_000 {
        let mut test_state = make_test_components::<CryptoProviderImpl>(&mut rng);

        // mangle the ciphertext
        test_state.ciphertext[0] ^= 0xAA;

        let cipher = build_np_adv_decrypter_from_key_seed(&test_state.hkdf, test_state.hmac);
        assert_eq!(
            Err(LdtAdvDecryptError::MacMismatch),
            cipher.decrypt_and_verify(&test_state.ciphertext, &test_state.padder)
        );
    }
}

#[test]
fn decrypt_doesnt_match_when_plaintext_doesnt_match_mac() {
    let mut rng = CryptoRng::new();
    for _ in 0..1_000 {
        let mut test_state = make_test_components::<CryptoProviderImpl>(&mut rng);

        // mangle the mac
        test_state.hmac[0] ^= 0xAA;

        let cipher = build_np_adv_decrypter_from_key_seed(&test_state.hkdf, test_state.hmac);
        assert_eq!(
            Err(LdtAdvDecryptError::MacMismatch),
            cipher.decrypt_and_verify(&test_state.ciphertext, &test_state.padder)
        );
    }
}

#[test]
#[allow(deprecated)]
fn encrypt_works() {
    let mut rng = CryptoRng::new();
    for _ in 0..1_000 {
        let test_state = make_test_components::<CryptoProviderImpl>(&mut rng);

        let cipher = test_state.ldt_enc;

        let mut plaintext_copy = test_state.all_plaintext.clone();
        cipher.encrypt(&mut plaintext_copy[..], &test_state.padder).unwrap();

        assert_eq!(test_state.ciphertext, plaintext_copy);
    }
}

#[test]
#[allow(deprecated)]
fn encrypt_too_short_err() {
    let ldt_enc =
        NpLdtEncryptCipher::<CryptoProviderImpl>::new(&LdtKey::from_concatenated(&[0; 64]));

    let mut payload = vec![0; BLOCK_SIZE - 1];
    assert_eq!(
        Err(LdtError::InvalidLength(BLOCK_SIZE - 1)),
        ldt_enc.encrypt(&mut payload, &DefaultPadder)
    );
}

#[test]
#[allow(deprecated)]
fn encrypt_too_long_err() {
    let ldt_enc =
        NpLdtEncryptCipher::<CryptoProviderImpl>::new(&LdtKey::from_concatenated(&[0; 64]));

    let mut payload = vec![0; BLOCK_SIZE * 2];
    assert_eq!(
        Err(LdtError::InvalidLength(BLOCK_SIZE * 2)),
        ldt_enc.encrypt(&mut payload, &DefaultPadder)
    );
}

#[test]
fn decrypt_too_short_err() {
    let adv_cipher: AuthenticatedNpLdtDecryptCipher<CryptoProviderImpl> =
        AuthenticatedNpLdtDecryptCipher {
            ldt_decrypter: NpLdtDecryptCipher::<CryptoProviderImpl>::new(
                &LdtKey::from_concatenated(&[0; 64]),
            ),
            metadata_key_tag: [0; 32],
            metadata_key_hmac_key: np_hkdf::NpHmacSha256Key::from([0; 32]),
        };

    // 1 byte less than a full block
    let payload = vec![0; BLOCK_SIZE - 1];
    assert_eq!(
        Err(LdtAdvDecryptError::InvalidLength(BLOCK_SIZE - 1)),
        adv_cipher.decrypt_and_verify(&payload, &XorPadder::from([0_u8; BLOCK_SIZE]))
    );
}

#[test]
fn decrypt_too_long_err() {
    let adv_cipher: AuthenticatedNpLdtDecryptCipher<CryptoProviderImpl> =
        AuthenticatedNpLdtDecryptCipher {
            ldt_decrypter: NpLdtDecryptCipher::<CryptoProviderImpl>::new(
                &LdtKey::from_concatenated(&[0; 64]),
            ),
            metadata_key_tag: [0; 32],
            metadata_key_hmac_key: np_hkdf::NpHmacSha256Key::from([0; 32]),
        };

    // 2 full blocks
    let payload = [0; BLOCK_SIZE * 2];
    assert_eq!(
        Err(LdtAdvDecryptError::InvalidLength(BLOCK_SIZE * 2)),
        adv_cipher.decrypt_and_verify(&payload, &XorPadder::from([0; BLOCK_SIZE]))
    );
}

#[test]
fn verify_input_len_const() {
    // make sure it matches the authoritative upstream
    assert_eq!(crate::VALID_INPUT_LEN, NpLdtEncryptCipher::<CryptoProviderImpl>::VALID_INPUT_LEN);
}

#[test]
fn ldt_ffi_test_scenario() {
    // used in ldt_ffi_tests.cc and LdtNpJniTests
    let key_seed: [u8; 32] = [
        204, 219, 36, 137, 233, 252, 172, 66, 179, 147, 72, 184, 148, 30, 209, 154, 29, 54, 14,
        117, 224, 152, 200, 193, 94, 107, 28, 194, 182, 32, 205, 57,
    ];

    let salt: V0Salt = [12, 15].into();
    let plaintext = [
        205_u8, 104, 63, 225, 161, 209, 248, 70, 84, 61, 10, 19, 212, 174, 164, 0, 64, 200, 214,
        123,
    ];

    let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let ldt_key = hkdf.v0_ldt_key();
    let identity_token_hmac = hkdf
        .v0_identity_token_hmac_key()
        .calculate_hmac::<CryptoProviderImpl>(&plaintext[..V0_IDENTITY_TOKEN_LEN]);
    let decrypter = build_np_adv_decrypter_from_key_seed(&hkdf, identity_token_hmac);

    let padder = salt_padder::<CryptoProviderImpl>(salt);
    let mut ciphertext = plaintext;
    NpLdtEncryptCipher::<CryptoProviderImpl>::new(&ldt_key)
        .encrypt(&mut ciphertext, &padder)
        .unwrap();

    let (token, contents) = decrypter.decrypt_and_verify(&ciphertext, &padder).unwrap();
    assert_eq!(&plaintext[..V0_IDENTITY_TOKEN_LEN], token.as_slice());
    assert_eq!(&plaintext[V0_IDENTITY_TOKEN_LEN..], contents.as_slice());

    // Uncomment if updating the FFI test
    // {
    //     use std::println;
    //     use test_helper::hex_bytes;
    //     println!("Key seed:\n{}\n{}", hex_bytes(&key_seed), hex::encode_upper(&key_seed));
    //     println!("Identity token HMAC:\n{}\n{}", hex_bytes(&identity_token_hmac), hex::encode_upper(&identity_token_hmac));
    //     println!("Plaintext:\n{}\n{}", hex_bytes(&plaintext), hex::encode_upper(&plaintext));
    //     println!("Salt:\n{}\n{}", hex_bytes(salt.bytes()), hex::encode_upper(salt.bytes()));
    //     println!("Ciphertext:\n{}\n{}", hex_bytes(&ciphertext), hex::encode_upper(&ciphertext));
    //     panic!();
    // }
}

/// Returns (plaintext, ciphertext, padder, hmac key, MAC, ldt)
fn make_test_components<C: crypto_provider::CryptoProvider>(
    rng: &mut C::CryptoRng,
) -> LdtAdvTestComponents<C> {
    // [1, 2) blocks of XTS-AES
    let mut rc_rng = seeded_rng();
    let payload_len = rc_rng.gen_range(BLOCK_SIZE..=(BLOCK_SIZE * 2 - 1));
    let all_plaintext = random_vec::<C>(rng, payload_len);

    let salt = V0Salt { bytes: random_bytes::<2, C>(rng) };
    let padder = salt_padder::<C>(salt);

    let key_seed: [u8; 32] = random_bytes::<32, C>(rng);
    let hkdf = np_hkdf::NpKeySeedHkdf::new(&key_seed);
    let ldt_key = hkdf.v0_ldt_key();
    let hmac_key = hkdf.v0_identity_token_hmac_key();
    let identity_token: [u8; V0_IDENTITY_TOKEN_LEN] =
        all_plaintext[..V0_IDENTITY_TOKEN_LEN].try_into().unwrap();
    let hmac: [u8; 32] = hmac_key.calculate_hmac::<C>(&identity_token);

    let ldt_enc = NpLdtEncryptCipher::<C>::new(&ldt_key);

    let mut ciphertext = [0_u8; LDT_XTS_AES_MAX_LEN];
    ciphertext[..all_plaintext.len()].copy_from_slice(&all_plaintext);
    ldt_enc.encrypt(&mut ciphertext[..all_plaintext.len()], &padder).unwrap();
    let remaining_plaintext = all_plaintext[V0_IDENTITY_TOKEN_LEN..].to_vec();
    LdtAdvTestComponents {
        all_plaintext,
        identity_token: identity_token.into(),
        remaining_plaintext,
        ciphertext: ciphertext[..payload_len].to_vec(),
        padder,
        hmac,
        ldt_enc,
        hkdf,
    }
}

struct LdtAdvTestComponents<C: CryptoProvider> {
    all_plaintext: Vec<u8>,
    identity_token: V0IdentityToken,
    /// Plaintext after the identity token
    remaining_plaintext: Vec<u8>,
    ciphertext: Vec<u8>,
    padder: XorPadder<{ BLOCK_SIZE }>,
    hmac: [u8; 32],
    ldt_enc: NpLdtEncryptCipher<C>,
    hkdf: NpKeySeedHkdf<C>,
}

mod coverage_gaming {
    extern crate std;

    use crate::{V0IdentityToken, V0_IDENTITY_TOKEN_LEN};
    use crypto_provider::{CryptoProvider, CryptoRng};
    use crypto_provider_default::CryptoProviderImpl;
    use std::{collections, format};

    #[test]
    fn legacy_identity_token() {
        let token = V0IdentityToken::from([0; V0_IDENTITY_TOKEN_LEN]);
        // debug
        let _ = format!("{:?}", token);
        // hash
        let _ = collections::HashSet::new().insert(&token);
        // bytes
        assert_eq!(token.0, token.bytes());
        // AsRef
        assert_eq!(token.0.as_slice(), token.as_ref());
        // FromCryptoRng
        let mut rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();
        let _: V0IdentityToken = rng.gen();
    }
}
