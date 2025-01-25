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

use crate::credential::book::CredentialBookBuilder;
use crate::credential::matched::KeySeedMatchedCredential;
use crate::extended::V1_ENCODING_UNENCRYPTED;
use crate::filter::IdentityFilterType::Any;
use crate::filter::{
    FilterOptions, FilterResult, NoMatch, V0DataElementsFilter, V0Filter, V1Filter,
};
use crate::header::{VERSION_HEADER_V0_UNENCRYPTED, VERSION_HEADER_V1};
use crypto_provider_default::CryptoProviderImpl;

mod actions_filter_tests;
mod data_elements_filter_tests;
mod v0_filter_tests;

#[test]
fn top_level_match_v0_adv() {
    let v0_filter = V0Filter {
        identity: Any,
        data_elements: V0DataElementsFilter { contains_tx_power: None, actions_filter: None },
    };

    let empty_cred_book =
        CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
            0,
            0,
            CryptoProviderImpl,
        >(&[], &[]);

    let filter = FilterOptions::V0FilterOptions(v0_filter);
    let result = filter.match_advertisement::<_, CryptoProviderImpl>(
        &empty_cred_book,
        &[
            VERSION_HEADER_V0_UNENCRYPTED,
            0x16,
            0x00, // actions
        ],
    );

    assert_eq!(Ok(FilterResult::Public), result);
}

#[test]
fn top_level_match_v0_adv_either() {
    let v0_filter = V0Filter {
        identity: Any,
        data_elements: V0DataElementsFilter { contains_tx_power: None, actions_filter: None },
    };

    let empty_cred_book =
        CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
            0,
            0,
            CryptoProviderImpl,
        >(&[], &[]);

    let filter = FilterOptions::Either(v0_filter, V1Filter {});
    let result = filter.match_advertisement::<_, CryptoProviderImpl>(
        &empty_cred_book,
        &[
            VERSION_HEADER_V0_UNENCRYPTED,
            0x16,
            0x00, // actions
        ],
    );

    assert_eq!(Ok(FilterResult::Public), result);
}

#[test]
fn top_level_no_match_v0_adv() {
    let v0_filter = V0Filter {
        identity: Any,
        data_elements: V0DataElementsFilter { contains_tx_power: None, actions_filter: None },
    };

    let empty_cred_book =
        CredentialBookBuilder::<KeySeedMatchedCredential>::build_cached_slice_book::<
            0,
            0,
            CryptoProviderImpl,
        >(&[], &[]);

    let filter = FilterOptions::V0FilterOptions(v0_filter);
    let result = filter.match_advertisement::<_, CryptoProviderImpl>(
        &empty_cred_book,
        &[
            VERSION_HEADER_V1,
            0x03, // Section Header
            V1_ENCODING_UNENCRYPTED,
            0x15,
            0x03, // Length 1 Tx Power DE with value 3
        ],
    );

    assert_eq!(Err(NoMatch), result);
}
