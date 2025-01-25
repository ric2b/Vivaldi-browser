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

extern crate std;

use crate::{
    build_np_adv_decrypter_from_key_seed, salt_padder, NpLdtEncryptCipher, V0Salt,
    V0_IDENTITY_TOKEN_LEN,
};
use anyhow::anyhow;
use crypto_provider_default::CryptoProviderImpl;
use ldt::LdtCipher;
use serde_json::json;
use std::vec::Vec;
use std::{fs, io::Read as _, println, string::String};
use test_helper::{extract_key_array, extract_key_vec};
use test_vector_hkdf::TestVectorHkdf;

#[test]
fn np_adv_test_vectors() -> Result<(), anyhow::Error> {
    let full_path =
        test_helper::get_data_file("presence/ldt_np_adv/resources/test/np_adv_test_vectors.json");
    let mut file = fs::File::open(full_path)?;
    let mut data = String::new();
    let _ = file.read_to_string(&mut data)?;
    let test_cases = match serde_json::de::from_str(&data)? {
        serde_json::Value::Array(a) => a,
        _ => return Err(anyhow!("bad json")),
    };

    assert_eq!(100, test_cases.len());

    for tc in test_cases {
        let key_seed = extract_key_array::<32>(&tc, "key_seed");

        let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
        let ldt_key = hkdf.v0_ldt_key();
        let hmac_key = hkdf.v0_identity_token_hmac_key();

        assert_eq!(&extract_key_vec(&tc, "ldt_key"), &ldt_key.as_concatenated());
        assert_eq!(&extract_key_vec(&tc, "hmac_key"), &hmac_key.as_bytes());

        let salt = V0Salt::from(extract_key_array(&tc, "adv_salt"));
        let padder = salt_padder::<CryptoProviderImpl>(salt);

        let ldt_enc = NpLdtEncryptCipher::<CryptoProviderImpl>::new(&ldt_key);

        let decrypter = build_np_adv_decrypter_from_key_seed(
            &hkdf,
            extract_key_array(&tc, "identity_token_hmac"),
        );

        let plaintext = extract_key_vec(&tc, "plaintext");
        let ciphertext = extract_key_vec(&tc, "ciphertext");

        let mut ciphertext_actual = plaintext.clone();
        ldt_enc.encrypt(&mut ciphertext_actual, &padder).unwrap();

        assert_eq!(ciphertext, ciphertext_actual);

        let (identity_token, plaintext_actual) =
            decrypter.decrypt_and_verify(&ciphertext, &padder).unwrap();

        assert_eq!(&plaintext[..V0_IDENTITY_TOKEN_LEN], identity_token.as_slice());
        assert_eq!(&plaintext[V0_IDENTITY_TOKEN_LEN..], plaintext_actual.as_slice());
    }

    Ok(())
}

// disable unless you want to print out a new set of test vectors
#[ignore]
#[test]
fn gen_test_vectors() {
    let mut array = Vec::<serde_json::Value>::new();

    for i in 0_u32..100 {
        let test_vector_seed_hkdf = TestVectorHkdf::<CryptoProviderImpl>::new(
            "NP LDT test vectors Y7yJTRXy+8saKiYSu76/TbabkAFhK55zF8QutxcQlGc",
            &i.to_be_bytes(),
        );

        let len = test_vector_seed_hkdf
            .derive_range_element(
                "plaintext len",
                crypto_provider::aes::BLOCK_SIZE as u64
                    ..=crypto_provider::aes::BLOCK_SIZE as u64 * 2 - 1,
            )
            .try_into()
            .unwrap();
        let plaintext = test_vector_seed_hkdf.derive_vec("plaintext", len);
        let key_seed: [u8; 32] = test_vector_seed_hkdf.derive_array("key seed");

        let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
        let ldt_key = hkdf.v0_ldt_key();
        let hmac_key = hkdf.v0_identity_token_hmac_key();
        let hmac: [u8; 32] =
            hmac_key.calculate_hmac::<CryptoProviderImpl>(&plaintext[..V0_IDENTITY_TOKEN_LEN]);
        let ldt_enc = NpLdtEncryptCipher::<CryptoProviderImpl>::new(&ldt_key);

        let salt = V0Salt::from(test_vector_seed_hkdf.derive_array("salt"));
        let padder = salt_padder::<CryptoProviderImpl>(salt);
        let mut ciphertext = plaintext.clone();
        ldt_enc.encrypt(&mut ciphertext[..], &padder).unwrap();

        array.push(json!({
            "key_seed": hex::encode_upper(key_seed),
            "ldt_key": hex::encode_upper(ldt_key.as_concatenated()),
            "hmac_key": hex::encode_upper(hmac_key.as_bytes()),
            "adv_salt": hex::encode_upper(salt.bytes()),
            "plaintext": hex::encode_upper(plaintext),
            "ciphertext": hex::encode_upper(ciphertext),
            "identity_token_hmac": hex::encode_upper(hmac),
        }));
    }

    println!("{}", serde_json::ser::to_string_pretty(&array).unwrap());
    panic!("Don't leave this test enabled. Meanwhile, enjoy the text output above.");
}
