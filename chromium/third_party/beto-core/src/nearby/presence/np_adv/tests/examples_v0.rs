// Copyright 2023 Google LLC
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

use crypto_provider_default::CryptoProviderImpl;
use ldt_np_adv::*;
use np_adv::legacy::data_elements::TxPowerDataElement;
use np_adv::{
    credential::{
        book::CredentialBookBuilder,
        v0::{V0DiscoveryCredential, V0},
        EmptyMatchedCredential, MatchableCredential, MetadataMatchedCredential,
        SimpleBroadcastCryptoMaterial,
    },
    de_type::*,
    legacy::{deserialize::*, ShortMetadataKey},
    shared_data::*,
    *,
};
use serde::{Deserialize, Serialize};

#[test]
fn v0_deser_plaintext() {
    let cred_book = CredentialBookBuilder::<EmptyMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&[], &[]);
    let arena = deserialization_arena!();
    let adv = deserialize_advertisement::<_, CryptoProviderImpl>(
        arena,
        &[
            0x00, // adv header
            0x03, // public identity
            0x15, 0x03, // Length 1 Tx Power DE with value 3
        ],
        &cred_book,
    )
    .expect("Should be a valid advertisement")
    .into_v0()
    .expect("Should be V0");

    match adv {
        V0AdvertisementContents::Plaintext(p) => {
            assert_eq!(PlaintextIdentityMode::Public, p.identity());
            assert_eq!(
                vec![PlainDataElement::TxPower(TxPowerDataElement::from(
                    TxPower::try_from(3).unwrap()
                ))],
                p.data_elements().collect::<Result<Vec<_>, _>>().unwrap()
            );
        }
        _ => panic!("this example is plaintext"),
    }
}

/// Sample contents for some encrypted identity metadata
/// which includes just a name and an e-mail address.
#[derive(Debug, Eq, PartialEq, Serialize, Deserialize)]
struct IdentityMetadata {
    name: String,
    email: String,
}

impl IdentityMetadata {
    /// Serialize this identity metadata to a JSON byte-string.
    fn to_bytes(&self) -> Vec<u8> {
        serde_json::to_vec(&self).expect("serialization should always succeed")
    }
    /// Attempt to deserialize identity metadata from a JSON byte-string.
    fn try_from_bytes(serialized: &[u8]) -> Option<Self> {
        serde_json::from_slice(serialized).ok()
    }
}

#[test]
fn v0_deser_ciphertext() {
    // These are kept fixed in this example for reproducibility.
    // In practice, these should instead be derived from a cryptographically-secure
    // random number generator.
    let key_seed = [0x11_u8; 32];
    let metadata_key: [u8; NP_LEGACY_METADATA_KEY_LEN] = [0x33; NP_LEGACY_METADATA_KEY_LEN];
    let metadata_key = ShortMetadataKey(metadata_key);

    let broadcast_cm = SimpleBroadcastCryptoMaterial::<V0>::new(key_seed, metadata_key);

    let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let metadata_key_hmac: [u8; 32] =
        hkdf.legacy_metadata_key_hmac_key().calculate_hmac(&metadata_key.0);

    // Serialize and encrypt some identity metadata (sender-side)
    let sender_metadata =
        IdentityMetadata { name: "Alice".to_string(), email: "alice@gmail.com".to_string() };
    let sender_metadata_bytes = sender_metadata.to_bytes();
    let encrypted_sender_metadata = MetadataMatchedCredential::<Vec<u8>>::encrypt_from_plaintext::<
        _,
        _,
        CryptoProviderImpl,
    >(&broadcast_cm, &sender_metadata_bytes);

    // output of building a packet using AdvBuilder
    let adv = &[
        0x00, // adv header
        0x21, // private DE w/ a 2 byte payload
        0x22, 0x22, // salt
        // ciphertext for metadata key & txpower DE
        0x85, 0xBF, 0xA8, 0x83, 0x58, 0x7C, 0x50, 0xCF, 0x98, 0x38, 0xA7, 0x8A, 0xC0, 0x1C, 0x96,
        0xF9,
    ];

    let discovery_credential = V0DiscoveryCredential::new(key_seed, metadata_key_hmac);

    let credentials: [MatchableCredential<V0, MetadataMatchedCredential<_>>; 1] =
        [MatchableCredential { discovery_credential, match_data: encrypted_sender_metadata }];

    let cred_book = CredentialBookBuilder::build_cached_slice_book::<0, 0, CryptoProviderImpl>(
        &credentials,
        &[],
    );

    let matched = match deserialize_advertisement::<_, CryptoProviderImpl>(
        deserialization_arena!(),
        adv,
        &cred_book,
    )
    .expect("Should be a valid advertisement")
    .into_v0()
    .expect("Should be V0")
    {
        V0AdvertisementContents::Decrypted(c) => c,
        _ => panic!("this examples is ciphertext"),
    };

    let decrypted_metadata_bytes = matched
        .decrypt_metadata::<CryptoProviderImpl>()
        .expect("Sender metadata should be decryptable");
    let decrypted_metadata = IdentityMetadata::try_from_bytes(&decrypted_metadata_bytes)
        .expect("Sender metadata should be deserializable");

    assert_eq!(sender_metadata, decrypted_metadata);

    let decrypted = matched.contents();

    assert_eq!(EncryptedIdentityDataElementType::Private, decrypted.identity_type());

    assert_eq!(metadata_key, decrypted.metadata_key());

    assert_eq!(
        vec![PlainDataElement::TxPower(TxPowerDataElement::from(TxPower::try_from(3).unwrap())),],
        decrypted.data_elements().collect::<Result<Vec<_>, _>>().unwrap(),
    );
}
