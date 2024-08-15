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

//! Tests of functionality related to credentials, credential-views, and credential suppliers.

extern crate alloc;

use crate::credential::{
    book::{
        init_cache_from_source, CachedCredentialSource, PossiblyCachedDiscoveryCryptoMaterialKind,
    },
    source::{CredentialSource, SliceCredentialSource},
    v0::{V0DiscoveryCredential, V0},
    v1::{
        SignedBroadcastCryptoMaterial, SimpleSignedBroadcastCryptoMaterial, V1DiscoveryCredential,
        V1DiscoveryCryptoMaterial, V1,
    },
    BroadcastCryptoMaterial, EmptyMatchedCredential, KeySeedMatchedCredential, MatchableCredential,
    MetadataDecryptionError, ProtocolVersion, ReferencedMatchedCredential,
    SimpleBroadcastCryptoMaterial,
};
use crate::legacy::ShortMetadataKey;
use crate::MetadataKey;
use alloc::{vec, vec::Vec};
use crypto_provider_default::CryptoProviderImpl;

fn get_zeroed_v0_discovery_credential() -> V0DiscoveryCredential {
    V0DiscoveryCredential::new([0u8; 32], [0u8; 32])
}

fn get_constant_packed_v1_discovery_credential(value: u8) -> V1DiscoveryCredential {
    let key_pair = np_ed25519::KeyPair::<CryptoProviderImpl>::generate();
    SimpleSignedBroadcastCryptoMaterial::new(
        [value; 32],
        MetadataKey([value; 16]),
        // NOTE: This winds up being unused in these test cases
        key_pair.private_key(),
    )
    .derive_v1_discovery_credential::<CryptoProviderImpl>()
}

#[test]
fn cached_credential_source_keeps_same_entries_as_original() {
    let creds: [MatchableCredential<V1, KeySeedMatchedCredential>; 5] =
        [0u8, 1, 2, 3, 4].map(|x| {
            let match_data = KeySeedMatchedCredential::from([x; 32]);
            MatchableCredential {
                discovery_credential: get_constant_packed_v1_discovery_credential(x),
                match_data,
            }
        });
    let supplier = SliceCredentialSource::new(&creds);
    let cache = init_cache_from_source::<_, _, 3, CryptoProviderImpl>(&supplier);
    let cached = CachedCredentialSource::new(supplier, cache);
    let cached_view = &cached;
    assert_eq!(cached_view.iter().count(), 5);
    // Now we're going to check that the pairings between the match-data
    // and the MIC hmac key wind up being the same between the original
    // creds list and what's provided by the cached source.
    let expected: Vec<_> = creds
        .iter()
        .map(|cred| {
            (
                cred.discovery_credential
                    .unsigned_verification_material::<CryptoProviderImpl>()
                    .mic_hmac_key,
                ReferencedMatchedCredential::from(&cred.match_data),
            )
        })
        .collect();
    let actual: Vec<_> = cached_view
        .iter()
        .map(|(crypto_material, match_data)| {
            (
                crypto_material.unsigned_verification_material::<CryptoProviderImpl>().mic_hmac_key,
                match_data,
            )
        })
        .collect();
    assert_eq!(actual, expected);
}

#[test]
fn cached_credential_source_has_requested_cache_size() {
    let creds: [MatchableCredential<V0, EmptyMatchedCredential>; 10] =
        [0u8; 10].map(|_| MatchableCredential {
            discovery_credential: get_zeroed_v0_discovery_credential(),
            match_data: EmptyMatchedCredential,
        });
    let supplier = SliceCredentialSource::new(&creds);
    let cache = init_cache_from_source::<_, _, 5, CryptoProviderImpl>(&supplier);
    let cached = CachedCredentialSource::new(supplier, cache);
    let cached_view = &cached;
    assert_eq!(cached_view.iter().count(), 10);
    for (i, (cred, _)) in cached_view.iter().enumerate() {
        if i < 5 {
            // Should be cached
            if let PossiblyCachedDiscoveryCryptoMaterialKind::Precalculated(_) = cred.wrapped {
            } else {
                panic!("Credential #{} was not cached", i);
            }
        } else {
            // Should be discovery credentials
            if let PossiblyCachedDiscoveryCryptoMaterialKind::Discovery(_) = cred.wrapped {
            } else {
                panic!("Credential #{} was not supposed to be cached", i);
            }
        }
    }
}

#[test]
fn v0_metadata_decryption_works_same_metadata_key() {
    let key_seed = [3u8; 32];
    let metadata_key = ShortMetadataKey([5u8; 14]);

    let metadata = vec![7u8; 42];

    let broadcast_cm = SimpleBroadcastCryptoMaterial::<V0>::new(key_seed, metadata_key);

    let encrypted_metadata = broadcast_cm.encrypt_metadata::<CryptoProviderImpl>(&metadata);

    let metadata_nonce = broadcast_cm.metadata_nonce::<CryptoProviderImpl>();

    let decryption_result = V0::decrypt_metadata::<CryptoProviderImpl>(
        metadata_nonce,
        metadata_key,
        &encrypted_metadata,
    );
    assert_eq!(decryption_result, Ok(metadata))
}

#[test]
fn v1_metadata_decryption_works_same_metadata_key() {
    let key_seed = [9u8; 32];
    let metadata_key = MetadataKey([2u8; 16]);

    let metadata = vec![6u8; 51];

    let broadcast_cm = SimpleBroadcastCryptoMaterial::<V1>::new(key_seed, metadata_key);

    let encrypted_metadata = broadcast_cm.encrypt_metadata::<CryptoProviderImpl>(&metadata);

    let metadata_nonce = broadcast_cm.metadata_nonce::<CryptoProviderImpl>();

    let decryption_result = V1::decrypt_metadata::<CryptoProviderImpl>(
        metadata_nonce,
        metadata_key,
        &encrypted_metadata,
    );
    assert_eq!(decryption_result, Ok(metadata))
}

#[test]
fn v0_metadata_decryption_fails_different_metadata_key() {
    let key_seed = [3u8; 32];
    let encrypting_metadata_key = ShortMetadataKey([5u8; 14]);

    let metadata = vec![7u8; 42];

    let broadcast_cm = SimpleBroadcastCryptoMaterial::<V0>::new(key_seed, encrypting_metadata_key);

    let encrypted_metadata = broadcast_cm.encrypt_metadata::<CryptoProviderImpl>(&metadata);

    let metadata_nonce = broadcast_cm.metadata_nonce::<CryptoProviderImpl>();

    let decrypting_metadata_key = ShortMetadataKey([6u8; 14]);

    let decryption_result = V0::decrypt_metadata::<CryptoProviderImpl>(
        metadata_nonce,
        decrypting_metadata_key,
        &encrypted_metadata,
    );
    assert_eq!(decryption_result, Err(MetadataDecryptionError))
}

#[test]
fn v1_metadata_decryption_fails_different_metadata_key() {
    let key_seed = [251u8; 32];
    let encrypting_metadata_key = MetadataKey([127u8; 16]);

    let metadata = vec![255u8; 42];

    let broadcast_cm = SimpleBroadcastCryptoMaterial::<V1>::new(key_seed, encrypting_metadata_key);

    let encrypted_metadata = broadcast_cm.encrypt_metadata::<CryptoProviderImpl>(&metadata);

    let metadata_nonce = broadcast_cm.metadata_nonce::<CryptoProviderImpl>();

    let decrypting_metadata_key = MetadataKey([249u8; 16]);

    let decryption_result = V1::decrypt_metadata::<CryptoProviderImpl>(
        metadata_nonce,
        decrypting_metadata_key,
        &encrypted_metadata,
    );
    assert_eq!(decryption_result, Err(MetadataDecryptionError))
}
