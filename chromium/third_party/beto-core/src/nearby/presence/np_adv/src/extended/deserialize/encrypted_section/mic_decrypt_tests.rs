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
use crate::deserialization_arena;
use crate::extended::data_elements::TxPowerDataElement;
use crate::extended::deserialize::DataElementParseError;
use crate::extended::serialize::{AdvertisementType, SingleTypeDataElement, WriteDataElement};
use crate::shared_data::TxPower;
use crate::{
    credential::{v1::V1, SimpleBroadcastCryptoMaterial},
    de_type::EncryptedIdentityDataElementType,
    extended::{
        deserialize::{
            encrypted_section::{
                EncryptedSectionContents, IdentityResolutionOrDeserializationError,
                MicVerificationError,
            },
            parse_sections,
            test_stubs::IntermediateSectionExt,
            CiphertextSection, DataElement, RawV1Salt,
        },
        serialize::{AdvBuilder, CapacityLimitedVec, MicEncryptedSectionEncoder},
    },
    parse_adv_header, AdvHeader, Section,
};
use crypto_provider_default::CryptoProviderImpl;
use np_hkdf::v1_salt::DataElementOffset;
use sink::Sink;
use std::{prelude::rust_2021::*, vec};

#[test]
fn deserialize_mic_encrypted_correct_keys() {
    let metadata_key = MetadataKey([1; 16]);
    let key_seed = [2; 32];
    let raw_salt = RawV1Salt([3; 16]);
    let section_salt = V1Salt::<CryptoProviderImpl>::from(raw_salt);
    let identity_type = EncryptedIdentityDataElementType::Private;
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let broadcast_cm = SimpleBroadcastCryptoMaterial::<V1>::new(key_seed, metadata_key);

    let mut section_builder = adv_builder
        .section_builder(MicEncryptedSectionEncoder::<CryptoProviderImpl>::new(
            identity_type,
            V1Salt::from(*section_salt.as_array_ref()),
            &broadcast_cm,
        ))
        .unwrap();

    let txpower_de = TxPowerDataElement::from(TxPower::try_from(5).unwrap());
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

    let contents = if let CiphertextSection::MicEncryptedIdentity(contents) = &enc_section {
        contents
    } else {
        panic!("incorrect flavor");
    };

    let keypair = np_ed25519::KeyPair::<CryptoProviderImpl>::generate();

    // deserializing to Section works
    let discovery_credential =
        SimpleSignedBroadcastCryptoMaterial::new(key_seed, metadata_key, keypair.private_key())
            .derive_v1_discovery_credential::<CryptoProviderImpl>();

    let identity_resolution_material =
        discovery_credential.unsigned_identity_resolution_material::<CryptoProviderImpl>();
    let verification_material =
        discovery_credential.unsigned_verification_material::<CryptoProviderImpl>();

    let arena = deserialization_arena!();
    let mut allocator = arena.into_allocator();
    let section = contents
        .try_resolve_identity_and_deserialize::<CryptoProviderImpl>(
            &mut allocator,
            &identity_resolution_material,
            &verification_material,
        )
        .unwrap();

    assert_eq!(
        DecryptedSection::new(
            EncryptedIdentityDataElementType::Private,
            VerificationMode::Mic,
            metadata_key,
            raw_salt,
            SectionContents {
                section_header: 19 // encryption info de
                        + 2 // de header
                        + 16 // metadata key
                        + 2 // de contents
                        + 16, // mic hmac tag
                // battery DE
                data_element_start_offset: 2,
                de_region_excl_identity: &[txpower_de.de_header().serialize().as_slice(), &[5],]
                    .concat(),
            }
        ),
        section
    );
    let data_elements = section.collect_data_elements().unwrap();
    assert_eq!(
        data_elements,
        &[DataElement { offset: 2.into(), de_type: 0x05_u8.into(), contents: &[5] }]
    );

    assert_eq!(
        vec![(DataElementOffset::from(2_u8), TxPowerDataElement::DE_TYPE, vec![5_u8])],
        data_elements
            .into_iter()
            .map(|de| (de.offset(), de.de_type(), de.contents().to_vec()))
            .collect::<Vec<_>>()
    );

    let mut encryption_info_bytes = [0_u8; 19];
    encryption_info_bytes[0..2].copy_from_slice(&[0x91, 0x10]);
    encryption_info_bytes[2] = 0x00;
    encryption_info_bytes[3..].copy_from_slice(section_salt.as_slice());

    let ciphertext_end = adv.as_slice().len() - 16;
    assert_eq!(
        &MicEncryptedSection {
            contents: EncryptedSectionContents {
                section_header: 19 // encryption info de
                    + 2 // de header
                    + 16 // metadata key
                    + 2 // de contents
                    + 16, // mic hmac tag
                adv_header,
                encryption_info: EncryptionInfo { bytes: encryption_info_bytes },
                identity: EncryptedIdentityMetadata {
                    header_bytes: [0x90, 0x1],
                    offset: 1.into(),
                    identity_type: EncryptedIdentityDataElementType::Private,
                },
                all_ciphertext: &adv.as_slice()[1 + 1 + 19 + 2..ciphertext_end],
            },
            mic: SectionMic { mic: adv.as_slice()[ciphertext_end..].try_into().unwrap() }
        },
        contents
    );

    // plaintext is correct
    {
        let identity_resolution_contents =
            contents.contents.compute_identity_resolution_contents::<CryptoProviderImpl>();
        let identity_match = identity_resolution_contents
            .try_match::<CryptoProviderImpl>(
                &identity_resolution_material.into_raw_resolution_material(),
            )
            .unwrap();
        let arena = deserialization_arena!();
        let mut allocator = arena.into_allocator();
        let decrypted = contents
            .contents
            .decrypt_ciphertext::<CryptoProviderImpl>(&mut allocator, identity_match)
            .unwrap();

        let mut expected = Vec::new();
        // battery de
        expected.extend_from_slice(txpower_de.clone().de_header().serialize().as_slice());
        let _ = txpower_de.write_de_contents(&mut expected);

        assert_eq!(metadata_key, decrypted.metadata_key_plaintext);
        assert_eq!(&expected, decrypted.plaintext_contents);
    }
}

#[test]
fn deserialize_mic_encrypted_incorrect_aes_key_error() {
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
fn deserialize_mic_encrypted_incorrect_metadata_key_hmac_key_error() {
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
fn deserialize_mic_encrypted_incorrect_mic_hmac_key_error() {
    // bad mic hmac key -> bad calculated mic
    do_bad_deserialize_params::<CryptoProviderImpl>(
        MicVerificationError::MicMismatch.into(),
        None,
        None,
        Some([0xFF; 32].into()),
        None,
    );
}

#[test]
fn deserialize_mic_encrypted_incorrect_expected_metadata_key_hmac_error() {
    // bad expected metadata key mac
    do_bad_deserialize_params::<CryptoProviderImpl>(
        IdentityResolutionOrDeserializationError::IdentityMatchingError,
        None,
        None,
        None,
        Some([0xFF; 32]),
    );
}

#[test]
fn deserialize_mic_encrypted_incorrect_salt_error() {
    // bad salt -> bad iv -> bad metadata key plaintext
    do_bad_deserialize_tampered(
        DeserializeError::IdentityResolutionOrDeserializationError(
            IdentityResolutionOrDeserializationError::IdentityMatchingError,
        ),
        |_| {},
        |adv| adv[23..39].copy_from_slice(&[0xFF; 16]),
    );
}

#[test]
fn deserialize_mic_encrypted_de_that_wont_parse() {
    // add an extra byte to the section, leading it to try to parse a DE that doesn't exist
    do_bad_deserialize_tampered(
        DeserializeError::DataElementParseError(DataElementParseError::UnexpectedDataAfterEnd),
        |sec| sec.try_push(0xFF).unwrap(),
        |_| {},
    );
}

#[test]
fn deserialize_mic_encrypted_tampered_mic_error() {
    // flip the a bit in the first MIC byte
    do_bad_deserialize_tampered(
        DeserializeError::IdentityResolutionOrDeserializationError(
            MicVerificationError::MicMismatch.into(),
        ),
        |_| {},
        |adv| {
            let mic_start = adv.len() - 16;
            adv[mic_start] ^= 0x01
        },
    );
}

#[test]
fn deserialize_mic_encrypted_tampered_payload_error() {
    // flip the last payload bit
    do_bad_deserialize_tampered(
        DeserializeError::IdentityResolutionOrDeserializationError(
            MicVerificationError::MicMismatch.into(),
        ),
        |_| {},
        |adv| *adv.last_mut().unwrap() ^= 0x01,
    );
}

/// Attempt a decryption that will fail when using the provided parameters for decryption only.
/// `None` means use the correct value for that parameter.
fn do_bad_deserialize_params<C: CryptoProvider>(
    error: IdentityResolutionOrDeserializationError<MicVerificationError>,
    aes_key: Option<crypto_provider::aes::Aes128Key>,
    metadata_key_hmac_key: Option<np_hkdf::NpHmacSha256Key<C>>,
    mic_hmac_key: Option<np_hkdf::NpHmacSha256Key<C>>,
    expected_metadata_key_hmac: Option<[u8; 32]>,
) {
    let metadata_key = MetadataKey([1; 16]);
    let key_seed = [2; 32];
    let section_salt: V1Salt<C> = [3; 16].into();
    let identity_type = EncryptedIdentityDataElementType::Private;
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::new(&key_seed);

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let broadcast_cm = SimpleBroadcastCryptoMaterial::<V1>::new(key_seed, metadata_key);

    let mut section_builder = adv_builder
        .section_builder(MicEncryptedSectionEncoder::<C>::new(
            identity_type,
            section_salt,
            &broadcast_cm,
        ))
        .unwrap();

    section_builder.add_de(|_| TxPowerDataElement::from(TxPower::try_from(7).unwrap())).unwrap();

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
    let contents = if let CiphertextSection::MicEncryptedIdentity(contents) = &enc_section {
        contents
    } else {
        panic!("incorrect flavor");
    };

    let unsigned_identity_resolution_material = SectionIdentityResolutionMaterial {
        aes_key: aes_key.unwrap_or_else(|| np_hkdf::UnsignedSectionKeys::aes_key(&key_seed_hkdf)),
        metadata_key_hmac_key: *metadata_key_hmac_key
            .unwrap_or_else(|| key_seed_hkdf.extended_unsigned_metadata_key_hmac_key())
            .as_bytes(),
        expected_metadata_key_hmac: expected_metadata_key_hmac.unwrap_or_else(|| {
            key_seed_hkdf.extended_unsigned_metadata_key_hmac_key().calculate_hmac(&metadata_key.0)
        }),
    };
    let identity_resolution_material =
        UnsignedSectionIdentityResolutionMaterial::from_raw(unsigned_identity_resolution_material);

    let verification_material = UnsignedSectionVerificationMaterial {
        mic_hmac_key: *mic_hmac_key
            .unwrap_or_else(|| np_hkdf::UnsignedSectionKeys::hmac_key(&key_seed_hkdf))
            .as_bytes(),
    };

    assert_eq!(
        error,
        contents
            .try_resolve_identity_and_deserialize::<C>(
                &mut deserialization_arena!().into_allocator(),
                &identity_resolution_material,
                &verification_material,
            )
            .unwrap_err()
    );
}

#[derive(Debug, PartialEq)]
enum DeserializeError {
    IdentityResolutionOrDeserializationError(
        IdentityResolutionOrDeserializationError<MicVerificationError>,
    ),
    DataElementParseError(DataElementParseError),
}

fn do_bad_deserialize_tampered(
    expected_error: DeserializeError,
    mangle_section: impl Fn(&mut CapacityLimitedVec<u8, NP_ADV_MAX_SECTION_LEN>),
    mangle_adv: impl Fn(&mut Vec<u8>),
) {
    let metadata_key = MetadataKey([1; 16]);
    let key_seed = [2; 32];
    let section_salt: V1Salt<CryptoProviderImpl> = [3; 16].into();
    let identity_type = EncryptedIdentityDataElementType::Private;

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let broadcast_cm = SimpleBroadcastCryptoMaterial::<V1>::new(key_seed, metadata_key);

    let mut section_builder = adv_builder
        .section_builder(MicEncryptedSectionEncoder::<CryptoProviderImpl>::new(
            identity_type,
            section_salt,
            &broadcast_cm,
        ))
        .unwrap();

    section_builder.add_de(|_| TxPowerDataElement::from(TxPower::try_from(7).unwrap())).unwrap();

    mangle_section(&mut section_builder.section);

    section_builder.add_to_advertisement();

    let adv = adv_builder.into_advertisement();
    let mut adv_mut = adv.as_slice().to_vec();
    mangle_adv(&mut adv_mut);

    let (remaining, header) = parse_adv_header(&adv_mut).unwrap();

    let v1_header = if let AdvHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };

    let sections = parse_sections(v1_header, remaining).unwrap();
    assert_eq!(1, sections.len());

    let section = sections.into_iter().next().unwrap();
    let enc_section = section.as_ciphertext().unwrap();
    let contents = if let CiphertextSection::MicEncryptedIdentity(contents) = &enc_section {
        contents
    } else {
        panic!("incorrect flavor");
    };

    // generate a random key pair since we need _some_ public key in our discovery
    // credential, even if it winds up going unused
    let key_pair = np_ed25519::KeyPair::<CryptoProviderImpl>::generate();

    let discovery_credential =
        SimpleSignedBroadcastCryptoMaterial::new(key_seed, metadata_key, key_pair.private_key())
            .derive_v1_discovery_credential::<CryptoProviderImpl>();

    let identity_resolution_material =
        discovery_credential.unsigned_identity_resolution_material::<CryptoProviderImpl>();
    let verification_material =
        discovery_credential.unsigned_verification_material::<CryptoProviderImpl>();

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
                )
            );
        }
        Err(e) => assert_eq!(
            expected_error,
            DeserializeError::IdentityResolutionOrDeserializationError(e),
        ),
    };
}
