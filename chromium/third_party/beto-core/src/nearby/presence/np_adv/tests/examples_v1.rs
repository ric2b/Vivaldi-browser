// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#![allow(clippy::unwrap_used, clippy::expect_used, clippy::indexing_slicing, clippy::panic)]

use crypto_provider::{CryptoProvider, CryptoRng};
use crypto_provider_default::CryptoProviderImpl;
use np_adv::extended::data_elements::TxPowerDataElement;
use np_adv::extended::serialize::{AdvertisementType, PublicSectionEncoder, SingleTypeDataElement};
use np_adv::extended::NP_V1_ADV_MAX_PUBLIC_SECTION_COUNT;
use np_adv::shared_data::TxPower;
use np_adv::{
    credential::{
        book::CredentialBookBuilder,
        v1::{SimpleSignedBroadcastCryptoMaterial, V1DiscoveryCredential, V1},
        EmptyMatchedCredential, MatchableCredential, MetadataMatchedCredential,
    },
    de_type::*,
    extended::{
        deserialize::{Section, VerificationMode},
        serialize::{AdvBuilder, SignedEncryptedSectionEncoder},
    },
    PlaintextIdentityMode, *,
};
use np_hkdf::v1_salt;
use serde::{Deserialize, Serialize};

#[test]
fn v1_deser_plaintext() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let mut section_builder = adv_builder.section_builder(PublicSectionEncoder::default()).unwrap();
    section_builder
        .add_de(|_salt| TxPowerDataElement::from(TxPower::try_from(6).unwrap()))
        .unwrap();
    section_builder.add_to_advertisement();
    let adv = adv_builder.into_advertisement();

    let cred_book = CredentialBookBuilder::<EmptyMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&[], &[]);

    let arena = deserialization_arena!();
    let contents =
        deserialize_advertisement::<_, CryptoProviderImpl>(arena, adv.as_slice(), &cred_book)
            .expect("Should be a valid advertisemement")
            .into_v1()
            .expect("Should be V1");

    assert_eq!(0, contents.invalid_sections_count());

    let sections = contents.sections().collect::<Vec<_>>();

    assert_eq!(1, sections.len());

    let section = match &sections[0] {
        V1DeserializedSection::Plaintext(s) => s,
        _ => panic!("this is a plaintext adv"),
    };
    assert_eq!(PlaintextIdentityMode::Public, section.identity());
    let data_elements = section.iter_data_elements().collect::<Result<Vec<_>, _>>().unwrap();
    assert_eq!(1, data_elements.len());

    let de = &data_elements[0];
    assert_eq!(v1_salt::DataElementOffset::from(1), de.offset());
    assert_eq!(TxPowerDataElement::DE_TYPE, de.de_type());
    assert_eq!(&[6], de.contents());
}

/// Sample contents for some encrypted identity metadata
/// which consists of a UUID together with a display name
/// and a general location.
#[derive(Debug, Eq, PartialEq, Serialize, Deserialize)]
struct IdentityMetadata {
    uuid: String,
    display_name: String,
    location: String,
}

impl IdentityMetadata {
    /// Serialize this identity metadata to a json byte-string.
    fn to_bytes(&self) -> Vec<u8> {
        serde_json::to_vec(self).expect("Identity metadata serialization is infallible")
    }
    /// Attempt to deserialize identity metadata from a json byte-string.
    fn try_from_bytes(serialized: &[u8]) -> Option<Self> {
        serde_json::from_slice(serialized).ok()
    }
}

#[test]
fn v1_deser_ciphertext() {
    // identity material
    let mut rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();
    let metadata_key: [u8; 16] = rng.gen();
    let metadata_key = MetadataKey(metadata_key);
    let key_pair = np_ed25519::KeyPair::<CryptoProviderImpl>::generate();
    let key_seed = rng.gen();
    let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);

    let broadcast_cm =
        SimpleSignedBroadcastCryptoMaterial::new(key_seed, metadata_key, key_pair.private_key());

    // Serialize and encrypt some identity metadata (sender-side)
    let sender_metadata = IdentityMetadata {
        uuid: "378845e1-2616-420d-86f5-674177a7504d".to_string(),
        display_name: "Alice".to_string(),
        location: "Wonderland".to_string(),
    };
    let sender_metadata_bytes = sender_metadata.to_bytes();
    let encrypted_sender_metadata = MetadataMatchedCredential::<Vec<u8>>::encrypt_from_plaintext::<
        _,
        _,
        CryptoProviderImpl,
    >(&broadcast_cm, &sender_metadata_bytes);

    // prepare advertisement
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let mut section_builder = adv_builder
        .section_builder(SignedEncryptedSectionEncoder::<CryptoProviderImpl>::new_random_salt(
            &mut rng,
            EncryptedIdentityDataElementType::Private,
            &broadcast_cm,
        ))
        .unwrap();
    section_builder
        .add_de(|_salt| TxPowerDataElement::from(TxPower::try_from(7).unwrap()))
        .unwrap();
    section_builder.add_to_advertisement();
    let adv = adv_builder.into_advertisement();

    let discovery_credential = V1DiscoveryCredential::new(
        key_seed,
        [0; 32], // Zeroing out MIC HMAC, since it's unused in examples here.
        hkdf.extended_signed_metadata_key_hmac_key().calculate_hmac(&metadata_key.0),
        key_pair.public().to_bytes(),
    );

    let credentials: [MatchableCredential<V1, MetadataMatchedCredential<_>>; 1] =
        [MatchableCredential { discovery_credential, match_data: encrypted_sender_metadata }];
    let cred_book = CredentialBookBuilder::build_cached_slice_book::<0, 0, CryptoProviderImpl>(
        &[],
        &credentials,
    );
    let arena = deserialization_arena!();
    let contents =
        deserialize_advertisement::<_, CryptoProviderImpl>(arena, adv.as_slice(), &cred_book)
            .expect("Should be a valid advertisement")
            .into_v1()
            .expect("Should be V1");

    assert_eq!(0, contents.invalid_sections_count());

    let sections = contents.sections().collect::<Vec<_>>();
    assert_eq!(1, sections.len());

    let matched: &WithMatchedCredential<_, _> = match &sections[0] {
        V1DeserializedSection::Decrypted(d) => d,
        _ => panic!("this is a ciphertext adv"),
    };

    let decrypted_metadata_bytes = matched
        .decrypt_metadata::<CryptoProviderImpl>()
        .expect("Sender metadata should be decryptable");
    let decrypted_metadata = IdentityMetadata::try_from_bytes(&decrypted_metadata_bytes)
        .expect("Sender metadata should be deserializable");
    assert_eq!(sender_metadata, decrypted_metadata);

    let section = matched.contents();

    assert_eq!(EncryptedIdentityDataElementType::Private, section.identity_type());
    assert_eq!(VerificationMode::Signature, section.verification_mode());
    assert_eq!(metadata_key, section.metadata_key());

    let data_elements = section.iter_data_elements().collect::<Result<Vec<_>, _>>().unwrap();
    assert_eq!(1, data_elements.len());

    let de = &data_elements[0];
    assert_eq!(v1_salt::DataElementOffset::from(2), de.offset());
    assert_eq!(TxPowerDataElement::DE_TYPE, de.de_type());
    assert_eq!(&[7], de.contents());
}

#[test]
fn v1_deser_no_section() {
    let adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let adv = adv_builder.into_advertisement();
    let cred_book = CredentialBookBuilder::<EmptyMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&[], &[]);
    let arena = deserialization_arena!();
    let v1_deserialize_error =
        deserialize_advertisement::<_, CryptoProviderImpl>(arena, adv.as_slice(), &cred_book)
            .expect_err(" Expected an error");
    assert_eq!(
        v1_deserialize_error,
        AdvDeserializationError::ParseError {
            details_hazmat: AdvDeserializationErrorDetailsHazmat::AdvertisementDeserializeError
        }
    );
}

#[test]
fn v1_deser_plaintext_over_max_sections() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    for _ in 0..NP_V1_ADV_MAX_PUBLIC_SECTION_COUNT {
        let mut section_builder =
            adv_builder.section_builder(PublicSectionEncoder::default()).unwrap();
        section_builder
            .add_de(|_salt| TxPowerDataElement::from(TxPower::try_from(7).unwrap()))
            .unwrap();
        section_builder.add_to_advertisement();
    }
    let mut adv = adv_builder.into_advertisement().as_slice().to_vec();
    // Push an extra section
    adv.extend_from_slice(
        [
            0x01, // Section header
            0x03, // Public identity
        ]
        .as_slice(),
    );
    let cred_book = CredentialBookBuilder::<EmptyMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&[], &[]);
    let arena = deserialization_arena!();
    assert_eq!(
        deserialize_advertisement::<_, CryptoProviderImpl>(arena, adv.as_slice(), &cred_book)
            .unwrap_err(),
        AdvDeserializationError::ParseError {
            details_hazmat: AdvDeserializationErrorDetailsHazmat::AdvertisementDeserializeError
        }
    );
}
