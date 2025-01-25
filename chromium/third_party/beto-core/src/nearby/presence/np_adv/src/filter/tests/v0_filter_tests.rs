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

#![allow(clippy::unwrap_used)]

use super::super::*;
use crate::credential::book::CredentialBookBuilder;
use crate::credential::matched::{KeySeedMatchedCredential, ReferencedMatchedCredential};
use crate::credential::v0::{V0DiscoveryCredential, V0};
use crate::credential::{v0::V0BroadcastCredential, MatchableCredential};
use crate::filter::IdentityFilterType::{Any, Private, Public};
use crate::legacy::data_elements::tx_power::TxPowerDataElement;
use crate::legacy::serialize::{AdvBuilder, LdtEncoder};
use crate::shared_data::TxPower;
use alloc::vec::Vec;
use crypto_provider_default::CryptoProviderImpl;
use ldt_np_adv::V0_IDENTITY_TOKEN_LEN;

const IDENTITY_TOKEN: [u8; V0_IDENTITY_TOKEN_LEN] = [0x33; V0_IDENTITY_TOKEN_LEN];
const KEY_SEED: [u8; 32] = [0x11_u8; 32];

const PUBLIC_IDENTITY_V0_ADV_CONTENTS: [u8; 2] = [
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

    let result = filter.match_v0_adv::<_, CryptoProviderImpl>(
        V0Encoding::Unencrypted,
        &cred_book,
        &PUBLIC_IDENTITY_V0_ADV_CONTENTS,
    );

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

    let result = filter.match_v0_adv::<_, CryptoProviderImpl>(
        V0Encoding::Ldt,
        &cred_book,
        &PUBLIC_IDENTITY_V0_ADV_CONTENTS,
    );

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

    let result = filter.match_v0_adv::<_, CryptoProviderImpl>(
        V0Encoding::Unencrypted,
        &cred_book,
        &PUBLIC_IDENTITY_V0_ADV_CONTENTS,
    );

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

    let result = filter.match_v0_adv::<_, CryptoProviderImpl>(
        V0Encoding::Ldt,
        &cred_book,
        v0_adv_contents().as_slice(),
    );

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
        hkdf.v0_identity_token_hmac_key().calculate_hmac::<CryptoProviderImpl>(&IDENTITY_TOKEN);
    let match_data: KeySeedMatchedCredential = KEY_SEED.into();
    let v0_creds: [MatchableCredential<V0, KeySeedMatchedCredential>; 1] = [MatchableCredential {
        discovery_credential: V0DiscoveryCredential::new(KEY_SEED, metadata_key_hmac),
        match_data: match_data.clone(),
    }];

    let cred_book = CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&v0_creds, &[]);

    let result = filter.match_v0_adv::<_, CryptoProviderImpl>(
        V0Encoding::Ldt,
        &cred_book,
        v0_adv_contents().as_slice(),
    );

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
        hkdf.v0_identity_token_hmac_key().calculate_hmac::<CryptoProviderImpl>(&IDENTITY_TOKEN);
    let match_data: KeySeedMatchedCredential = KEY_SEED.into();
    let v0_creds: [MatchableCredential<V0, KeySeedMatchedCredential>; 1] = [MatchableCredential {
        discovery_credential: V0DiscoveryCredential::new(KEY_SEED, metadata_key_hmac),
        match_data: match_data.clone(),
    }];

    let cred_book = CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&v0_creds, &[]);

    let result = filter.match_v0_adv::<_, CryptoProviderImpl>(
        V0Encoding::Ldt,
        &cred_book,
        v0_adv_contents().as_slice(),
    );

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
        hkdf.v0_identity_token_hmac_key().calculate_hmac::<CryptoProviderImpl>(&IDENTITY_TOKEN);
    let v0_creds: [MatchableCredential<V0, KeySeedMatchedCredential>; 1] = [MatchableCredential {
        discovery_credential: V0DiscoveryCredential::new(key_seed, metadata_key_hmac),
        match_data: KEY_SEED.into(),
    }];

    let cred_book = CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&v0_creds, &[]);

    let result = filter.match_v0_adv::<_, CryptoProviderImpl>(
        V0Encoding::Ldt,
        &cred_book,
        v0_adv_contents().as_slice(),
    );

    assert_eq!(result, Err(NoMatch));
}

#[test]
fn test_contains_private_identity_invalid_hmac_match() {
    let filter = V0Filter {
        identity: Private,
        data_elements: V0DataElementsFilter { contains_tx_power: None, actions_filter: None },
    };

    let v0_creds: [MatchableCredential<V0, KeySeedMatchedCredential>; 1] = [MatchableCredential {
        discovery_credential: V0DiscoveryCredential::new(KEY_SEED, [0u8; 32]),
        match_data: KEY_SEED.into(),
    }];

    let cred_book = CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&v0_creds, &[]);

    let result = filter.match_v0_adv::<_, CryptoProviderImpl>(
        V0Encoding::Ldt,
        &cred_book,
        v0_adv_contents().as_slice(),
    );

    assert_eq!(result, Err(NoMatch));
}

/// Returns the contents of an advertisement after the version header.
fn v0_adv_contents() -> Vec<u8> {
    let broadcast_cm = V0BroadcastCredential::new(KEY_SEED, IDENTITY_TOKEN.into());
    let mut builder =
        AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new([0x22; 2].into(), &broadcast_cm));

    builder.add_data_element(TxPowerDataElement::from(TxPower::try_from(3).unwrap())).unwrap();
    builder.into_advertisement().unwrap().as_slice()[1..].to_vec()
}
