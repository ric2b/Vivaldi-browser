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

use crypto_provider::{ed25519, CryptoProvider, CryptoRng};
use crypto_provider_default::CryptoProviderImpl;
use np_adv::credential::matched::{
    EmptyMatchedCredential, MetadataMatchedCredential, WithMatchedCredential,
};
use np_adv::extended::deserialize::{Section, V1DeserializedSection};
use np_adv::extended::{V1IdentityToken, V1_ENCODING_UNENCRYPTED};
use np_adv::{
    credential::{
        book::CredentialBookBuilder,
        v1::{V1BroadcastCredential, V1DiscoveryCredential, V1},
        MatchableCredential,
    },
    deserialization_arena, deserialize_advertisement,
    extended::{
        data_elements::TxPowerDataElement,
        deserialize::VerificationMode,
        serialize::{
            AdvBuilder, AdvertisementType, SignedEncryptedSectionEncoder, SingleTypeDataElement,
            UnencryptedSectionEncoder,
        },
        NP_V1_ADV_MAX_SECTION_COUNT,
    },
    shared_data::TxPower,
    AdvDeserializationError, AdvDeserializationErrorDetailsHazmat,
};
use np_hkdf::{v1_salt, DerivedSectionKeys};
use serde::{Deserialize, Serialize};

type Ed25519ProviderImpl = <CryptoProviderImpl as CryptoProvider>::Ed25519;

#[test]
fn v1_deser_plaintext() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let mut section_builder = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();
    section_builder
        .add_de(|_salt| TxPowerDataElement::from(TxPower::try_from(6).unwrap()))
        .unwrap();
    section_builder.add_to_advertisement::<CryptoProviderImpl>();
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
    let data_elements = section.iter_data_elements().collect::<Result<Vec<_>, _>>().unwrap();
    assert_eq!(1, data_elements.len());

    let de = &data_elements[0];
    assert_eq!(v1_salt::DataElementOffset::from(0), de.offset());
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
    let token_array: [u8; 16] = rng.gen();
    let identity_token = V1IdentityToken::from(token_array);
    let private_key = ed25519::PrivateKey::generate::<Ed25519ProviderImpl>();
    let key_seed = rng.gen();
    let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);

    let broadcast_cred = V1BroadcastCredential::new(key_seed, identity_token, private_key.clone());

    // Serialize and encrypt some identity metadata (sender-side)
    let sender_metadata = IdentityMetadata {
        uuid: "378845e1-2616-420d-86f5-674177a7504d".to_string(),
        display_name: "Alice".to_string(),
        location: "Wonderland".to_string(),
    };
    let sender_metadata_bytes = sender_metadata.to_bytes();
    let encrypted_sender_metadata = MetadataMatchedCredential::<Vec<u8>>::encrypt_from_plaintext::<
        V1,
        CryptoProviderImpl,
    >(&hkdf, identity_token, &sender_metadata_bytes);

    // prepare advertisement
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let mut section_builder = adv_builder
        .section_builder(SignedEncryptedSectionEncoder::new_random_salt::<CryptoProviderImpl>(
            &mut rng,
            &broadcast_cred,
        ))
        .unwrap();
    section_builder
        .add_de(|_salt| TxPowerDataElement::from(TxPower::try_from(7).unwrap()))
        .unwrap();
    section_builder.add_to_advertisement::<CryptoProviderImpl>();
    let adv = adv_builder.into_advertisement();

    let discovery_credential = V1DiscoveryCredential::new(
        key_seed,
        [0; 32],
        [0; 32], // Zeroing out MIC HMAC, since it's unused in examples here.
        hkdf.v1_signature_keys()
            .identity_token_hmac_key()
            .calculate_hmac::<CryptoProviderImpl>(identity_token.bytes()),
        private_key.derive_public_key::<Ed25519ProviderImpl>(),
    );

    let credentials: [MatchableCredential<V1, MetadataMatchedCredential<_>>; 1] =
        [MatchableCredential {
            discovery_credential,
            match_data: encrypted_sender_metadata.clone(),
        }];
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

    assert_eq!(VerificationMode::Signature, section.verification_mode());
    assert_eq!(&identity_token, section.identity_token());

    let data_elements = section.iter_data_elements().collect::<Result<Vec<_>, _>>().unwrap();
    assert_eq!(1, data_elements.len());

    let de = &data_elements[0];
    assert_eq!(v1_salt::DataElementOffset::from(0), de.offset());
    assert_eq!(TxPowerDataElement::DE_TYPE, de.de_type());
    assert_eq!(&[7], de.contents());

    // Uncomment if you need to regenerate C++ v1_private_identity_tests data
    // {
    //     use test_helper::hex_bytes;
    //     use np_adv::extended::salt::MultiSalt;
    //     use np_adv_credential_matched::MatchedCredential;
    //     println!("adv:\n{}", hex_bytes(adv.as_slice()));
    //     println!("key seed:\n{}", hex_bytes(key_seed));
    //     println!(
    //         "identity token hmac:\n{}",
    //         hex_bytes(
    //             hkdf.v1_signature_keys()
    //                 .identity_token_hmac_key()
    //                 .calculate_hmac(identity_token.bytes())
    //         )
    //     );
    //     println!("public key:\n{}", hex_bytes(key_pair.public().to_bytes()));
    //     println!(
    //         "encrypted metadata:\n{}",
    //         hex_bytes(encrypted_sender_metadata.fetch_encrypted_metadata().unwrap())
    //     );
    //     std::println!("offset is: {:?}", de.offset());
    //     let derived_salt = match section.salt() {
    //         MultiSalt::Short(_) => panic!(),
    //         MultiSalt::Extended(s) => {
    //             s.derive::<16, CryptoProviderImpl>(Some(de.offset())).unwrap()
    //         }
    //     };
    //     println!("DE derived salt:\n{}", hex_bytes(derived_salt));
    //     panic!();
    // }
}

#[test]
fn v1_deser_no_section() {
    // TODO: we shouldn't allow this invalid advertisement to be serialized
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
    for _ in 0..NP_V1_ADV_MAX_SECTION_COUNT {
        let mut section_builder = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();
        section_builder
            .add_de(|_salt| TxPowerDataElement::from(TxPower::try_from(7).unwrap()))
            .unwrap();
        section_builder.add_to_advertisement::<CryptoProviderImpl>();
    }
    let mut adv = adv_builder.into_advertisement().as_slice().to_vec();
    // Push an extra section
    adv.extend_from_slice(
        [
            0x01, // Section header
            V1_ENCODING_UNENCRYPTED,
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
