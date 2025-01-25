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

use super::*;
use crate::credential::v0::V0;
use crate::credential::v1::V1;
use crate::extended::V1IdentityToken;
use alloc::vec;
use crypto_provider_default::CryptoProviderImpl;
use ldt_np_adv::V0IdentityToken;

#[test]
fn v0_metadata_decryption_works_same_metadata_key() {
    let key_seed = [3u8; 32];
    let identity_token = V0IdentityToken::from([5u8; 14]);

    let metadata = vec![7u8; 42];

    let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let encrypted_metadata =
        encrypt_metadata::<CryptoProviderImpl, V0>(&hkdf, identity_token, &metadata);

    let decryption_result =
        decrypt_metadata::<CryptoProviderImpl, V0>(&hkdf, identity_token, &encrypted_metadata);
    assert_eq!(decryption_result, Ok(metadata))
}

#[test]
fn v1_metadata_decryption_works_same_metadata_key() {
    let key_seed = [9u8; 32];
    let identity_token = V1IdentityToken::from([2u8; 16]);

    let metadata = vec![6u8; 51];

    let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let encrypted_metadata =
        encrypt_metadata::<CryptoProviderImpl, V1>(&hkdf, identity_token, &metadata);

    let decryption_result =
        decrypt_metadata::<CryptoProviderImpl, V1>(&hkdf, identity_token, &encrypted_metadata);
    assert_eq!(decryption_result, Ok(metadata))
}

#[test]
fn v0_metadata_decryption_fails_different_metadata_key() {
    let key_seed = [3u8; 32];
    let identity_token = V0IdentityToken::from([5u8; 14]);

    let metadata = vec![7u8; 42];

    let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let encrypted_metadata =
        encrypt_metadata::<CryptoProviderImpl, V0>(&hkdf, identity_token, &metadata);

    let decrypting_identity_token = V0IdentityToken::from([6u8; 14]);

    let decryption_result = decrypt_metadata::<CryptoProviderImpl, V0>(
        &hkdf,
        decrypting_identity_token,
        &encrypted_metadata,
    );
    assert_eq!(decryption_result, Err(MetadataDecryptionError))
}

#[test]
fn v1_metadata_decryption_fails_different_metadata_key() {
    let key_seed = [251u8; 32];
    let identity_token = V1IdentityToken::from([127u8; 16]);

    let metadata = vec![255u8; 42];

    let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let encrypted_metadata =
        encrypt_metadata::<CryptoProviderImpl, V1>(&hkdf, identity_token, &metadata);

    let decrypting_identity_token = V1IdentityToken::from([249u8; 16]);

    let decryption_result = decrypt_metadata::<CryptoProviderImpl, V1>(
        &hkdf,
        decrypting_identity_token,
        &encrypted_metadata,
    );
    assert_eq!(decryption_result, Err(MetadataDecryptionError))
}
