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
use crate::extended::serialize::section_tests::{fill_section_builder, DummyDataElement};
use crate::extended::V1_ENCODING_UNENCRYPTED;
use crypto_provider_default::CryptoProviderImpl;
use std::{prelude::rust_2021::*, vec};

#[test]
fn adv_encode_unencrypted() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);

    let mut public_identity_section_builder =
        adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();
    public_identity_section_builder
        .add_de(|_| DummyDataElement { de_type: 30_u32.into(), data: vec![] })
        .unwrap();

    public_identity_section_builder.add_to_advertisement::<CryptoProviderImpl>();

    assert_eq!(
        &[
            0x20, // NP version header
            V1_ENCODING_UNENCRYPTED,
            0x02, // section len
            0x80, // de header, 0 length
            30,
        ],
        adv_builder.into_advertisement().as_slice()
    )
}

#[test]
fn adding_any_ble5_allowed_section_length_always_works_for_single_section() {
    // up to section len - 2 to leave room for NP version header, section length,
    // and header
    for section_contents_len in 0..=BLE_5_ADV_SVC_MAX_CONTENT_LEN - 3 {
        let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
        let mut section_builder = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();
        fill_section_builder(section_contents_len, &mut section_builder).unwrap();

        section_builder.add_to_advertisement::<CryptoProviderImpl>();

        let adv = adv_builder.into_advertisement();
        assert_eq!(
            section_contents_len + 1 + 1 + 1, // NP version header, section length, section header
            adv.as_slice().len(),
            "adv: {:?}\nsection contents len: {}",
            adv.as_slice(),
            section_contents_len
        );
    }

    // one longer won't fit, though
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let mut section_builder = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();
    assert_eq!(
        AddDataElementError::InsufficientSectionSpace,
        fill_section_builder(BLE_5_ADV_SVC_MAX_CONTENT_LEN - 2, &mut section_builder).unwrap_err()
    );
}

#[test]
fn building_capacity_0_ble5_section_works() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);

    let mut section_builder = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();

    // leave room for NP version header, section length and header
    fill_section_builder(BLE_5_ADV_SVC_MAX_CONTENT_LEN - 3, &mut section_builder).unwrap();

    // this section can fill everything except the NP version header
    assert_eq!(BLE_5_ADV_SVC_MAX_CONTENT_LEN - 1, section_builder.section.capacity);
    assert_eq!(BLE_5_ADV_SVC_MAX_CONTENT_LEN - 1, section_builder.section.len());

    section_builder.add_to_advertisement::<CryptoProviderImpl>();

    assert_eq!(BLE_5_ADV_SVC_MAX_CONTENT_LEN, adv_builder.into_advertisement().as_slice().len());
}

// TODO tests for other encoding types interacting with maximum possible section len
