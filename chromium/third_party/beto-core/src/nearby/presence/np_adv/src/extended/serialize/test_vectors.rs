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

#![allow(clippy::unwrap_used)]

extern crate std;

use crate::credential::v1::V1BroadcastCredential;
use crate::extended::de_type::DeType;
use crate::extended::salt::V1Salt;
use crate::extended::serialize::AdvertisementType;
use crate::extended::serialize::{
    section_tests::{DummyDataElement, SectionBuilderExt},
    AdvBuilder, MicEncryptedSectionEncoder,
};
use crate::extended::V1IdentityToken;
use alloc::format;
use anyhow::anyhow;
use crypto_provider::{aes::ctr::AES_CTR_NONCE_LEN, aes::AesKey, ed25519, CryptoProvider};
use crypto_provider_default::CryptoProviderImpl;
use np_hkdf::{v1_salt, DerivedSectionKeys};
use serde_json::json;
use std::{fs, io::Read as _, prelude::rust_2021::*, println};
use test_helper::extract_key_array;
use test_vector_hkdf::TestVectorHkdf;

type Ed25519ProviderImpl = <CryptoProviderImpl as CryptoProvider>::Ed25519;

#[test]
fn mic_encrypted_test_vectors() -> Result<(), anyhow::Error> {
    let full_path = test_helper::get_data_file(
        "presence/np_adv/resources/test/mic-extended-salt-encrypted-test-vectors.json",
    );
    let mut file = fs::File::open(full_path)?;
    let mut data = String::new();
    let _ = file.read_to_string(&mut data)?;

    let test_cases = match serde_json::de::from_str(&data)? {
        serde_json::Value::Array(a) => a,
        _ => return Err(anyhow!("bad json")),
    };

    for tc in test_cases {
        {
            let key_seed = extract_key_array::<32>(&tc, "key_seed");
            let identity_token =
                V1IdentityToken::from(extract_key_array::<16>(&tc, "identity_token"));
            let adv_header_byte = extract_key_array::<1>(&tc, "adv_header_byte")[0];
            let section_salt =
                v1_salt::ExtendedV1Salt::from(extract_key_array::<16>(&tc, "section_salt"));
            let data_elements = tc["data_elements"]
                .as_array()
                .unwrap()
                .iter()
                .map(|json_de| DummyDataElement {
                    data: hex::decode(json_de["contents"].as_str().unwrap()).unwrap(),
                    de_type: (json_de["de_type"].as_u64().unwrap() as u32).into(),
                })
                .collect::<Vec<_>>();

            let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);

            assert_eq!(
                extract_key_array::<16>(&tc, "aes_key").as_slice(),
                hkdf.v1_mic_extended_salt_keys().aes_key().as_slice()
            );
            assert_eq!(
                extract_key_array::<32>(&tc, "section_mic_hmac_key").as_slice(),
                hkdf.v1_mic_extended_salt_keys().mic_hmac_key().as_bytes()
            );
            assert_eq!(
                extract_key_array::<{ AES_CTR_NONCE_LEN }>(&tc, "nonce").as_slice(),
                section_salt.compute_nonce::<CryptoProviderImpl>()
            );

            let broadcast_cred = V1BroadcastCredential::new(
                key_seed,
                identity_token,
                ed25519::PrivateKey::generate::<Ed25519ProviderImpl>(),
            );

            // make an adv builder in the configuration we need
            let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
            assert_eq!(adv_header_byte, adv_builder.adv_header().contents());

            let mut section_builder = adv_builder
                .section_builder(MicEncryptedSectionEncoder::<_>::new::<CryptoProviderImpl>(
                    section_salt,
                    &broadcast_cred,
                ))
                .unwrap();

            for de in data_elements {
                section_builder.add_de(|_| de).unwrap();
            }

            assert_eq!(
                hex::decode(tc["encoded_section"].as_str().unwrap()).unwrap(),
                section_builder.into_section::<CryptoProviderImpl>().as_slice()
            );
        }
    }

    Ok(())
}

#[ignore]
#[test]
fn gen_mic_encrypted_test_vectors() {
    let mut array = Vec::<serde_json::Value>::new();

    for i in 0_u32..100 {
        let test_vector_seed_hkdf = TestVectorHkdf::<CryptoProviderImpl>::new(
            "NP V1 MIC test vectors HpakGBH8I+zbcEdxEanIT1r3bkgmL+mWI/kMrPiCzPw",
            &i.to_be_bytes(),
        );

        let num_des = test_vector_seed_hkdf.derive_range_element("num des", 0_u64..=5);

        let extra_des =
            (0..num_des)
                .map(|de_index| {
                    let de_len = test_vector_seed_hkdf
                        .derive_range_element(&format!("de len {}", de_index), 0..=30);
                    let data = test_vector_seed_hkdf
                        .derive_vec(&format!("de data {}", de_index), de_len.try_into().unwrap());
                    DummyDataElement {
                        de_type: DeType::from(
                            u32::try_from(test_vector_seed_hkdf.derive_range_element(
                                &format!("de type {}", de_index),
                                0_u64..=1_000,
                            ))
                            .unwrap(),
                        ),
                        data,
                    }
                })
                .collect::<Vec<_>>();

        let identity_token =
            V1IdentityToken::from(test_vector_seed_hkdf.derive_array("identity token"));
        let key_seed = test_vector_seed_hkdf.derive_array("key seed");
        let adv_header_byte = 0b00100000;

        let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);

        let broadcast_cred = V1BroadcastCredential::new(
            key_seed,
            identity_token,
            ed25519::PrivateKey::generate::<Ed25519ProviderImpl>(),
        );

        let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

        let section_salt =
            v1_salt::ExtendedV1Salt::from(test_vector_seed_hkdf.derive_array::<16>("salt"));
        let mut section_builder = adv_builder
            .section_builder(MicEncryptedSectionEncoder::<_>::new::<CryptoProviderImpl>(
                section_salt,
                &broadcast_cred,
            ))
            .unwrap();

        for de in extra_des.iter() {
            section_builder.add_de(|_| de).unwrap();
        }

        let nonce = section_salt.compute_nonce::<CryptoProviderImpl>();
        let keys = key_seed_hkdf.v1_mic_extended_salt_keys();
        array.push(json!({
            "key_seed": hex::encode_upper(key_seed),
            "identity_token": hex::encode_upper(identity_token.as_slice()),
            "adv_header_byte": hex::encode_upper([adv_header_byte]),
            "section_salt": hex::encode_upper(section_salt.as_slice()),
            "data_elements": extra_des.iter().map(|de| json!({
                "de_type": de.de_type.as_u32(),
                "contents": hex::encode_upper(&de.data)
            })).collect::<Vec<_>>(),
            "aes_key": hex::encode_upper(keys.aes_key().as_slice()),
            "nonce": hex::encode_upper(nonce),
            "section_mic_hmac_key": hex::encode_upper(keys.mic_hmac_key().as_bytes()),
            "encoded_section": hex::encode_upper(section_builder.into_section::<CryptoProviderImpl>().as_slice())
        }));
    }

    println!("{}", serde_json::ser::to_string_pretty(&array).unwrap());
    panic!("Don't leave this test enabled. Meanwhile, enjoy the text output above.");
}
