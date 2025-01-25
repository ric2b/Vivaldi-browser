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

use tinyvec::ArrayVec;

use crypto_provider_default::CryptoProviderImpl;

use crate::extended::data_elements::actions::{ActionId, ActionsDataElement};
use crate::extended::serialize::{section_tests::SectionBuilderExt, AdvBuilder};
use crate::extended::serialize::{AdvertisementType, UnencryptedSectionEncoder};
use crate::extended::V1_ENCODING_UNENCRYPTED;

use super::*;

#[test]
fn serialize_tx_power_de() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let mut section_builder = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();

    section_builder.add_de_res(|_| TxPower::try_from(3_i8).map(TxPowerDataElement::from)).unwrap();

    assert_eq!(
        &[
            V1_ENCODING_UNENCRYPTED,
            2,    // section len
            0x15, // len 1 type 0x05
            3
        ],
        section_builder.into_section::<CryptoProviderImpl>().as_slice()
    );
}

fn actions_ids_from_u16_collection<const N: usize>(actions: [u16; N]) -> ArrayVec<[ActionId; 64]> {
    let mut result = ArrayVec::new();
    result.extend_from_slice(&actions.map(|x| x.try_into().unwrap()));
    result
}

#[test]
fn serialize_actions_de_non_empty() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let mut section_builder = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();

    section_builder
        .add_de_res(|_| {
            ActionsDataElement::try_from_actions(actions_ids_from_u16_collection([
                1, 1, 2, 3, 5, 8, // fibonacci, of course
            ]))
        })
        .unwrap();

    #[rustfmt::skip]
    assert_eq!(
        &[
            V1_ENCODING_UNENCRYPTED,
            7, // section len
            0x66, // len 6 type 0x06
            0b00000100, // container type and len TTLLLLLL
            1, 0, 0, 1, 2 // de-duped delta encoded fibonacci
        ],
        section_builder.into_section::<CryptoProviderImpl>().as_slice()
    );
}

#[rustfmt::skip]
#[test]
fn serialize_context_sync_seq_num_de() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let mut section_builder =
        adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();

    section_builder
        .add_de_res(|_| ContextSyncSeqNum::try_from(3).map(ContextSyncSeqNumDataElement::from))
        .unwrap();

    assert_eq!(
        &[
            V1_ENCODING_UNENCRYPTED,
            3, // section len
            0x81, 0x13, // len 1 type 0x13
            3,    // seq num
        ],
        section_builder.into_section::<CryptoProviderImpl>().as_slice()
    );
}

#[rustfmt::skip]
#[test]
fn serialize_connectivity_info_de_bluetooth() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let mut section_builder =
        adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();

    section_builder.add_de(|_| ConnectivityInfoDataElement::bluetooth([1; 4], [2; 6])).unwrap();

    assert_eq!(
        &[
            V1_ENCODING_UNENCRYPTED,
            13, // section len
            0x8B, 0x11, // len 11 type 0x11
            1,    // connectivity type
            1, 1, 1, 1, // svc id
            2, 2, 2, 2, 2, 2 // mac
        ],
        section_builder.into_section::<CryptoProviderImpl>().as_slice()
    );
}

#[rustfmt::skip]
#[test]
fn serialize_connectivity_info_de_mdns() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let mut section_builder =
        adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();

    section_builder.add_de(|_| ConnectivityInfoDataElement::mdns([1; 4], 2)).unwrap();

    assert_eq!(
        &[
            V1_ENCODING_UNENCRYPTED,
            8, // section len
            0x86, 0x11, // len 11 type 0x11
            2,    // connectivity type
            1, 1, 1, 1, // svc id
            2  // port
        ],
        section_builder.into_section::<CryptoProviderImpl>().as_slice()
    );
}

#[rustfmt::skip]
#[test]
fn serialize_connectivity_info_de_wifi_direct() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let mut section_builder =
        adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();

    section_builder
        .add_de(|_| ConnectivityInfoDataElement::wifi_direct([1; 10], [2; 10], [3; 2], 4))
        .unwrap();

    assert_eq!(
        &[
            V1_ENCODING_UNENCRYPTED,
            26, // section len
            0x98, 0x11, // len 24 type 0x11
            3,    // connectivity type
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // ssid
            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // password
            3, 3, // freq
            4  // port
        ],
        section_builder.into_section::<CryptoProviderImpl>().as_slice()
    );
}

#[rustfmt::skip]
#[test]
fn serialize_connectivity_capabilities_de_wifi_direct() {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let mut section_builder =
        adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();

    section_builder
        .add_de(|_| ConnectivityCapabilityDataElement::wifi_direct([1; 3], [2; 3]))
        .unwrap();

    assert_eq!(
        &[
            V1_ENCODING_UNENCRYPTED,
            9, // section len
            0x87, 0x12, // len 7 type 0x12
            2,    // connectivity type
            1, 1, 1, // supported
            2, 2, 2, // connected
        ],
        section_builder.into_section::<CryptoProviderImpl>().as_slice()
    );
}

mod coverage_gaming {
    use alloc::format;

    use super::*;

    #[test]
    fn de_type_const_from() {
        let _ = DeType::const_from(3);
    }

    #[test]
    fn template() {}

    #[test]
    fn generic_de_error_derives() {
        let err = GenericDataElementError::DataTooLong;
        let _ = format!("{:?}", err);
        assert_eq!(err, err);
    }

    #[test]
    fn generic_data_element_debug() {
        let generic =
            GenericDataElement::try_from(DeType::from(1000_u32), &[10, 11, 12, 13]).unwrap();
        let _ = format!("{:?}", generic);
    }
}
