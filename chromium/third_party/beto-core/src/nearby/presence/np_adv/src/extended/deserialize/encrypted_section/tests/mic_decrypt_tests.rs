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

use super::super::*;
use crate::{
    deserialization_arena,
    extended::{
        data_elements::TxPowerDataElement,
        deserialize::{
            encrypted_section::tests::first_section_identity_token,
            section::intermediate::{
                parse_sections, tests::IntermediateSectionExt, CiphertextSection,
            },
            DataElement, DataElementParseError, Section,
        },
        salt::{ShortV1Salt, SHORT_SALT_LEN},
        serialize::{
            AdvBuilder, AdvertisementType, CapacityLimitedVec, MicEncryptedSectionEncoder,
            WriteDataElement,
        },
        V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN,
        V1_ENCODING_ENCRYPTED_MIC_WITH_SHORT_SALT_AND_TOKEN,
    },
    shared_data::TxPower,
    NpVersionHeader,
};
use crypto_provider::ed25519;
use crypto_provider_default::CryptoProviderImpl;
use np_hkdf::{v1_salt::EXTENDED_SALT_LEN, DerivedSectionKeys};
use sink::Sink;

type Ed25519ProviderImpl = <CryptoProviderImpl as CryptoProvider>::Ed25519;

#[test]
fn deserialize_mic_encrypted_correct_keys_extended_salt() {
    deserialize_mic_encrypted_correct_keys(ExtendedV1Salt::from([3; EXTENDED_SALT_LEN]).into())
}

#[test]
fn deserialize_mic_encrypted_correct_keys_short_salt() {
    deserialize_mic_encrypted_correct_keys(ShortV1Salt::from([3; SHORT_SALT_LEN]).into())
}

fn deserialize_mic_encrypted_correct_keys(salt: MultiSalt) {
    let identity_token = V1IdentityToken::from([1; 16]);
    let key_seed = [2; 32];
    let broadcast_cm = V1BroadcastCredential::new(
        key_seed,
        identity_token,
        ed25519::PrivateKey::generate::<Ed25519ProviderImpl>(),
    );

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
    let mut section_builder = adv_builder
        .section_builder(MicEncryptedSectionEncoder::<_>::new::<CryptoProviderImpl>(
            salt,
            &broadcast_cm,
        ))
        .unwrap();

    let txpower_de = TxPowerDataElement::from(TxPower::try_from(5).unwrap());
    section_builder.add_de(|_| txpower_de.clone()).unwrap();
    section_builder.add_to_advertisement::<CryptoProviderImpl>();
    let adv = adv_builder.into_advertisement();

    let (remaining, header) = NpVersionHeader::parse(adv.as_slice()).unwrap();

    let adv_header = if let NpVersionHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };

    let sections = parse_sections(adv_header, remaining).unwrap();
    assert_eq!(1, sections.len());

    let section = sections.into_iter().next().unwrap();
    let enc_section = section.as_ciphertext().unwrap();

    let contents = if let CiphertextSection::MicEncrypted(contents) = &enc_section {
        contents
    } else {
        panic!("incorrect flavor");
    };

    let private_key = ed25519::PrivateKey::generate::<Ed25519ProviderImpl>();
    // deserializing to Section works
    let discovery_credential = V1BroadcastCredential::new(key_seed, identity_token, private_key)
        .derive_discovery_credential::<CryptoProviderImpl>();

    let arena = deserialization_arena!();
    let mut allocator = arena.into_allocator();
    let section = contents
        .try_resolve_identity_and_deserialize::<CryptoProviderImpl>(
            &mut allocator,
            &discovery_credential,
        )
        .unwrap();

    assert_eq!(
        DecryptedSection::new(
            VerificationMode::Mic,
            salt,
            identity_token,
            &[txpower_de.de_header().serialize().as_slice(), &[5],].concat(),
        ),
        section
    );
    let data_elements = section.collect_data_elements().unwrap();
    assert_eq!(data_elements, &[DataElement::new(0.into(), 0x05_u8.into(), &[5])]);

    let (_header, contents_bytes) =
        remaining.split_at(1 + 1 + salt.as_slice().len() + V1_IDENTITY_TOKEN_LEN);
    assert_eq!(
        &MicEncryptedSection {
            contents: EncryptedSectionContents {
                adv_header,
                format_bytes: &[match salt {
                    MultiSalt::Short(_) => {
                        V1_ENCODING_ENCRYPTED_MIC_WITH_SHORT_SALT_AND_TOKEN
                    }
                    MultiSalt::Extended(_) => {
                        V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN
                    }
                }],
                salt,
                identity_token: first_section_identity_token(salt, remaining),
                section_contents: &contents_bytes
                    [..contents_bytes.len() - SectionMic::CONTENTS_LEN],
                total_section_contents_len: contents_bytes.len().try_into().unwrap(),
            },
            mic: SectionMic {
                mic: contents_bytes[contents_bytes.len() - SectionMic::CONTENTS_LEN..]
                    .try_into()
                    .unwrap()
            },
        },
        contents
    );

    // plaintext is correct
    {
        let identity_resolution_contents =
            contents.contents.compute_identity_resolution_contents::<CryptoProviderImpl>();
        let identity_match = identity_resolution_contents
            .try_match::<CryptoProviderImpl>(&match salt {
                MultiSalt::Short(_) => discovery_credential
                    .mic_short_salt_identity_resolution_material::<CryptoProviderImpl>()
                    .into_raw_resolution_material(),
                MultiSalt::Extended(_) => discovery_credential
                    .mic_extended_salt_identity_resolution_material::<CryptoProviderImpl>()
                    .into_raw_resolution_material(),
            })
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

        assert_eq!(identity_token, decrypted.identity_token);
        assert_eq!(&expected, decrypted.plaintext_contents);
    }
}

#[test]
fn deserialize_mic_encrypted_short_salt_incorrect_aes_key_error() {
    // bad aes key -> bad metadata key plaintext
    do_bad_deserialize_params::<CryptoProviderImpl>(
        ShortV1Salt::from([3; SHORT_SALT_LEN]).into(),
        IdentityResolutionOrDeserializationError::IdentityMatchingError,
        |cm| {
            cm.mic_short_salt_identity_resolution_material
                .as_mut_raw_resolution_material()
                .aes_key = [0xFF; 16].into();
        },
    );
}

#[test]
fn deserialize_mic_encrypted_short_salt_incorrect_identity_token_hmac_key_error() {
    // bad metadata key hmac key -> bad calculated metadata key mac
    do_bad_deserialize_params::<CryptoProviderImpl>(
        ShortV1Salt::from([3; SHORT_SALT_LEN]).into(),
        IdentityResolutionOrDeserializationError::IdentityMatchingError,
        |cm| {
            cm.mic_short_salt_identity_resolution_material
                .as_mut_raw_resolution_material()
                .identity_token_hmac_key = [0xFF; 32];
        },
    );
}

#[test]
fn deserialize_mic_encrypted_short_salt_incorrect_mic_hmac_key_error() {
    // bad mic hmac key -> bad calculated mic
    do_bad_deserialize_params::<CryptoProviderImpl>(
        ShortV1Salt::from([3; SHORT_SALT_LEN]).into(),
        MicVerificationError::MicMismatch.into(),
        |cm| {
            cm.mic_short_salt_verification_material.mic_hmac_key = [0xFF; 32];
        },
    );
}

#[test]
fn deserialize_mic_encrypted_short_salt_incorrect_expected_identity_token_hmac_error() {
    // bad expected metadata key mac
    do_bad_deserialize_params::<CryptoProviderImpl>(
        ShortV1Salt::from([3; SHORT_SALT_LEN]).into(),
        IdentityResolutionOrDeserializationError::IdentityMatchingError,
        |cm| {
            cm.mic_short_salt_identity_resolution_material
                .as_mut_raw_resolution_material()
                .expected_identity_token_hmac = [0xFF; 32];
        },
    );
}

#[test]
fn deserialize_mic_encrypted_extended_salt_incorrect_aes_key_error() {
    // bad aes key -> bad metadata key plaintext
    do_bad_deserialize_params::<CryptoProviderImpl>(
        ExtendedV1Salt::from([3; EXTENDED_SALT_LEN]).into(),
        IdentityResolutionOrDeserializationError::IdentityMatchingError,
        |cm| {
            cm.mic_extended_salt_identity_resolution_material
                .as_mut_raw_resolution_material()
                .aes_key = [0xFF; 16].into();
        },
    );
}

#[test]
fn deserialize_mic_encrypted_extended_salt_incorrect_identity_token_hmac_key_error() {
    // bad metadata key hmac key -> bad calculated metadata key mac
    do_bad_deserialize_params::<CryptoProviderImpl>(
        ExtendedV1Salt::from([3; EXTENDED_SALT_LEN]).into(),
        IdentityResolutionOrDeserializationError::IdentityMatchingError,
        |cm| {
            cm.mic_extended_salt_identity_resolution_material
                .as_mut_raw_resolution_material()
                .identity_token_hmac_key = [0xFF; 32];
        },
    );
}

#[test]
fn deserialize_mic_encrypted_extended_salt_incorrect_mic_hmac_key_error() {
    // bad mic hmac key -> bad calculated mic
    do_bad_deserialize_params::<CryptoProviderImpl>(
        ExtendedV1Salt::from([3; EXTENDED_SALT_LEN]).into(),
        MicVerificationError::MicMismatch.into(),
        |cm| {
            cm.mic_extended_salt_verification_material.mic_hmac_key = [0xFF; 32];
        },
    );
}

#[test]
fn deserialize_mic_encrypted_extended_salt_incorrect_expected_identity_token_hmac_error() {
    // bad expected metadata key mac
    do_bad_deserialize_params::<CryptoProviderImpl>(
        ExtendedV1Salt::from([3; EXTENDED_SALT_LEN]).into(),
        IdentityResolutionOrDeserializationError::IdentityMatchingError,
        |cm| {
            cm.mic_extended_salt_identity_resolution_material
                .as_mut_raw_resolution_material()
                .expected_identity_token_hmac = [0xFF; 32];
        },
    );
}

#[test]
fn deserialize_mic_encrypted_extended_salt_incorrect_salt_error() {
    // bad salt -> bad iv -> bad metadata key plaintext
    do_bad_deserialize_tampered(
        ExtendedV1Salt::from([3; EXTENDED_SALT_LEN]).into(),
        DeserializeError::IdentityResolutionOrDeserializationError(
            IdentityResolutionOrDeserializationError::IdentityMatchingError,
        ),
        |_| {},
        // replace the extended salt bytes
        |adv| adv[2..18].fill(0xFF),
    );
}

#[test]
fn deserialize_mic_encrypted_extended_salt_de_that_wont_parse() {
    // add an extra byte to the section, leading it to try to parse a DE that doesn't exist
    do_bad_deserialize_tampered(
        ExtendedV1Salt::from([3; EXTENDED_SALT_LEN]).into(),
        DeserializeError::DataElementParseError(DataElementParseError::UnexpectedDataAfterEnd),
        |sec| sec.try_push(0xFF).unwrap(),
        |_| {},
    );
}

#[test]
fn deserialize_mic_encrypted_extended_salt_tampered_mic_error() {
    // flip a bit in the first MIC byte
    do_bad_deserialize_tampered(
        ExtendedV1Salt::from([3; EXTENDED_SALT_LEN]).into(),
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
fn deserialize_mic_encrypted_extended_salt_tampered_payload_error() {
    // flip the last payload bit
    do_bad_deserialize_tampered(
        ExtendedV1Salt::from([3; EXTENDED_SALT_LEN]).into(),
        DeserializeError::IdentityResolutionOrDeserializationError(
            MicVerificationError::MicMismatch.into(),
        ),
        |_| {},
        |adv| {
            let before_mic = adv.len() - 17;
            adv[before_mic] ^= 0x01
        },
    );
}
#[test]
fn deserialize_mic_encrypted_short_salt_incorrect_salt_error() {
    // bad salt -> bad iv -> bad metadata key plaintext
    do_bad_deserialize_tampered(
        ShortV1Salt::from([3; SHORT_SALT_LEN]).into(),
        DeserializeError::IdentityResolutionOrDeserializationError(
            IdentityResolutionOrDeserializationError::IdentityMatchingError,
        ),
        |_| {},
        // replace the extended salt bytes
        |adv| adv[2..4].fill(0xFF),
    );
}

#[test]
fn deserialize_mic_encrypted_short_salt_de_that_wont_parse() {
    // add an extra byte to the section, leading it to try to parse a DE that doesn't exist
    do_bad_deserialize_tampered(
        ShortV1Salt::from([3; SHORT_SALT_LEN]).into(),
        DeserializeError::DataElementParseError(DataElementParseError::UnexpectedDataAfterEnd),
        |sec| sec.try_push(0xFF).unwrap(),
        |_| {},
    );
}

#[test]
fn deserialize_mic_encrypted_short_salt_tampered_mic_error() {
    // flip a bit in the first MIC byte
    do_bad_deserialize_tampered(
        ShortV1Salt::from([3; SHORT_SALT_LEN]).into(),
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
fn deserialize_mic_encrypted_short_salt_tampered_payload_error() {
    // flip the last payload bit
    do_bad_deserialize_tampered(
        ShortV1Salt::from([3; SHORT_SALT_LEN]).into(),
        DeserializeError::IdentityResolutionOrDeserializationError(
            MicVerificationError::MicMismatch.into(),
        ),
        |_| {},
        |adv| {
            let before_mic = adv.len() - 17;
            adv[before_mic] ^= 0x01
        },
    );
}

#[test]
fn arena_out_of_space_on_mic_verify() {
    let identity_token = V1IdentityToken::from([1; 16]);
    let key_seed = [2; 32];
    let section_salt = ExtendedV1Salt::from([3; 16]);
    let broadcast_cm = V1BroadcastCredential::new(
        key_seed,
        identity_token,
        ed25519::PrivateKey::generate::<Ed25519ProviderImpl>(),
    );

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
    let mut section_builder = adv_builder
        .section_builder(MicEncryptedSectionEncoder::<_>::new::<CryptoProviderImpl>(
            section_salt,
            &broadcast_cm,
        ))
        .unwrap();

    let txpower_de = TxPowerDataElement::from(TxPower::try_from(5).unwrap());
    section_builder.add_de(|_| txpower_de.clone()).unwrap();
    section_builder.add_to_advertisement::<CryptoProviderImpl>();
    let adv = adv_builder.into_advertisement();

    let (remaining, header) = NpVersionHeader::parse(adv.as_slice()).unwrap();

    let adv_header = if let NpVersionHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };

    let sections = parse_sections(adv_header, remaining).unwrap();
    assert_eq!(1, sections.len());

    let section = sections.into_iter().next().unwrap();
    let enc_section = section.as_ciphertext().unwrap();

    let contents = if let CiphertextSection::MicEncrypted(contents) = &enc_section {
        contents
    } else {
        panic!("incorrect flavor");
    };

    let private_key = ed25519::PrivateKey::generate::<Ed25519ProviderImpl>();
    // deserializing to Section works
    let discovery_credential = V1BroadcastCredential::new(key_seed, identity_token, private_key)
        .derive_discovery_credential::<CryptoProviderImpl>();

    let arena = deserialization_arena!();
    let mut allocator = arena.into_allocator();
    let _ = allocator.allocate(250).unwrap();
    let res = contents.try_resolve_identity_and_deserialize::<CryptoProviderImpl>(
        &mut allocator,
        &discovery_credential,
    );
    assert_eq!(
        Err(IdentityResolutionOrDeserializationError::DeserializationError(
            DeserializationError::ArenaOutOfSpace
        )),
        res
    );
}

/// Attempt a decryption that will fail when using the provided parameters for decryption only.
/// `None` means use the correct value for that parameter.
fn do_bad_deserialize_params<C: CryptoProvider>(
    salt: MultiSalt,
    error: IdentityResolutionOrDeserializationError<MicVerificationError>,
    mut mangle_crypto_material: impl FnMut(&mut PrecalculatedV1DiscoveryCryptoMaterial),
) {
    let identity_token = V1IdentityToken([1; 16]);
    let key_seed = [2; 32];
    let key_seed_hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&key_seed);

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let broadcast_cm = V1BroadcastCredential::new(
        key_seed,
        identity_token,
        ed25519::PrivateKey::generate::<Ed25519ProviderImpl>(),
    );

    let mut section_builder = adv_builder
        .section_builder(MicEncryptedSectionEncoder::<_>::new::<CryptoProviderImpl>(
            salt,
            &broadcast_cm,
        ))
        .unwrap();

    section_builder.add_de(|_| TxPowerDataElement::from(TxPower::try_from(7).unwrap())).unwrap();

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
    let contents = if let CiphertextSection::MicEncrypted(contents) = &enc_section {
        contents
    } else {
        panic!("incorrect flavor");
    };

    // start with correct crypto material
    let mut crypto_material = V1DiscoveryCredential::new(
        key_seed,
        key_seed_hkdf
            .v1_mic_short_salt_keys()
            .identity_token_hmac_key()
            .calculate_hmac::<C>(&identity_token.0),
        key_seed_hkdf
            .v1_mic_extended_salt_keys()
            .identity_token_hmac_key()
            .calculate_hmac::<C>(&identity_token.0),
        key_seed_hkdf
            .v1_signature_keys()
            .identity_token_hmac_key()
            .calculate_hmac::<C>(&identity_token.0),
        crypto_provider::ed25519::PrivateKey::generate::<C::Ed25519>()
            .derive_public_key::<C::Ed25519>(),
    )
    .to_precalculated::<C>();

    // then break it per the test case
    mangle_crypto_material(&mut crypto_material);

    assert_eq!(
        error,
        contents
            .try_resolve_identity_and_deserialize::<C>(
                &mut deserialization_arena!().into_allocator(),
                &crypto_material
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
    salt: MultiSalt,
    expected_error: DeserializeError,
    mangle_section: impl Fn(&mut CapacityLimitedVec<u8, NP_ADV_MAX_SECTION_LEN>),
    mangle_adv: impl Fn(&mut Vec<u8>),
) {
    let metadata_key = V1IdentityToken([1; 16]);
    let key_seed = [2; 32];

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let broadcast_cm = V1BroadcastCredential::new(
        key_seed,
        metadata_key,
        ed25519::PrivateKey::generate::<Ed25519ProviderImpl>(),
    );

    let mut section_builder = adv_builder
        .section_builder(MicEncryptedSectionEncoder::<_>::new::<CryptoProviderImpl>(
            salt,
            &broadcast_cm,
        ))
        .unwrap();

    section_builder.add_de(|_| TxPowerDataElement::from(TxPower::try_from(7).unwrap())).unwrap();

    mangle_section(&mut section_builder.section);

    section_builder.add_to_advertisement::<CryptoProviderImpl>();

    let adv = adv_builder.into_advertisement();
    let mut adv_mut = adv.as_slice().to_vec();
    mangle_adv(&mut adv_mut);

    let (remaining, header) = NpVersionHeader::parse(&adv_mut).unwrap();

    let v1_header = if let NpVersionHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };

    let sections = parse_sections(v1_header, remaining).unwrap();
    assert_eq!(1, sections.len());

    let section = sections.into_iter().next().unwrap();
    let enc_section = section.as_ciphertext().unwrap();
    let contents = if let CiphertextSection::MicEncrypted(contents) = &enc_section {
        contents
    } else {
        panic!("incorrect flavor");
    };

    // generate a random key pair since we need _some_ public key in our discovery
    // credential, even if it winds up going unused
    let private_key = ed25519::PrivateKey::generate::<Ed25519ProviderImpl>();

    let discovery_credential = V1BroadcastCredential::new(key_seed, metadata_key, private_key)
        .derive_discovery_credential::<CryptoProviderImpl>();

    match contents.try_resolve_identity_and_deserialize::<CryptoProviderImpl>(
        &mut deserialization_arena!().into_allocator(),
        &discovery_credential,
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
