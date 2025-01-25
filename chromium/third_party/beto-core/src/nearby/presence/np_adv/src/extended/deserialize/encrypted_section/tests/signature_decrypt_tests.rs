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
use crate::{
    deserialization_arena,
    extended::{
        data_elements::TxPowerDataElement,
        deserialize::{
            encrypted_section,
            section::intermediate::{
                parse_sections, tests::IntermediateSectionExt, CiphertextSection,
            },
            DataElement, DataElementParseError, Section,
        },
        serialize::{
            AdvBuilder, AdvertisementType, CapacityLimitedVec, SignedEncryptedSectionEncoder,
            SingleTypeDataElement, WriteDataElement,
        },
        V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN,
    },
    shared_data::TxPower,
    NpVersionHeader,
};
use crypto_provider::ed25519;
use crypto_provider_default::CryptoProviderImpl;
use np_hkdf::{v1_salt, DerivedSectionKeys};
use sink::Sink;
use std::{prelude::rust_2021::*, vec};

type Ed25519ProviderImpl = <CryptoProviderImpl as CryptoProvider>::Ed25519;

#[test]
fn deserialize_signature_encrypted_correct_keys_extended_salt() {
    let identity_token = V1IdentityToken::from([1; 16]);
    let key_seed = [2; 32];
    let section_salt = ExtendedV1Salt::from([3; 16]);
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let private_key = ed25519::PrivateKey::generate::<Ed25519ProviderImpl>();

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let broadcast_cm = V1BroadcastCredential::new(key_seed, identity_token, private_key.clone());

    let mut section_builder = adv_builder
        .section_builder(SignedEncryptedSectionEncoder::new::<CryptoProviderImpl>(
            section_salt,
            &broadcast_cm,
        ))
        .unwrap();

    let txpower_de = TxPowerDataElement::from(TxPower::try_from(7).unwrap());
    section_builder.add_de(|_| txpower_de.clone()).unwrap();

    section_builder.add_to_advertisement::<CryptoProviderImpl>();

    let adv = adv_builder.into_advertisement();

    let (after_version_header, header) = NpVersionHeader::parse(adv.as_slice()).unwrap();

    let adv_header = if let NpVersionHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };

    let sections = parse_sections(adv_header, after_version_header).unwrap();
    assert_eq!(1, sections.len());

    let section = sections.into_iter().next().unwrap();
    let enc_section = section.as_ciphertext().unwrap();
    let contents = if let CiphertextSection::SignatureEncrypted(contents) = &enc_section {
        contents
    } else {
        panic!("incorrect flavor");
    };

    assert_eq!(
        &SignatureEncryptedSection {
            contents: EncryptedSectionContents {
                adv_header,
                format_bytes: &[V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN],
                salt: section_salt,
                identity_token: first_section_identity_token(
                    section_salt.into(),
                    after_version_header
                ),
                // adv header + section len + format + salt + identity token
                section_contents: first_section_contents(after_version_header),
                total_section_contents_len: contents.contents.total_section_contents_len
            },
        },
        contents
    );

    let mut de_bytes = Vec::new();
    de_bytes.extend_from_slice(txpower_de.de_header().serialize().as_slice());
    let _ = txpower_de.write_de_contents(&mut de_bytes);
    let de_bytes = de_bytes;

    // plaintext is correct
    {
        let credential = V1DiscoveryCredential::new(
            key_seed,
            key_seed_hkdf
                .v1_mic_short_salt_keys()
                .identity_token_hmac_key()
                .calculate_hmac::<CryptoProviderImpl>(&identity_token.0),
            key_seed_hkdf
                .v1_mic_extended_salt_keys()
                .identity_token_hmac_key()
                .calculate_hmac::<CryptoProviderImpl>(&identity_token.0),
            key_seed_hkdf
                .v1_signature_keys()
                .identity_token_hmac_key()
                .calculate_hmac::<CryptoProviderImpl>(&identity_token.0),
            private_key.derive_public_key::<Ed25519ProviderImpl>(),
        );
        let signed_identity_resolution_material =
            credential.signed_identity_resolution_material::<CryptoProviderImpl>();
        let identity_resolution_contents =
            contents.contents.compute_identity_resolution_contents::<CryptoProviderImpl>();
        let identity_match = identity_resolution_contents
            .try_match::<CryptoProviderImpl>(
                &signed_identity_resolution_material.into_raw_resolution_material(),
            )
            .unwrap();

        let arena = deserialization_arena!();
        let mut allocator = arena.into_allocator();
        let decrypted =
            contents.contents.decrypt_ciphertext(&mut allocator, identity_match).unwrap();

        let nonce: AesCtrNonce = section_salt.compute_nonce::<CryptoProviderImpl>();

        let sig_payload = SectionSignaturePayload::new(
            &[V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN],
            section_salt.as_slice(),
            &nonce,
            identity_token.as_slice(),
            (de_bytes.len() + crypto_provider::ed25519::SIGNATURE_LENGTH).try_into().unwrap(),
            &de_bytes,
        );

        let mut de_and_sig = de_bytes.clone();
        de_and_sig
            .extend_from_slice(&sig_payload.sign::<Ed25519ProviderImpl>(&private_key).to_bytes());
        assert_eq!(identity_token, decrypted.identity_token);
        assert_eq!(nonce, decrypted.nonce);
        assert_eq!(&de_and_sig, decrypted.plaintext_contents);
    }

    // deserialization to Section works
    {
        let credential = V1DiscoveryCredential::new(
            key_seed,
            key_seed_hkdf
                .v1_mic_short_salt_keys()
                .identity_token_hmac_key()
                .calculate_hmac::<CryptoProviderImpl>(&identity_token.0),
            key_seed_hkdf
                .v1_mic_extended_salt_keys()
                .identity_token_hmac_key()
                .calculate_hmac::<CryptoProviderImpl>(&identity_token.0),
            key_seed_hkdf
                .v1_signature_keys()
                .identity_token_hmac_key()
                .calculate_hmac::<CryptoProviderImpl>(&identity_token.0),
            private_key.derive_public_key::<Ed25519ProviderImpl>(),
        );
        let signed_identity_resolution_material =
            credential.signed_identity_resolution_material::<CryptoProviderImpl>();
        let signed_verification_material =
            credential.signed_verification_material::<CryptoProviderImpl>();

        let arena = deserialization_arena!();
        let mut allocator = arena.into_allocator();
        let section = contents
            .try_resolve_identity_and_deserialize::<CryptoProviderImpl>(
                &mut allocator,
                &signed_identity_resolution_material,
                &signed_verification_material,
            )
            .unwrap();

        assert_eq!(
            DecryptedSection::new(
                VerificationMode::Signature,
                section_salt.into(),
                identity_token,
                &de_bytes,
            ),
            section
        );
        let data_elements = section.collect_data_elements().unwrap();
        assert_eq!(data_elements, &[DataElement::new(0.into(), 0x05_u8.into(), &[7])]);

        assert_eq!(
            vec![(v1_salt::DataElementOffset::from(0_u8), TxPowerDataElement::DE_TYPE, vec![7u8])],
            data_elements
                .into_iter()
                .map(|de| (de.offset(), de.de_type(), de.contents().to_vec()))
                .collect::<Vec<_>>()
        );
    }
}

#[test]
fn deserialize_signature_encrypted_incorrect_aes_key_error() {
    // bad aes key -> bad metadata key plaintext
    do_bad_deserialize_params::<CryptoProviderImpl>(
        IdentityResolutionOrDeserializationError::IdentityMatchingError,
        Some([0xFF; 16].into()),
        None,
        None,
        None,
    );
}

#[test]
fn deserialize_signature_encrypted_incorrect_metadata_key_hmac_key_error() {
    // bad metadata key hmac key -> bad calculated metadata key mac
    do_bad_deserialize_params::<CryptoProviderImpl>(
        IdentityResolutionOrDeserializationError::IdentityMatchingError,
        None,
        Some([0xFF; 32].into()),
        None,
        None,
    );
}

#[test]
fn deserialize_signature_encrypted_incorrect_expected_metadata_key_hmac_error() {
    // bad expected metadata key mac
    do_bad_deserialize_params::<CryptoProviderImpl>(
        IdentityResolutionOrDeserializationError::IdentityMatchingError,
        None,
        None,
        Some([0xFF; 32]),
        None,
    );
}

#[test]
fn deserialize_signature_encrypted_incorrect_pub_key_error() {
    // a random pub key will lead to signature mismatch
    do_bad_deserialize_params::<CryptoProviderImpl>(
        SignatureVerificationError::SignatureMismatch.into(),
        None,
        None,
        None,
        Some(
            ed25519::PrivateKey::generate::<Ed25519ProviderImpl>()
                .derive_public_key::<Ed25519ProviderImpl>(),
        ),
    );
}

#[test]
fn deserialize_signature_encrypted_incorrect_salt_error() {
    // bad salt -> bad iv -> bad metadata key plaintext
    do_bad_deserialize_tampered(
        DeserializeError::IdentityResolutionOrDeserializationError(
            IdentityResolutionOrDeserializationError::IdentityMatchingError,
        ),
        None,
        |_| {},
        |adv_mut| {
            adv_mut[1 + 1 + 1..][..EXTENDED_SALT_LEN].copy_from_slice(&[0xFF; EXTENDED_SALT_LEN])
        },
    )
}

#[test]
fn deserialize_signature_encrypted_tampered_signature_error() {
    do_bad_deserialize_tampered(
        DeserializeError::IdentityResolutionOrDeserializationError(
            SignatureVerificationError::SignatureMismatch.into(),
        ),
        None,
        |_| {},
        // flip a bit in the middle of the signature
        |adv_mut| {
            let len = adv_mut.len();
            adv_mut[len - 30] ^= 0x1
        },
    )
}

#[test]
fn deserialize_signature_encrypted_tampered_ciphertext_error() {
    do_bad_deserialize_tampered(
        DeserializeError::IdentityResolutionOrDeserializationError(
            SignatureVerificationError::SignatureMismatch.into(),
        ),
        None,
        |_| {},
        // flip a bit outside of the signature
        |adv_mut| {
            let len = adv_mut.len();
            adv_mut[len - 1 - 64] ^= 0x1
        },
    )
}

#[test]
fn deserialize_signature_encrypted_missing_signature_error() {
    let de_len = 2;
    do_bad_deserialize_tampered(
        DeserializeError::IdentityResolutionOrDeserializationError(
            SignatureVerificationError::SignatureMissing.into(),
        ),
        Some(de_len as u8),
        |_| {},
        |adv_mut| {
            // chop off signature
            adv_mut.truncate(adv_mut.len() - 64);
            // fix section length
            adv_mut[1 + 1 + EXTENDED_SALT_LEN + V1_IDENTITY_TOKEN_LEN..][0] = de_len as u8;
        },
    )
}

#[test]
fn deserialize_signature_encrypted_des_wont_parse() {
    do_bad_deserialize_tampered(
        DeserializeError::DataElementParseError(DataElementParseError::UnexpectedDataAfterEnd),
        Some(2 + 1 + crypto_provider::ed25519::SIGNATURE_LENGTH as u8),
        // add an impossible DE
        |section| {
            section.try_push(0xFF).unwrap();
        },
        |_| {},
    )
}

#[test]
fn arena_out_of_space_on_signature_verify() {
    let salt: ExtendedV1Salt = [0; 16].into();
    let nonce = salt.compute_nonce::<CryptoProviderImpl>();
    let sig_enc_section = SignatureEncryptedSection {
        contents: EncryptedSectionContents::new(
            V1AdvHeader::new(0),
            &[0],
            [0; 16].into(),
            [0x00; V1_IDENTITY_TOKEN_LEN].into(),
            1,
            &[0x00; 1],
        ),
    };

    let arena = deserialization_arena!();
    let mut allocator = arena.into_allocator();
    let _ = allocator.allocate(250).unwrap();

    let cipher = <CryptoProviderImpl as CryptoProvider>::AesCtr128::new(
        &[0u8; 16].into(),
        NonceAndCounter::from_nonce(nonce),
    );
    let im: encrypted_section::IdentityMatch<CryptoProviderImpl> =
        IdentityMatch { cipher, identity_token: V1IdentityToken([0; 16]), nonce };

    let signed_verification_material = SignedSectionVerificationMaterial {
        public_key: ed25519::PublicKey::from_bytes::<Ed25519ProviderImpl>([0u8; 32])
            .expect("public key should be valid bytes"),
    };

    let _ = sig_enc_section.try_deserialize(&mut allocator, im, &signed_verification_material);
}

/// Attempt a deserialization that will fail when using the provided parameters for decryption only.
/// `None` means use the correct value for that parameter.
fn do_bad_deserialize_params<C: CryptoProvider>(
    error: IdentityResolutionOrDeserializationError<SignatureVerificationError>,
    aes_key: Option<crypto_provider::aes::Aes128Key>,
    metadata_key_hmac_key: Option<np_hkdf::NpHmacSha256Key>,
    expected_metadata_key_hmac: Option<[u8; 32]>,
    pub_key: Option<ed25519::PublicKey>,
) {
    let metadata_key = V1IdentityToken([1; 16]);
    let key_seed = [2; 32];
    let section_salt: v1_salt::ExtendedV1Salt = [3; 16].into();
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&key_seed);
    let private_key = ed25519::PrivateKey::generate::<C::Ed25519>();

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let broadcast_cm = V1BroadcastCredential::new(key_seed, metadata_key, private_key.clone());

    let mut section_builder = adv_builder
        .section_builder(SignedEncryptedSectionEncoder::new::<C>(section_salt, &broadcast_cm))
        .unwrap();

    section_builder.add_de_res(|_| TxPower::try_from(2).map(TxPowerDataElement::from)).unwrap();

    section_builder.add_to_advertisement::<CryptoProviderImpl>();

    let adv = adv_builder.into_advertisement();

    let (remaining, header) = NpVersionHeader::parse(adv.as_slice()).unwrap();

    let v1_header = if let NpVersionHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };

    let sections = parse_sections(v1_header, remaining).unwrap();
    assert_eq!(1, sections.len());

    let section = sections.into_iter().next().unwrap();
    let enc_section = section.as_ciphertext().unwrap();
    let contents = if let CiphertextSection::SignatureEncrypted(contents) = &enc_section {
        contents
    } else {
        panic!("incorrect flavor");
    };

    let signed_identity_resolution_material =
        SignedSectionIdentityResolutionMaterial::from_raw(SectionIdentityResolutionMaterial {
            aes_key: aes_key.unwrap_or_else(|| key_seed_hkdf.v1_signature_keys().aes_key()),

            identity_token_hmac_key: *metadata_key_hmac_key
                .unwrap_or_else(|| key_seed_hkdf.v1_signature_keys().identity_token_hmac_key())
                .as_bytes(),
            expected_identity_token_hmac: expected_metadata_key_hmac.unwrap_or_else(|| {
                key_seed_hkdf
                    .v1_signature_keys()
                    .identity_token_hmac_key()
                    .calculate_hmac::<CryptoProviderImpl>(&metadata_key.0)
            }),
        });

    let signed_verification_material = SignedSectionVerificationMaterial {
        public_key: pub_key
            .unwrap_or_else(|| private_key.derive_public_key::<Ed25519ProviderImpl>()),
    };

    assert_eq!(
        error,
        contents
            .try_resolve_identity_and_deserialize::<C>(
                &mut deserialization_arena!().into_allocator(),
                &signed_identity_resolution_material,
                &signed_verification_material,
            )
            .unwrap_err()
    );
}

#[derive(Debug, PartialEq)]
enum DeserializeError {
    IdentityResolutionOrDeserializationError(
        IdentityResolutionOrDeserializationError<SignatureVerificationError>,
    ),
    DataElementParseError(DataElementParseError),
}

/// Run a test that mangles the advertisement contents before attempting to deserialize.
///
/// The section will have a TxPower DE added.
fn do_bad_deserialize_tampered(
    expected_error: DeserializeError,
    expected_section_de_len: Option<u8>,
    mangle_section: impl Fn(&mut CapacityLimitedVec<u8, NP_ADV_MAX_SECTION_LEN>),
    mangle_adv_contents: impl Fn(&mut Vec<u8>),
) {
    let identity_token = V1IdentityToken::from([1; V1_IDENTITY_TOKEN_LEN]);
    let key_seed = [2; 32];
    let section_salt: v1_salt::ExtendedV1Salt = [3; EXTENDED_SALT_LEN].into();
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let private_key = ed25519::PrivateKey::generate::<Ed25519ProviderImpl>();

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let broadcast_cm = V1BroadcastCredential::new(key_seed, identity_token, private_key.clone());

    let mut section_builder = adv_builder
        .section_builder(SignedEncryptedSectionEncoder::new::<CryptoProviderImpl>(
            section_salt,
            &broadcast_cm,
        ))
        .unwrap();

    section_builder.add_de_res(|_| TxPower::try_from(2).map(TxPowerDataElement::from)).unwrap();

    mangle_section(&mut section_builder.section);

    section_builder.add_to_advertisement::<CryptoProviderImpl>();

    let adv = adv_builder.into_advertisement();
    let mut adv_mut = adv.as_slice().to_vec();
    mangle_adv_contents(&mut adv_mut);

    let (after_version_header, header) = NpVersionHeader::parse(&adv_mut).unwrap();

    let adv_header = if let NpVersionHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };

    let sections = parse_sections(adv_header, after_version_header).unwrap();
    assert_eq!(1, sections.len());

    let section = sections.into_iter().next().unwrap();
    let enc_section = section.as_ciphertext().unwrap();
    let contents = if let CiphertextSection::SignatureEncrypted(contents) = &enc_section {
        contents
    } else {
        panic!("incorrect flavor");
    };
    if let Some(len) = expected_section_de_len {
        assert_eq!(usize::from(len), contents.contents.section_contents.len());
    }

    let extracted_salt = first_section_salt(after_version_header);
    assert_eq!(
        &SignatureEncryptedSection {
            contents: EncryptedSectionContents {
                adv_header,
                // extract data from adv buffer in case the caller tampered with things
                salt: extracted_salt,
                identity_token: first_section_identity_token(
                    extracted_salt.into(),
                    after_version_header
                ),
                section_contents: first_section_contents(after_version_header),
                format_bytes: first_section_format(after_version_header),
                total_section_contents_len: first_section_contents_len(after_version_header),
            },
        },
        contents
    );

    let credential = V1DiscoveryCredential::new(
        key_seed,
        key_seed_hkdf
            .v1_mic_short_salt_keys()
            .identity_token_hmac_key()
            .calculate_hmac::<CryptoProviderImpl>(&identity_token.0),
        key_seed_hkdf
            .v1_mic_extended_salt_keys()
            .identity_token_hmac_key()
            .calculate_hmac::<CryptoProviderImpl>(&identity_token.0),
        key_seed_hkdf
            .v1_signature_keys()
            .identity_token_hmac_key()
            .calculate_hmac::<CryptoProviderImpl>(&identity_token.0),
        private_key.derive_public_key::<Ed25519ProviderImpl>(),
    );
    let identity_resolution_material =
        credential.signed_identity_resolution_material::<CryptoProviderImpl>();
    let verification_material = credential.signed_verification_material::<CryptoProviderImpl>();

    match contents.try_resolve_identity_and_deserialize::<CryptoProviderImpl>(
        &mut deserialization_arena!().into_allocator(),
        &identity_resolution_material,
        &verification_material,
    ) {
        Ok(section) => {
            assert_eq!(
                expected_error,
                DeserializeError::DataElementParseError(
                    section.collect_data_elements().unwrap_err()
                ),
            );
        }
        Err(e) => assert_eq!(
            expected_error,
            DeserializeError::IdentityResolutionOrDeserializationError(e),
        ),
    };
}
