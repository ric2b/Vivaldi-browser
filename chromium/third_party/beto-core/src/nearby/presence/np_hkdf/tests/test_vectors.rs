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

#![allow(clippy::indexing_slicing, clippy::unwrap_used)]

use anyhow::anyhow;
use crypto_provider::aes::AesKey;
use crypto_provider_default::CryptoProviderImpl;
use np_hkdf::{v1_salt::ExtendedV1Salt, *};
use serde_json::json;
use std::{fs, io::Read as _};
use test_helper::extract_key_array;
use test_vector_hkdf::TestVectorHkdf;

#[test]
fn hkdf_test_vectors() -> Result<(), anyhow::Error> {
    let full_path =
        test_helper::get_data_file("presence/np_hkdf/resources/test/hkdf-test-vectors.json");
    let mut file = fs::File::open(full_path)?;
    let mut data = String::new();
    let _ = file.read_to_string(&mut data)?;
    let test_cases = match serde_json::de::from_str(&data)? {
        serde_json::Value::Array(a) => a,
        _ => return Err(anyhow!("bad json")),
    };

    for tc in test_cases {
        {
            let group = &tc["key_seed_hkdf"];
            let key_seed = extract_key_array::<32>(group, "key_seed");
            let hkdf = NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
            assert_eq!(
                extract_key_array::<64>(group, "v0_ldt_key"),
                hkdf.v0_ldt_key().as_concatenated()
            );
            assert_eq!(
                &extract_key_array::<32>(group, "v0_identity_token_hmac_key"),
                hkdf.v0_identity_token_hmac_key().as_bytes()
            );
            assert_eq!(
                extract_key_array::<12>(group, "v0_metadata_nonce"),
                hkdf.v0_metadata_nonce()
            );
            assert_eq!(
                extract_key_array::<12>(group, "v1_metadata_nonce"),
                hkdf.v1_metadata_nonce()
            );
            assert_eq!(
                &extract_key_array::<32>(group, "v1_mic_short_salt_identity_token_hmac_key"),
                hkdf.v1_mic_short_salt_keys().identity_token_hmac_key().as_bytes()
            );
            assert_eq!(
                extract_key_array::<16>(group, "v1_mic_short_salt_aes_key"),
                *hkdf.v1_mic_short_salt_keys().aes_key().as_array()
            );
            assert_eq!(
                &extract_key_array::<32>(group, "v1_mic_short_salt_mic_hmac_key"),
                hkdf.v1_mic_short_salt_keys().mic_hmac_key().as_bytes()
            );
            assert_eq!(
                &extract_key_array::<32>(group, "v1_mic_extended_salt_identity_token_hmac_key"),
                hkdf.v1_mic_extended_salt_keys().identity_token_hmac_key().as_bytes()
            );
            assert_eq!(
                extract_key_array::<16>(group, "v1_mic_extended_salt_aes_key"),
                *hkdf.v1_mic_extended_salt_keys().aes_key().as_array()
            );
            assert_eq!(
                &extract_key_array::<32>(group, "v1_mic_extended_salt_mic_hmac_key"),
                hkdf.v1_mic_extended_salt_keys().mic_hmac_key().as_bytes()
            );
            assert_eq!(
                &extract_key_array::<32>(group, "v1_signature_identity_token_hmac_key"),
                hkdf.v1_signature_keys().identity_token_hmac_key().as_bytes()
            );
            assert_eq!(
                extract_key_array::<16>(group, "v1_signature_section_aes_key"),
                *hkdf.v1_signature_keys().aes_key().as_array()
            );
        }

        {
            let group = &tc["v0_adv_salt_hkdf"];
            let ikm = extract_key_array::<2>(group, "adv_salt");
            assert_eq!(
                extract_key_array::<16>(group, "expanded_salt"),
                v0_ldt_expanded_salt::<CryptoProviderImpl>(&ikm)
            )
        }

        {
            let group = &tc["v0_identity_token_hkdf"];
            let ikm = extract_key_array::<14>(group, "v0_identity_token");
            assert_eq!(
                extract_key_array::<16>(group, "expanded_key"),
                v0_metadata_expanded_key::<CryptoProviderImpl>(&ikm)
            )
        }

        {
            let group = &tc["v1_section_extended_salt_hkdf"];
            let ikm = extract_key_array::<16>(group, "section_extended_salt");
            let salt = ExtendedV1Salt::from(ikm);
            assert_eq!(
                extract_key_array::<16>(group, "derived_salt_nonce"),
                salt.derive::<16, CryptoProviderImpl>(None).unwrap(),
            );
            assert_eq!(
                extract_key_array::<16>(group, "derived_salt_first_de"),
                salt.derive::<16, CryptoProviderImpl>(Some(0.into())).unwrap(),
            );
            assert_eq!(
                extract_key_array::<16>(group, "derived_salt_third_de"),
                salt.derive::<16, CryptoProviderImpl>(Some(2.into())).unwrap(),
            );
        }

        {
            let group = &tc["v1_mic_section_short_salt_hkdf"];
            let ikm = extract_key_array::<2>(group, "section_short_salt");
            assert_eq!(
                extract_key_array::<12>(group, "short_salt_nonce"),
                extended_mic_section_short_salt_nonce::<CryptoProviderImpl>(ikm),
            );
        }
    }

    Ok(())
}

// disable unless you want to print out a new set of test vectors
#[ignore]
#[allow(clippy::panic)]
#[test]
fn gen_test_vectors() {
    let mut array = Vec::<serde_json::Value>::new();

    for i in 0_u32..100 {
        // build "random" things in a repeatable way so future changes don't
        // rebuild unrelated things, with some /dev/random thrown in for good measure
        let test_vector_seed_hkdf = TestVectorHkdf::<CryptoProviderImpl>::new(
            "NP HKDF test vectors pb4qoNqM9aL/ezSC2FU5EQzu8JJoJ25B+rLqbU5kVN8",
            &i.to_be_bytes(),
        );

        let key_seed: [u8; 32] = test_vector_seed_hkdf.derive_array("key seed");
        let v0_adv_salt: [u8; 2] = test_vector_seed_hkdf.derive_array("legacy adv salt");
        let v0_identity_token: [u8; 14] = test_vector_seed_hkdf.derive_array("v0 identity token");
        let extended_salt_bytes: [u8; 16] =
            test_vector_seed_hkdf.derive_array("v1 section extended salt");
        let v1_mic_short_salt: [u8; 2] =
            test_vector_seed_hkdf.derive_array("v1 mic section short salt");
        let v1_extended_salt = ExtendedV1Salt::from(extended_salt_bytes);

        let key_seed_hkdf = NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
        array
            .push(json!({
                "key_seed_hkdf": {
                    "key_seed": hex::encode_upper(key_seed),
                    "v0_ldt_key": hex::encode_upper(key_seed_hkdf.v0_ldt_key().as_concatenated()),
                    "v0_identity_token_hmac_key":
                        hex::encode_upper(key_seed_hkdf.v0_identity_token_hmac_key().as_bytes()),
                    "v0_metadata_nonce": hex::encode_upper(key_seed_hkdf.v0_metadata_nonce()),
                    "v1_metadata_nonce": hex::encode_upper(key_seed_hkdf.v1_metadata_nonce()),
                    "v1_mic_short_salt_identity_token_hmac_key": hex::encode_upper(key_seed_hkdf.v1_mic_short_salt_keys().identity_token_hmac_key().as_bytes()),
                    "v1_mic_short_salt_aes_key": hex::encode_upper(key_seed_hkdf.v1_mic_short_salt_keys().aes_key().as_array()),
                    "v1_mic_short_salt_mic_hmac_key": hex::encode_upper(key_seed_hkdf.v1_mic_short_salt_keys().mic_hmac_key().as_bytes()),
                    "v1_mic_extended_salt_identity_token_hmac_key": hex::encode_upper(key_seed_hkdf.v1_mic_extended_salt_keys().identity_token_hmac_key().as_bytes()),
                    "v1_mic_extended_salt_aes_key": hex::encode_upper(key_seed_hkdf.v1_mic_extended_salt_keys().aes_key().as_array()),
                    "v1_mic_extended_salt_mic_hmac_key": hex::encode_upper(key_seed_hkdf.v1_mic_extended_salt_keys().mic_hmac_key().as_bytes()),
                    "v1_signature_identity_token_hmac_key": hex::encode_upper(key_seed_hkdf.v1_signature_keys().identity_token_hmac_key().as_bytes()),
                    "v1_signature_section_aes_key": hex::encode_upper(key_seed_hkdf.v1_signature_keys().aes_key().as_array()),
                },
                "v0_adv_salt_hkdf": {
                    "adv_salt": hex::encode_upper(v0_adv_salt),
                    "expanded_salt": hex::encode_upper(v0_ldt_expanded_salt::<CryptoProviderImpl>(&v0_adv_salt))
                },
                "v0_identity_token_hkdf": {
                    "v0_identity_token": hex::encode_upper(v0_identity_token),
                    "expanded_key":
                        hex::encode_upper(v0_metadata_expanded_key::<CryptoProviderImpl>(&v0_identity_token))
                },
                "v1_section_extended_salt_hkdf": {
                    "section_extended_salt": hex::encode_upper(v1_extended_salt.bytes()),
                    // 0-based offsets -> 1-based indexing
                    "derived_salt_nonce": hex::encode_upper(v1_extended_salt.derive::<16, CryptoProviderImpl>(None).unwrap()),
                    "derived_salt_first_de": hex::encode_upper(v1_extended_salt.derive::<16, CryptoProviderImpl>(Some(0.into())).unwrap()),
                    "derived_salt_third_de": hex::encode_upper(v1_extended_salt.derive::<16, CryptoProviderImpl>(Some(2.into())).unwrap()),
                },
                "v1_mic_section_short_salt_hkdf": {
                    "section_short_salt": hex::encode_upper(v1_mic_short_salt),
                    "short_salt_nonce": hex::encode_upper(extended_mic_section_short_salt_nonce::<CryptoProviderImpl>(v1_mic_short_salt)),
                }
            }));
    }

    println!("{}", serde_json::ser::to_string_pretty(&array).unwrap());
    panic!("Don't leave this test enabled. Meanwhile, enjoy the text output above.");
}
