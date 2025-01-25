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

use super::*;
use crate::extended::{
    V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN,
    V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN, V1_ENCODING_UNENCRYPTED,
};
use crate::{
    credential::v1::V1BroadcastCredential,
    extended::{
        data_elements::{GenericDataElement, GenericDataElementError},
        deserialize::SectionMic,
        salt::V1Salt,
        section_signature_payload::SectionSignaturePayload,
        serialize::AddSectionError::MaxSectionCountExceeded,
        V1IdentityToken, V1_IDENTITY_TOKEN_LEN,
    },
    NP_SVC_UUID,
};
use crypto_provider::ed25519;
use crypto_provider::ed25519::SIGNATURE_LENGTH;
use crypto_provider::{
    aes::ctr::{AesCtr, NonceAndCounter},
    hmac::Hmac,
    CryptoRng,
};
use crypto_provider_default::CryptoProviderImpl;
use np_hkdf::v1_salt::EXTENDED_SALT_LEN;
use np_hkdf::DerivedSectionKeys;
use rand::{rngs::StdRng, Rng as _, SeedableRng as _};
use std::{prelude::rust_2021::*, vec};

type Ed25519ProviderImpl = <CryptoProviderImpl as CryptoProvider>::Ed25519;

#[test]
fn unencrypted_section_empty() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let section_builder = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();

    assert_eq!(
        &[V1_ENCODING_UNENCRYPTED, 0_u8],
        section_builder.into_section::<CryptoProviderImpl>().as_slice()
    );
}

#[test]
fn mic_encrypted_identity_section_empty() {
    do_mic_encrypted_identity_fixed_key_material_test::<DummyDataElement>(&[]);
}

#[test]
fn mic_encrypted_identity_section_random_des() {
    let mut rng = StdRng::from_entropy();
    let mut crypto_rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();

    for _ in 0..1_000 {
        let num_des = rng.gen_range(1..=5);

        let extra_des = (0..num_des)
            .map(|_| {
                let de_len = rng.gen_range(0..=30);
                DummyDataElement {
                    de_type: rng.gen_range(0_u32..=u32::MAX).into(),
                    data: rand_ext::random_vec_rc(&mut rng, de_len),
                }
            })
            .collect::<Vec<_>>();

        let identity_token: V1IdentityToken = crypto_rng.gen();
        let key_seed = rng.gen();
        let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);

        let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

        let broadcast_cred = V1BroadcastCredential::new(
            key_seed,
            identity_token,
            ed25519::PrivateKey::generate::<Ed25519ProviderImpl>(),
        );

        let mut section_builder =
            adv_builder
                .section_builder(MicEncryptedSectionEncoder::<_>::new_random_salt::<
                    CryptoProviderImpl,
                >(&mut crypto_rng, &broadcast_cred))
                .unwrap();
        let section_salt = section_builder.section_encoder.salt;

        for de in extra_des.iter() {
            section_builder.add_de(|_| de).unwrap();
        }

        let section_length = mic_section_len(&extra_des);

        let mut hmac = key_seed_hkdf
            .v1_mic_extended_salt_keys()
            .mic_hmac_key()
            .build_hmac::<CryptoProviderImpl>();
        let nonce = section_salt.compute_nonce::<CryptoProviderImpl>();

        let mut cipher = <CryptoProviderImpl as CryptoProvider>::AesCtr128::new(
            &key_seed_hkdf.v1_mic_extended_salt_keys().aes_key(),
            NonceAndCounter::from_nonce(nonce),
        );

        // encrypt identity token and de contents
        let mut plaintext_identity_token = identity_token.as_slice().to_vec();
        cipher.apply_keystream(&mut plaintext_identity_token);
        let ct_identity_token = plaintext_identity_token;

        let mut de_contents = Vec::new();
        for de in extra_des {
            de_contents.extend_from_slice(de.de_header().serialize().as_slice());
            let _ = de.write_de_contents(&mut de_contents);
        }
        cipher.apply_keystream(&mut de_contents);

        // just to be sure, we'll construct our test hmac all in one update() call
        let mut hmac_input = vec![];
        hmac_input.extend_from_slice(NP_SVC_UUID.as_slice());
        hmac_input.push(VERSION_HEADER_V1);
        hmac_input.push(V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN);
        hmac_input.extend_from_slice(section_salt.as_slice());
        hmac_input.extend_from_slice(nonce.as_slice());
        hmac_input.extend_from_slice(&ct_identity_token);
        hmac_input.push(section_length);
        hmac_input.extend_from_slice(&de_contents);
        hmac.update(&hmac_input);
        let mic = hmac.finalize();

        let mut expected = vec![];
        expected.push(V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN);
        expected.extend_from_slice(section_salt.as_slice());
        expected.extend_from_slice(&ct_identity_token);
        expected.push(section_length);
        expected.extend_from_slice(&de_contents);
        expected.extend_from_slice(&mic[..16]);

        assert_eq!(&expected, section_builder.into_section::<CryptoProviderImpl>().as_slice());
    }
}

#[test]
fn signature_encrypted_identity_section_random_des() {
    let mut rng = StdRng::from_entropy();
    let mut crypto_rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();

    for _ in 0..1_000 {
        let num_des = rng.gen_range(1..=5);

        let identity_token = V1IdentityToken(rng.gen());
        let key_seed = rng.gen();
        let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);

        let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
        let private_key = ed25519::PrivateKey::generate::<Ed25519ProviderImpl>();

        let broadcast_cred =
            V1BroadcastCredential::new(key_seed, identity_token, private_key.clone());

        let mut section_builder = adv_builder
            .section_builder(SignedEncryptedSectionEncoder::new_random_salt::<CryptoProviderImpl>(
                &mut crypto_rng,
                &broadcast_cred,
            ))
            .unwrap();
        let section_salt = section_builder.section_encoder.salt;

        let extra_des = (0..num_des)
            .map(|_| {
                let de_len = rng.gen_range(0..=30);
                DummyDataElement {
                    de_type: rng.gen_range(0_u32..=u32::MAX).into(),
                    data: rand_ext::random_vec_rc(&mut rng, de_len),
                }
            })
            // the DEs might not all fit; keep those that do
            .filter_map(|de| section_builder.add_de(|_| de.clone()).ok().map(|_| de))
            .collect::<Vec<_>>();

        let section_length = sig_section_len(&extra_des);
        let nonce = section_salt.compute_nonce::<CryptoProviderImpl>();

        let mut section_body = Vec::new();
        for de in extra_des {
            section_body.extend_from_slice(de.de_header().serialize().as_slice());
            let _ = de.write_de_contents(&mut section_body);
        }

        let mut section_header = vec![];
        section_header
            .extend_from_slice(&[V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN]);
        section_header.extend_from_slice(section_salt.as_slice());
        section_header.extend_from_slice(identity_token.as_slice());

        let sig_payload = SectionSignaturePayload::new(
            &[V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN],
            section_salt.as_slice(),
            &nonce,
            identity_token.as_slice(),
            section_length,
            &section_body,
        );

        let mut plaintext = section_body.as_slice().to_vec();
        plaintext
            .extend_from_slice(&sig_payload.sign::<Ed25519ProviderImpl>(&private_key).to_bytes());

        let mut cipher = <CryptoProviderImpl as CryptoProvider>::AesCtr128::new(
            &key_seed_hkdf.v1_signature_keys().aes_key(),
            NonceAndCounter::from_nonce(nonce),
        );
        let start_of_identity_token = section_header.len() - V1_IDENTITY_TOKEN_LEN;
        cipher.apply_keystream(&mut section_header[start_of_identity_token..]);

        cipher.apply_keystream(&mut plaintext);
        let ciphertext = plaintext;

        let mut expected = vec![];
        expected.extend_from_slice(&section_header);
        expected.push(section_length);
        expected.extend_from_slice(&ciphertext);

        assert_eq!(&expected, section_builder.into_section::<CryptoProviderImpl>().as_slice());
    }
}

#[test]
fn section_builder_too_full_doesnt_advance_de_index() {
    let mut crypto_rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let key_seed = [22; 32];
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let identity_token = V1IdentityToken([33; 16]);

    let broadcast_cred = V1BroadcastCredential::new(
        key_seed,
        identity_token,
        ed25519::PrivateKey::generate::<Ed25519ProviderImpl>(),
    );

    let mut section_builder = adv_builder
        .section_builder(MicEncryptedSectionEncoder::<_>::new_random_salt::<CryptoProviderImpl>(
            &mut crypto_rng,
            &broadcast_cred,
        ))
        .unwrap();
    let salt = section_builder.section_encoder.salt;

    section_builder
        .add_de(|de_salt| DummyDataElement {
            de_type: 100_u32.into(),
            data: de_salt.derive::<100, CryptoProviderImpl>().unwrap().to_vec(),
        })
        .unwrap();

    // this write won't advance the de offset or the internal section buffer length
    assert_eq!(
        AddDataElementError::InsufficientSectionSpace,
        section_builder
            .add_de(|de_salt| DummyDataElement {
                de_type: 101_u32.into(),
                data: de_salt.derive::<100, CryptoProviderImpl>().unwrap().to_vec(),
            })
            .unwrap_err()
    );

    section_builder
        .add_de(|de_salt| DummyDataElement {
            de_type: 102_u32.into(),
            data: de_salt.derive::<10, CryptoProviderImpl>().unwrap().to_vec(),
        })
        .unwrap();

    section_builder.add_to_advertisement::<CryptoProviderImpl>();

    let mut expected = vec![];
    // identity token
    expected.extend_from_slice(&identity_token.0);
    // section len
    expected.push(2 + 100 + 2 + 10u8 + u8::try_from(SectionMic::CONTENTS_LEN).unwrap());
    // de header
    expected.extend_from_slice(&[0x80 + 100, 100]);
    // section 0 de 0
    expected.extend_from_slice(&salt.derive::<100, CryptoProviderImpl>(Some(0.into())).unwrap());
    // de header
    expected.extend_from_slice(&[0x80 + 10, 102]);
    // section 0 de 1
    expected.extend_from_slice(&salt.derive::<10, CryptoProviderImpl>(Some(1.into())).unwrap());

    let mut cipher = <CryptoProviderImpl as CryptoProvider>::AesCtr128::new(
        &key_seed_hkdf.v1_mic_extended_salt_keys().aes_key(),
        NonceAndCounter::from_nonce(salt.compute_nonce::<CryptoProviderImpl>()),
    );

    cipher.apply_keystream(&mut expected[..V1_IDENTITY_TOKEN_LEN]);
    cipher.apply_keystream(&mut expected[V1_IDENTITY_TOKEN_LEN + 1..]);

    let adv_bytes = adv_builder.into_advertisement();
    // ignoring the MIC, etc, since that's tested elsewhere
    let ciphertext_end = adv_bytes.as_slice().len() - SectionMic::CONTENTS_LEN;
    assert_eq!(&expected, &adv_bytes.as_slice()[1 + 1 + EXTENDED_SALT_LEN..ciphertext_end])
}

#[test]
fn section_de_fits_exactly() {
    // leave room for initial filler section's header and the identities
    // for section_contents_capacity in 1..NP_ADV_MAX_SECTION_LEN - 3 {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);

    // fill up space to produce desired capacity
    let mut section_builder = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();
    // leave space for adv header, 1 section len, 1 section header and 1 extra byte
    fill_section_builder(BLE_5_ADV_SVC_MAX_CONTENT_LEN - 1 - 1 - 1 - 1, &mut section_builder)
        .unwrap();
    assert_eq!(1, section_builder.section.capacity() - section_builder.section.len(), "capacity: ");

    // can't add a 2 byte DE
    assert_eq!(
        Err(AddDataElementError::InsufficientSectionSpace),
        section_builder.add_de_res(|_| GenericDataElement::try_from(1_u32.into(), &[0xFF])),
        "capacity: "
    );

    // can add a 1 byte DE
    section_builder.add_de_res(|_| GenericDataElement::try_from(1_u32.into(), &[])).unwrap();
}

#[test]
fn section_builder_build_de_error_doesnt_advance_de_index() {
    let mut crypto_rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let key_seed = [22; 32];
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let identity_token = V1IdentityToken([33; 16]);

    let broadcast_cred = V1BroadcastCredential::new(
        key_seed,
        identity_token,
        ed25519::PrivateKey::generate::<Ed25519ProviderImpl>(),
    );

    let mut section_builder = adv_builder
        .section_builder(MicEncryptedSectionEncoder::<_>::new_random_salt::<CryptoProviderImpl>(
            &mut crypto_rng,
            &broadcast_cred,
        ))
        .unwrap();
    let salt = section_builder.section_encoder.salt;

    section_builder
        .add_de(|de_salt| DummyDataElement {
            de_type: 100_u32.into(),
            data: de_salt.derive::<100, CryptoProviderImpl>().unwrap().to_vec(),
        })
        .unwrap();

    assert_eq!(
        AddDataElementError::BuildDeError(()),
        section_builder.add_de_res(|_| Err::<DummyDataElement, _>(())).unwrap_err()
    );

    section_builder
        .add_de(|de_salt| DummyDataElement {
            de_type: 103_u32.into(),
            data: de_salt.derive::<10, CryptoProviderImpl>().unwrap().to_vec(),
        })
        .unwrap();

    section_builder.add_to_advertisement::<CryptoProviderImpl>();

    let mut expected = vec![];
    // identity_token
    expected.extend_from_slice(identity_token.as_slice());
    //section len
    expected.push(2 + 100 + 2 + 10 + u8::try_from(SectionMic::CONTENTS_LEN).unwrap());
    // de header
    expected.extend_from_slice(&[0x80 + 100, 100]);
    // section 0 de 0
    expected.extend_from_slice(&salt.derive::<100, CryptoProviderImpl>(Some(0.into())).unwrap());
    // de header
    expected.extend_from_slice(&[0x80 + 10, 103]);
    // section 0 de 1
    expected.extend_from_slice(&salt.derive::<10, CryptoProviderImpl>(Some(1.into())).unwrap());

    let mut cipher = <CryptoProviderImpl as CryptoProvider>::AesCtr128::new(
        &key_seed_hkdf.v1_mic_extended_salt_keys().aes_key(),
        NonceAndCounter::from_nonce(salt.compute_nonce::<CryptoProviderImpl>()),
    );

    cipher.apply_keystream(&mut expected[..V1_IDENTITY_TOKEN_LEN]);
    cipher.apply_keystream(&mut expected[V1_IDENTITY_TOKEN_LEN + 1..]);

    let adv_bytes = adv_builder.into_advertisement();
    // ignoring the MIC, etc, since that's tested elsewhere
    let ciphertext_end = adv_bytes.as_slice().len() - SectionMic::CONTENTS_LEN;
    assert_eq!(&expected, &adv_bytes.as_slice()[1 + 1 + EXTENDED_SALT_LEN..ciphertext_end])
}

#[test]
fn add_multiple_de_correct_de_offsets_mic_encrypted_identity() {
    let mut crypto_rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let key_seed = [22; 32];
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let identity_token = V1IdentityToken([33; 16]);

    let broadcast_cred = V1BroadcastCredential::new(
        key_seed,
        identity_token,
        ed25519::PrivateKey::generate::<Ed25519ProviderImpl>(),
    );

    let mut section_builder = adv_builder
        .section_builder(MicEncryptedSectionEncoder::<_>::new_random_salt::<CryptoProviderImpl>(
            &mut crypto_rng,
            &broadcast_cred,
        ))
        .unwrap();
    let salt = section_builder.section_encoder.salt;

    section_builder
        .add_de(|de_salt| DummyDataElement {
            de_type: 64_u32.into(),
            data: de_salt.derive::<16, CryptoProviderImpl>().unwrap().to_vec(),
        })
        .unwrap();
    section_builder
        .add_de(|de_salt| DummyDataElement {
            de_type: 65_u32.into(),
            data: de_salt.derive::<16, CryptoProviderImpl>().unwrap().to_vec(),
        })
        .unwrap();

    section_builder.add_to_advertisement::<CryptoProviderImpl>();

    let mut expected = vec![];
    // identity_token
    expected.extend_from_slice(identity_token.as_slice());
    // section len, 2 des of 18 bytes each + section mic length
    expected.push((18 * 2u8) + u8::try_from(SectionMic::CONTENTS_LEN).unwrap());
    // de header
    expected.extend_from_slice(&[0x90, 0x40]);
    // section 0 de 0
    expected.extend_from_slice(&salt.derive::<16, CryptoProviderImpl>(Some(0.into())).unwrap());
    // de header
    expected.extend_from_slice(&[0x90, 0x41]);
    // section 0 de 1
    expected.extend_from_slice(&salt.derive::<16, CryptoProviderImpl>(Some(1.into())).unwrap());

    let mut cipher = <CryptoProviderImpl as CryptoProvider>::AesCtr128::new(
        &key_seed_hkdf.v1_mic_extended_salt_keys().aes_key(),
        NonceAndCounter::from_nonce(salt.derive::<12, CryptoProviderImpl>(None).unwrap()),
    );

    cipher.apply_keystream(&mut expected[..V1_IDENTITY_TOKEN_LEN]);
    cipher.apply_keystream(&mut expected[V1_IDENTITY_TOKEN_LEN + 1..]);

    let adv_bytes = adv_builder.into_advertisement();
    // ignoring the MIC, etc, since that's tested elsewhere
    let ciphertext_end = adv_bytes.as_slice().len() - 16;
    assert_eq!(&expected, &adv_bytes.as_slice()[1 + 1 + 16..ciphertext_end])
}

#[test]
fn add_multiple_de_correct_de_offsets_signature_encrypted_identity() {
    let mut crypto_rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let key_seed = [22; 32];
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let identity_token = V1IdentityToken([33; 16]);
    let private_key = ed25519::PrivateKey::generate::<Ed25519ProviderImpl>();

    let broadcast_cred = V1BroadcastCredential::new(key_seed, identity_token, private_key.clone());

    let mut section_builder = adv_builder
        .section_builder(SignedEncryptedSectionEncoder::new_random_salt::<CryptoProviderImpl>(
            &mut crypto_rng,
            &broadcast_cred,
        ))
        .unwrap();
    let salt = section_builder.section_encoder.salt;

    section_builder
        .add_de(|de_salt| DummyDataElement {
            de_type: 64_u32.into(),
            data: de_salt.derive::<16, CryptoProviderImpl>().unwrap().to_vec(),
        })
        .unwrap();
    section_builder
        .add_de(|de_salt| DummyDataElement {
            de_type: 65_u32.into(),
            data: de_salt.derive::<16, CryptoProviderImpl>().unwrap().to_vec(),
        })
        .unwrap();

    section_builder.add_to_advertisement::<CryptoProviderImpl>();

    let mut expected = vec![];
    // identity token
    expected.extend_from_slice(&identity_token.0);
    //len
    expected.push((18 * 2) + u8::try_from(SIGNATURE_LENGTH).unwrap());
    // de header
    expected.extend_from_slice(&[0x90, 0x40]);
    // section 0 de 0
    expected.extend_from_slice(&salt.derive::<16, CryptoProviderImpl>(Some(0.into())).unwrap());
    // de header
    expected.extend_from_slice(&[0x90, 0x41]);
    // section 0 de 1
    expected.extend_from_slice(&salt.derive::<16, CryptoProviderImpl>(Some(1.into())).unwrap());

    let mut cipher = <CryptoProviderImpl as CryptoProvider>::AesCtr128::new(
        &key_seed_hkdf.v1_signature_keys().aes_key(),
        NonceAndCounter::from_nonce(salt.derive::<12, CryptoProviderImpl>(None).unwrap()),
    );

    cipher.apply_keystream(&mut expected[..V1_IDENTITY_TOKEN_LEN]);
    cipher.apply_keystream(&mut expected[V1_IDENTITY_TOKEN_LEN + 1..]);

    let adv_bytes = adv_builder.into_advertisement();
    // ignoring the signature since that's tested elsewhere
    assert_eq!(
        &expected,
        // skip adv header + section header + salt
        &adv_bytes.as_slice()[1 + 1 + 16..adv_bytes.as_slice().len() - 64]
    )
}

#[test]
fn signature_encrypted_section_de_lengths_allow_room_for_suffix() {
    let mut crypto_rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let key_seed = [22; 32];
    let metadata_key = V1IdentityToken([33; 16]);
    let private_key = ed25519::PrivateKey::generate::<Ed25519ProviderImpl>();

    let broadcast_cred = V1BroadcastCredential::new(key_seed, metadata_key, private_key.clone());

    let mut section_builder = adv_builder
        .section_builder(SignedEncryptedSectionEncoder::new_random_salt::<CryptoProviderImpl>(
            &mut crypto_rng,
            &broadcast_cred,
        ))
        .unwrap();

    // section header + identity + signature
    let max_total_de_len = NP_ADV_MAX_SECTION_LEN - 1 - 2 - 16 - 2 - 64;

    // take up 100 bytes to put us within 1 DE of the limit
    section_builder
        .add_de(|_| DummyDataElement { de_type: 100_u32.into(), data: vec![0; 98] })
        .unwrap();

    // one byte too many won't fit
    assert_eq!(
        AddDataElementError::InsufficientSectionSpace,
        section_builder
            .add_de(|_| DummyDataElement {
                de_type: 100_u32.into(),
                data: vec![0; max_total_de_len - 100 - 1],
            })
            .unwrap_err()
    );

    // but this will, as it allows 2 bytes for this DE's header
    assert_eq!(
        AddDataElementError::InsufficientSectionSpace,
        section_builder
            .add_de(|_| DummyDataElement {
                de_type: 100_u32.into(),
                data: vec![0; max_total_de_len - 100 - 2],
            })
            .unwrap_err()
    );
}

#[test]
fn serialize_max_number_of_public_sections() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    for _ in 0..NP_V1_ADV_MAX_SECTION_COUNT {
        let mut section_builder = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();
        section_builder
            .add_de(|_| DummyDataElement { de_type: 100_u32.into(), data: vec![0; 27] })
            .unwrap();
        section_builder.add_to_advertisement::<CryptoProviderImpl>();
    }
    assert_eq!(
        adv_builder.section_builder(UnencryptedSectionEncoder).unwrap_err(),
        MaxSectionCountExceeded
    );
}

fn do_mic_encrypted_identity_fixed_key_material_test<W: WriteDataElement>(extra_des: &[W]) {
    let identity_token = V1IdentityToken([1; 16]);
    let key_seed = [2; 32];
    let adv_header_byte = 0b00100000;
    let section_salt: ExtendedV1Salt = [3; 16].into();
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);

    let broadcast_cred = V1BroadcastCredential::new(
        key_seed,
        identity_token,
        ed25519::PrivateKey::generate::<Ed25519ProviderImpl>(),
    );

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
    let mut section_builder = adv_builder
        .section_builder(MicEncryptedSectionEncoder::<_>::new::<CryptoProviderImpl>(
            section_salt,
            &broadcast_cred,
        ))
        .unwrap();
    for de in extra_des {
        section_builder.add_de(|_| de).unwrap();
    }

    // now construct expected bytes
    // mic length + length of des
    let section_length = mic_section_len(extra_des);

    let mut hmac =
        key_seed_hkdf.v1_mic_extended_salt_keys().mic_hmac_key().build_hmac::<CryptoProviderImpl>();
    let nonce = section_salt.compute_nonce::<CryptoProviderImpl>();

    let mut cipher = <CryptoProviderImpl as CryptoProvider>::AesCtr128::new(
        &key_seed_hkdf.v1_mic_extended_salt_keys().aes_key(),
        NonceAndCounter::from_nonce(nonce),
    );

    // encrypt identity token and de contents
    let mut plaintext_identity_token = identity_token.as_slice().to_vec();
    cipher.apply_keystream(&mut plaintext_identity_token);
    let ct_identity_token = plaintext_identity_token;

    let mut de_contents = Vec::new();
    for de in extra_des {
        de_contents.extend_from_slice(de.de_header().serialize().as_slice());
        let _ = de.write_de_contents(&mut de_contents);
    }
    cipher.apply_keystream(&mut de_contents);

    // just to be sure, we'll construct our test hmac all in one update() call
    let mut hmac_input = vec![];
    hmac_input.extend_from_slice(NP_SVC_UUID.as_slice());
    hmac_input.push(adv_header_byte);
    hmac_input.push(V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN);
    hmac_input.extend_from_slice(section_salt.as_slice());
    hmac_input.extend_from_slice(nonce.as_slice());
    hmac_input.extend_from_slice(&ct_identity_token);
    hmac_input.push(section_length);
    hmac_input.extend_from_slice(&de_contents);
    hmac.update(&hmac_input);
    let mic = hmac.finalize();

    let mut expected = vec![];
    expected.push(V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN);
    expected.extend_from_slice(section_salt.as_slice());
    expected.extend_from_slice(&ct_identity_token);
    expected.push(section_length);
    expected.extend_from_slice(&de_contents);
    expected.extend_from_slice(&mic[..16]);

    assert_eq!(&expected, section_builder.into_section::<CryptoProviderImpl>().as_slice());
}

/// Returns the length of a mic section containing `extra_des`
fn mic_section_len<W: WriteDataElement>(extra_des: &[W]) -> u8 {
    u8::try_from(SectionMic::CONTENTS_LEN).unwrap()
        + extra_des
            .iter()
            .map(|de| de.de_header().serialize().len() as u8 + de.de_header().len.as_u8())
            .sum::<u8>()
}

/// Returns the length of a signed section containing `extra_des`
fn sig_section_len<W: WriteDataElement>(extra_des: &[W]) -> u8 {
    u8::try_from(crypto_provider::ed25519::SIGNATURE_LENGTH).unwrap()
        + extra_des
            .iter()
            .map(|de| de.de_header().serialize().len() as u8 + de.de_header().len.as_u8())
            .sum::<u8>()
}

#[test]
fn signature_encrypted_identity_section_empty() {
    do_signature_encrypted_identity_fixed_key_material_test::<DummyDataElement>(&[]);
}

fn do_signature_encrypted_identity_fixed_key_material_test<W: WriteDataElement>(extra_des: &[W]) {
    // input fixed credential data used to encode the adv
    let identity_token = V1IdentityToken::from([1; 16]);
    let key_seed = [2; 32];
    let section_salt: ExtendedV1Salt = [3; 16].into();
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let private_key = ed25519::PrivateKey::generate::<Ed25519ProviderImpl>();
    let broadcast_cred = V1BroadcastCredential::new(key_seed, identity_token, private_key.clone());

    // Build an adv given the provided DEs in extra_des
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
    let mut section_builder = adv_builder
        .section_builder(SignedEncryptedSectionEncoder::new::<CryptoProviderImpl>(
            section_salt,
            &broadcast_cred,
        ))
        .unwrap();
    for de in extra_des {
        section_builder.add_de(|_| de).unwrap();
    }
    let section = section_builder.into_section::<CryptoProviderImpl>();

    // now manually construct the expected output
    let format = [V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN];
    let salt_bytes = section_salt.as_slice();
    let identity_token_bytes = identity_token.as_slice();
    // len of de contents + signature
    let section_length = sig_section_len(extra_des);

    let mut section_body = Vec::new();
    for de in extra_des {
        section_body.extend_from_slice(de.de_header().serialize().as_slice());
        let _ = de.write_de_contents(&mut section_body);
    }

    let nonce = section_salt.compute_nonce::<CryptoProviderImpl>();

    let sig_payload = SectionSignaturePayload::new(
        format.as_slice(),
        salt_bytes,
        &nonce,
        identity_token.as_slice(),
        section_length,
        &section_body,
    );
    let sig = sig_payload.sign::<Ed25519ProviderImpl>(&private_key).to_bytes();

    let mut cipher = <CryptoProviderImpl as CryptoProvider>::AesCtr128::new(
        &key_seed_hkdf.v1_signature_keys().aes_key(),
        NonceAndCounter::from_nonce(nonce),
    );

    let mut expected = vec![];
    expected.extend_from_slice(&format);
    expected.extend_from_slice(salt_bytes);

    let mut ct_identity_token = Vec::new();
    ct_identity_token.extend_from_slice(identity_token_bytes);
    cipher.apply_keystream(&mut ct_identity_token);

    expected.extend_from_slice(&ct_identity_token);
    expected.push(section_length);

    let mut remaining_ct = Vec::new();
    remaining_ct.extend_from_slice(&section_body);
    remaining_ct.extend_from_slice(&sig);
    cipher.apply_keystream(&mut remaining_ct);

    expected.extend_from_slice(&remaining_ct);

    assert_eq!(&expected, section.as_slice());
}

/// Write `section_contents_len` bytes of DE into `section_builder`
pub(crate) fn fill_section_builder<I: SectionEncoder>(
    section_contents_len: usize,
    section_builder: &mut SectionBuilder<&mut AdvBuilder, I>,
) -> Result<(), AddDataElementError<GenericDataElementError>> {
    let original_len = section_builder.section.len();
    // DEs can only go up to 127, so we'll need multiple for long sections
    for _ in 0..(section_contents_len / 100) {
        let de_contents = vec![0x33; 98];
        section_builder
            .add_de_res(|_| GenericDataElement::try_from(100_u32.into(), &de_contents))?;
    }

    let remainder_len = section_contents_len % 100;
    match remainder_len {
        0 => {
            // leave remainder empty
        }
        1 => {
            // 1 byte header
            section_builder.add_de_res(|_| GenericDataElement::try_from(3_u32.into(), &[]))?;
        }
        2 => {
            // 2 byte header
            section_builder.add_de_res(|_| GenericDataElement::try_from(100_u32.into(), &[]))?;
        }
        _ => {
            // 2 byte header + contents as needed
            // leave room for section length, section header, and DE headers
            let de_contents = vec![0x44; remainder_len - 2];
            section_builder
                .add_de_res(|_| GenericDataElement::try_from(100_u32.into(), &de_contents))?;
        }
    }

    assert_eq!(section_contents_len, section_builder.section.len() - original_len);

    Ok(())
}

#[derive(Clone)]
pub(crate) struct DummyDataElement {
    pub(crate) de_type: DeType,
    pub(crate) data: Vec<u8>,
}

impl WriteDataElement for DummyDataElement {
    fn de_header(&self) -> DeHeader {
        DeHeader { de_type: self.de_type, len: self.data.len().try_into().unwrap() }
    }

    fn write_de_contents<S: Sink<u8>>(&self, sink: &mut S) -> Option<()> {
        sink.try_extend_from_slice(&self.data)
    }
}

pub(crate) trait SectionBuilderExt {
    fn into_section<C: CryptoProvider>(self) -> EncodedSection;
}

impl<R: AsMut<AdvBuilder>, I: SectionEncoder> SectionBuilderExt for SectionBuilder<R, I> {
    /// Convenience method for tests
    fn into_section<C: CryptoProvider>(self) -> EncodedSection {
        Self::build_section::<C>(self.header_len, self.section.into_inner(), self.section_encoder)
    }
}
