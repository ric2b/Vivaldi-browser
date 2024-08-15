// Copyright 2023 Google LLC
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

use super::super::*;
use crate::credential::book::CredentialBookBuilder;
use crate::credential::v0::{V0DiscoveryCredential, V0};
use crate::credential::{
    KeySeedMatchedCredential, MatchableCredential, ReferencedMatchedCredential,
};
use crate::filter::IdentityFilterType::{Any, Private, Public};
use crypto_provider_default::CryptoProviderImpl;
use ldt_np_adv::NP_LEGACY_METADATA_KEY_LEN;

const METADATA_KEY: [u8; NP_LEGACY_METADATA_KEY_LEN] = [0x33; NP_LEGACY_METADATA_KEY_LEN];
const KEY_SEED: [u8; 32] = [0x11_u8; 32];
const PRIVATE_IDENTITY_V0_ADV_CONTENTS: [u8; 19] = [
    0x21, // private DE
    0x22, 0x22, // salt
    // ciphertext
    0x85, 0xBF, 0xA8, 0x83, 0x58, 0x7C, 0x50, 0xCF, 0x98, 0x38, 0xA7, 0x8A, 0xC0, 0x1C, 0x96, 0xF9,
];

const PUBLIC_IDENTITY_V0_ADV_CONTENTS: [u8; 3] = [
    0x03, // public DE
    0x16, 0x00, // actions
];

#[test]
fn test_contains_public_identity() {
    let filter = V0Filter {
        identity: Public,
        data_elements: V0DataElementsFilter { contains_tx_power: None, actions_filter: None },
    };

    let cred_book = CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&[], &[]);

    let result =
        filter.match_v0_adv::<_, CryptoProviderImpl>(&cred_book, &PUBLIC_IDENTITY_V0_ADV_CONTENTS);

    assert_eq!(result, Ok(FilterResult::Public));
}

#[test]
fn test_not_contains_private() {
    let filter = V0Filter {
        identity: Private,
        data_elements: V0DataElementsFilter { contains_tx_power: None, actions_filter: None },
    };

    let cred_book = CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&[], &[]);

    let result =
        filter.match_v0_adv::<_, CryptoProviderImpl>(&cred_book, &PUBLIC_IDENTITY_V0_ADV_CONTENTS);

    assert_eq!(result, Err(NoMatch));
}

#[test]
fn test_contains_any_public() {
    let filter = V0Filter {
        identity: Any,
        data_elements: V0DataElementsFilter { contains_tx_power: None, actions_filter: None },
    };

    let cred_book = CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&[], &[]);

    let result =
        filter.match_v0_adv::<_, CryptoProviderImpl>(&cred_book, &PUBLIC_IDENTITY_V0_ADV_CONTENTS);

    assert_eq!(result, Ok(FilterResult::Public));
}

#[test]
fn test_not_contains_public_identity() {
    let filter = V0Filter {
        identity: Public,
        data_elements: V0DataElementsFilter { contains_tx_power: None, actions_filter: None },
    };

    let cred_book = CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&[], &[]);

    let result =
        filter.match_v0_adv::<_, CryptoProviderImpl>(&cred_book, &PRIVATE_IDENTITY_V0_ADV_CONTENTS);

    assert_eq!(result, Err(NoMatch));
}

#[test]
fn test_contains_private_identity() {
    let filter = V0Filter {
        identity: Private,
        data_elements: V0DataElementsFilter { contains_tx_power: None, actions_filter: None },
    };

    let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&KEY_SEED);
    let metadata_key_hmac: [u8; 32] =
        hkdf.legacy_metadata_key_hmac_key().calculate_hmac(&METADATA_KEY);
    let discovery_credential = V0DiscoveryCredential::new(KEY_SEED, metadata_key_hmac);
    let match_data: KeySeedMatchedCredential = KEY_SEED.into();
    let v0_creds: [MatchableCredential<V0, KeySeedMatchedCredential>; 1] =
        [MatchableCredential { discovery_credential, match_data: match_data.clone() }];

    let cred_book = CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&v0_creds, &[]);

    let result =
        filter.match_v0_adv::<_, CryptoProviderImpl>(&cred_book, &PRIVATE_IDENTITY_V0_ADV_CONTENTS);

    assert_eq!(result, Ok(FilterResult::Private(ReferencedMatchedCredential::from(&match_data))));
}

#[test]
fn test_contains_any_private_identity() {
    let filter = V0Filter {
        identity: Any,
        data_elements: V0DataElementsFilter { contains_tx_power: None, actions_filter: None },
    };

    let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&KEY_SEED);
    let metadata_key_hmac: [u8; 32] =
        hkdf.legacy_metadata_key_hmac_key().calculate_hmac(&METADATA_KEY);
    let discovery_credential = V0DiscoveryCredential::new(KEY_SEED, metadata_key_hmac);
    let match_data: KeySeedMatchedCredential = KEY_SEED.into();
    let v0_creds: [MatchableCredential<V0, KeySeedMatchedCredential>; 1] =
        [MatchableCredential { discovery_credential, match_data: match_data.clone() }];

    let cred_book = CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&v0_creds, &[]);

    let result =
        filter.match_v0_adv::<_, CryptoProviderImpl>(&cred_book, &PRIVATE_IDENTITY_V0_ADV_CONTENTS);

    assert_eq!(result, Ok(FilterResult::Private(ReferencedMatchedCredential::from(&match_data))));
}

#[test]
fn test_contains_private_identity_no_matching_credential() {
    let filter = V0Filter {
        identity: Private,
        data_elements: V0DataElementsFilter { contains_tx_power: None, actions_filter: None },
    };

    let mut key_seed = [0u8; 32];
    key_seed.copy_from_slice(&KEY_SEED);
    // change one byte
    key_seed[0] = 0x00;

    let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
    let metadata_key_hmac: [u8; 32] =
        hkdf.legacy_metadata_key_hmac_key().calculate_hmac(&METADATA_KEY);
    let discovery_credential = V0DiscoveryCredential::new(key_seed, metadata_key_hmac);
    let v0_creds: [MatchableCredential<V0, KeySeedMatchedCredential>; 1] =
        [MatchableCredential { discovery_credential, match_data: KEY_SEED.into() }];

    let cred_book = CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&v0_creds, &[]);

    let result =
        filter.match_v0_adv::<_, CryptoProviderImpl>(&cred_book, &PRIVATE_IDENTITY_V0_ADV_CONTENTS);

    assert_eq!(result, Err(NoMatch));
}

#[test]
fn test_contains_private_identity_invalid_hmac_match() {
    let filter = V0Filter {
        identity: Private,
        data_elements: V0DataElementsFilter { contains_tx_power: None, actions_filter: None },
    };

    let discovery_credential = V0DiscoveryCredential::new(KEY_SEED, [0u8; 32]);
    let v0_creds: [MatchableCredential<V0, KeySeedMatchedCredential>; 1] =
        [MatchableCredential { discovery_credential, match_data: KEY_SEED.into() }];

    let cred_book = CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&v0_creds, &[]);

    let result =
        filter.match_v0_adv::<_, CryptoProviderImpl>(&cred_book, &PRIVATE_IDENTITY_V0_ADV_CONTENTS);

    assert_eq!(result, Err(NoMatch));
}
