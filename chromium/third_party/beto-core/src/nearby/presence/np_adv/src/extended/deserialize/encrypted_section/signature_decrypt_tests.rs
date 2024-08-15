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

use crate::deserialization_arena;
use crate::extended::data_elements::TxPowerDataElement;
use crate::extended::deserialize::{
    DataElementParseError, DecryptedSection, EncryptionInfo, RawV1Salt, SectionContents,
};
use crate::extended::serialize::AdvertisementType;
use crate::shared_data::TxPower;
use crate::{
    credential::v1::*,
    de_type::EncryptedIdentityDataElementType,
    extended::{
        deserialize::{
            encrypted_section::{
                EncryptedSectionContents, IdentityResolutionOrDeserializationError,
                SignatureEncryptedSection, SignatureVerificationError,
            },
            parse_sections,
            test_stubs::IntermediateSectionExt,
            CiphertextSection, DataElement, EncryptedIdentityMetadata, VerificationMode,
        },
        section_signature_payload::*,
        serialize::{
            AdvBuilder, CapacityLimitedVec, SignedEncryptedSectionEncoder, SingleTypeDataElement,
            WriteDataElement,
        },
        NP_ADV_MAX_SECTION_LEN,
    },
    parse_adv_header, AdvHeader, MetadataKey, Section,
};
use crypto_provider::{aes::ctr::AesCtrNonce, CryptoProvider};
use crypto_provider_default::CryptoProviderImpl;
use np_hkdf::v1_salt;
use np_hkdf::v1_salt::V1Salt;
use sink::Sink;
use std::{prelude::rust_2021::*, vec};

type KeyPair = np_ed25519::KeyPair<CryptoProviderImpl>;

#[test]
fn deserialize_signature_encrypted_correct_keys() {
    let metadata_key = MetadataKey([1; 16]);
    let key_seed = [2; 32];
    let raw_salt = RawV1Salt([3; 16]);
    let section_salt = V1Salt::<CryptoProviderImpl>::from(raw_salt);
    let identity_type = EncryptedIdentityDataElementType::Private;
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let key_pair = KeyPair::generate();

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let broadcast_cm =
        SimpleSignedBroadcastCryptoMaterial::new(key_seed, metadata_key, key_pair.private_key());

    let mut section_builder = adv_builder
        .section_builder(SignedEncryptedSectionEncoder::<CryptoProviderImpl>::new(
            identity_type,
            V1Salt::from(*section_salt.as_array_ref()),
            &broadcast_cm,
        ))
        .unwrap();

    let txpower_de = TxPowerDataElement::from(TxPower::try_from(7).unwrap());
    section_builder.add_de(|_| txpower_de.clone()).unwrap();

    section_builder.add_to_advertisement();

    let adv = adv_builder.into_advertisement();

    let (remaining, header) = parse_adv_header(adv.as_slice()).unwrap();

    let adv_header = if let AdvHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };

    let sections = parse_sections(adv_header, remaining).unwrap();
    assert_eq!(1, sections.len());

    let section = sections.into_iter().next().unwrap();
    let enc_section = section.as_ciphertext().unwrap();
    let contents = if let CiphertextSection::SignatureEncryptedIdentity(contents) = &enc_section {
        contents
    } else {
        panic!("incorrect flavor");
    };

    let mut encryption_info_bytes = [0_u8; 19];
    encryption_info_bytes[0..2].copy_from_slice(&[0x91, 0x10]);
    encryption_info_bytes[2] = 0x08;
    encryption_info_bytes[3..].copy_from_slice(section_salt.as_slice());

    let section_len = 19 + 2 + 16 + 2 + 64;
    assert_eq!(
        &SignatureEncryptedSection {
            contents: EncryptedSectionContents {
                section_header: section_len,
                adv_header,
                encryption_info: EncryptionInfo { bytes: encryption_info_bytes },
                identity: EncryptedIdentityMetadata {
                    header_bytes: [0x90, 0x1],
                    offset: 1.into(),
                    identity_type: EncryptedIdentityDataElementType::Private,
                },
                // adv header + salt + section header + encryption info + identity header
                all_ciphertext: &adv.as_slice()[1 + 1 + 19 + 2..],
            },
        },
        contents
    );

    // plaintext is correct
    {
        let crypto_material = V1DiscoveryCredential::new(
            key_seed,
            key_seed_hkdf.extended_unsigned_metadata_key_hmac_key().calculate_hmac(&metadata_key.0),
            key_seed_hkdf.extended_signed_metadata_key_hmac_key().calculate_hmac(&metadata_key.0),
            key_pair.public().to_bytes(),
        );
        let signed_identity_resolution_material =
            crypto_material.signed_identity_resolution_material::<CryptoProviderImpl>();
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

        let mut expected = Vec::new();
        expected.extend_from_slice(txpower_de.de_header().serialize().as_slice());
        let _ = txpower_de.write_de_contents(&mut expected);

        let nonce: AesCtrNonce = section_salt.derive(Some(1.into())).unwrap();

        let mut encryption_info = vec![0x91, 0x10, 0x08];
        encryption_info.extend_from_slice(section_salt.as_slice());
        let encryption_info: [u8; EncryptionInfo::TOTAL_DE_LEN] =
            encryption_info.try_into().unwrap();

        let sig_payload = SectionSignaturePayload::from_deserialized_parts(
            0x20,
            section_len,
            &encryption_info,
            &nonce,
            [0x90, 0x1],
            metadata_key,
            &expected,
        );

        expected.extend_from_slice(&sig_payload.sign(&key_pair).to_bytes());
        assert_eq!(metadata_key, decrypted.metadata_key_plaintext);
        assert_eq!(nonce, decrypted.nonce);
        assert_eq!(&expected, decrypted.plaintext_contents);
    }

    // deserialization to Section works
    {
        let crypto_material = V1DiscoveryCredential::new(
            key_seed,
            key_seed_hkdf.extended_unsigned_metadata_key_hmac_key().calculate_hmac(&metadata_key.0),
            key_seed_hkdf.extended_signed_metadata_key_hmac_key().calculate_hmac(&metadata_key.0),
            key_pair.public().to_bytes(),
        );
        let signed_identity_resolution_material =
            crypto_material.signed_identity_resolution_material::<CryptoProviderImpl>();
        let signed_verification_material =
            crypto_material.signed_verification_material::<CryptoProviderImpl>();

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
                EncryptedIdentityDataElementType::Private,
                VerificationMode::Signature,
                metadata_key,
                raw_salt,
                SectionContents {
                    section_header: 19 + 2 + 16 + 1 + 1 + 64,
                    data_element_start_offset: 2,
                    de_region_excl_identity:
                        &[txpower_de.de_header().serialize().as_slice(), &[7],].concat(),
                },
            ),
            section
        );
        let data_elements = section.collect_data_elements().unwrap();
        assert_eq!(
            data_elements,
            &[DataElement { offset: 2.into(), de_type: 0x05_u8.into(), contents: &[7] }]
        );

        assert_eq!(
            vec![(v1_salt::DataElementOffset::from(2_u8), TxPowerDataElement::DE_TYPE, vec![7u8])],
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
        Some(KeyPair::generate().public()),
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
        |adv_mut| adv_mut[5..21].copy_from_slice(&[0xFF; 16]),
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
fn deserialize_signature_encrypted_missing_signature_de_error() {
    let section_len = 19 + 2 + 16 + 1 + 1;
    do_bad_deserialize_tampered(
        DeserializeError::IdentityResolutionOrDeserializationError(
            SignatureVerificationError::SignatureMissing.into(),
        ),
        Some(section_len),
        |_| {},
        |adv_mut| {
            // chop off signature DE
            adv_mut.truncate(adv_mut.len() - 64);
            // fix section length
            adv_mut[1] = section_len;
        },
    )
}

#[test]
fn deserialize_signature_encrypted_des_wont_parse() {
    do_bad_deserialize_tampered(
        DeserializeError::DataElementParseError(DataElementParseError::UnexpectedDataAfterEnd),
        Some(19 + 2 + 16 + 1 + 1 + 64 + 1),
        // add an impossible DE
        |section| section.try_push(0xFF).unwrap(),
        |_| {},
    )
}

/// Attempt a deserialization that will fail when using the provided parameters for decryption only.
/// `None` means use the correct value for that parameter.
fn do_bad_deserialize_params<C: CryptoProvider>(
    error: IdentityResolutionOrDeserializationError<SignatureVerificationError>,
    aes_key: Option<crypto_provider::aes::Aes128Key>,
    metadata_key_hmac_key: Option<np_hkdf::NpHmacSha256Key<C>>,
    expected_metadata_key_hmac: Option<[u8; 32]>,
    pub_key: Option<np_ed25519::PublicKey<C>>,
) {
    let metadata_key = MetadataKey([1; 16]);
    let key_seed = [2; 32];
    let section_salt: v1_salt::V1Salt<C> = [3; 16].into();
    let identity_type = EncryptedIdentityDataElementType::Private;
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::new(&key_seed);
    let key_pair = np_ed25519::KeyPair::<C>::generate();

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let broadcast_cm =
        SimpleSignedBroadcastCryptoMaterial::new(key_seed, metadata_key, key_pair.private_key());

    let mut section_builder = adv_builder
        .section_builder(SignedEncryptedSectionEncoder::new(
            identity_type,
            section_salt,
            &broadcast_cm,
        ))
        .unwrap();

    section_builder.add_de_res(|_| TxPower::try_from(2).map(TxPowerDataElement::from)).unwrap();

    section_builder.add_to_advertisement();

    let adv = adv_builder.into_advertisement();

    let (remaining, header) = parse_adv_header(adv.as_slice()).unwrap();

    let v1_header = if let AdvHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };

    let sections = parse_sections(v1_header, remaining).unwrap();
    assert_eq!(1, sections.len());

    let section = sections.into_iter().next().unwrap();
    let enc_section = section.as_ciphertext().unwrap();
    let contents = if let CiphertextSection::SignatureEncryptedIdentity(contents) = &enc_section {
        contents
    } else {
        panic!("incorrect flavor");
    };

    let signed_identity_resolution_material =
        SignedSectionIdentityResolutionMaterial::from_raw(SectionIdentityResolutionMaterial {
            aes_key: aes_key.unwrap_or_else(|| key_seed_hkdf.extended_signed_section_aes_key()),

            metadata_key_hmac_key: *metadata_key_hmac_key
                .unwrap_or_else(|| key_seed_hkdf.extended_signed_metadata_key_hmac_key())
                .as_bytes(),
            expected_metadata_key_hmac: expected_metadata_key_hmac.unwrap_or_else(|| {
                key_seed_hkdf
                    .extended_signed_metadata_key_hmac_key()
                    .calculate_hmac(&metadata_key.0)
            }),
        });

    let signed_verification_material = SignedSectionVerificationMaterial {
        pub_key: pub_key.unwrap_or_else(|| key_pair.public()).to_bytes(),
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
/// Since the advertisement is ciphertext, only changes outside
fn do_bad_deserialize_tampered(
    expected_error: DeserializeError,
    expected_section_len: Option<u8>,
    mangle_section: impl Fn(&mut CapacityLimitedVec<u8, NP_ADV_MAX_SECTION_LEN>),
    mangle_adv_contents: impl Fn(&mut Vec<u8>),
) {
    let metadata_key = MetadataKey([1; 16]);
    let key_seed = [2; 32];
    let section_salt: v1_salt::V1Salt<CryptoProviderImpl> = [3; 16].into();
    let identity_type = EncryptedIdentityDataElementType::Private;
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let key_pair = KeyPair::generate();

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let broadcast_cm =
        SimpleSignedBroadcastCryptoMaterial::new(key_seed, metadata_key, key_pair.private_key());

    let mut section_builder = adv_builder
        .section_builder(SignedEncryptedSectionEncoder::new(
            identity_type,
            section_salt,
            &broadcast_cm,
        ))
        .unwrap();

    section_builder.add_de_res(|_| TxPower::try_from(2).map(TxPowerDataElement::from)).unwrap();

    mangle_section(&mut section_builder.section);

    section_builder.add_to_advertisement();

    let adv = adv_builder.into_advertisement();
    let mut adv_mut = adv.as_slice().to_vec();
    mangle_adv_contents(&mut adv_mut);

    let (remaining, header) = parse_adv_header(&adv_mut).unwrap();

    let adv_header = if let AdvHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };

    let sections = parse_sections(adv_header, remaining).unwrap();
    assert_eq!(1, sections.len());

    let section = sections.into_iter().next().unwrap();
    let enc_section = section.as_ciphertext().unwrap();
    let contents = if let CiphertextSection::SignatureEncryptedIdentity(contents) = &enc_section {
        contents
    } else {
        panic!("incorrect flavor");
    };

    let mut encryption_info_bytes = [0_u8; 19];
    encryption_info_bytes[0..2].copy_from_slice(&[0x91, 0x10]);
    encryption_info_bytes[2] = 0x08;
    encryption_info_bytes[3..].copy_from_slice(&adv_mut[5..21]);

    let section_len = 19 + 2 + 16 + 2 + 64;
    assert_eq!(
        &SignatureEncryptedSection {
            contents: EncryptedSectionContents {
                section_header: expected_section_len.unwrap_or(section_len),
                adv_header,
                encryption_info: EncryptionInfo { bytes: encryption_info_bytes },
                identity: EncryptedIdentityMetadata {
                    header_bytes: [0x90, 0x1],
                    offset: 1.into(),
                    identity_type: EncryptedIdentityDataElementType::Private,
                },
                all_ciphertext: &adv_mut[1 + 1 + 19 + 2..],
            },
        },
        contents
    );

    let crypto_material = V1DiscoveryCredential::new(
        key_seed,
        key_seed_hkdf.extended_unsigned_metadata_key_hmac_key().calculate_hmac(&metadata_key.0),
        key_seed_hkdf.extended_signed_metadata_key_hmac_key().calculate_hmac(&metadata_key.0),
        key_pair.public().to_bytes(),
    );
    let identity_resolution_material =
        crypto_material.signed_identity_resolution_material::<CryptoProviderImpl>();
    let verification_material =
        crypto_material.signed_verification_material::<CryptoProviderImpl>();

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
