// Copyright 2024 Google LLC
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

use alloc::collections::VecDeque;
use alloc::vec;
use alloc::vec::Vec;

extern crate std;

use crate::credential::book::{CachedSliceCredentialBook, CredentialBook, CredentialBookBuilder};
use crate::credential::matched::EmptyMatchedCredential;
use crate::deserialization_arena::DeserializationArena;
use crate::extended::data_elements::actions::{
    ActionsContainer, ActionsDataElement, ActionsDataElementError, ActionsDeserializationError,
    BitVectorOffsetContainer, ContainerEncoder, ContainerType, DeltaEncodedContainer,
    DeltaEncodedOffsetContainer, DeserializedActionsDE, MAX_ACTIONS_CONTAINER_LENGTH,
};
use crate::extended::data_elements::ActionId;
use crate::extended::de_type::DeType;
use crate::extended::deserialize::data_element::DataElement;
use crate::extended::deserialize::{Section, V1AdvertisementContents, V1DeserializedSection};
use crate::extended::serialize::{
    AdvBuilder, AdvertisementType, EncodedAdvertisement, SingleTypeDataElement,
    UnencryptedSectionEncoder,
};
use crate::{deserialization_arena, deserialize_advertisement};
use crypto_provider_default::CryptoProviderImpl;
use nom::error::ErrorKind;
use nom::Err::Error;
use np_hkdf::v1_salt::DataElementOffset;
use rand::distributions::uniform::SampleRange;
use rand::distributions::Standard;
use rand::prelude::Distribution;
use rand::rngs::StdRng;
use rand::{Rng, SeedableRng};
use tinyvec::{array_vec, ArrayVec};

#[test]
fn actions_de_round_trip() {
    let mut actions = ArrayVec::<[ActionId; 64]>::new();
    actions.extend_from_slice([100u16, 500, 300, 600].map(|v| v.try_into().unwrap()).as_slice());
    let actions_de = ActionsDataElement::try_from_actions(actions).expect("should succeed");
    let adv = create_adv_with_de(actions_de);
    let arena = deserialization_arena!();
    let cred_book = create_empty_cred_book();
    let contents = deser_into_v1_contents(arena, adv.as_slice(), &cred_book);
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
    assert_eq!(ActionsDataElement::DE_TYPE, de.de_type());
    let actions_de =
        DeserializedActionsDE::try_from(de).expect("Should succeed since this de is an actions de");
    assert_eq!(
        actions_de
            .collect_action_ids()
            .iter()
            .map(|res| res.expect("valid action ids").as_u16())
            .collect::<Vec<_>>(),
        vec![100, 300, 500, 600]
    )
}

#[test]
fn randomized_containers_roundtrip_tests() {
    let mut rng = StdRng::from_entropy();
    for _ in 0..10_000 {
        let mut actions_de = ActionsDataElement::new_empty();
        let mut expected = VecDeque::new();
        for _num_containers in 0..rng.gen_range(1..MAX_ACTIONS_CONTAINER_LENGTH) {
            let (container, actions) = gen_random_container(&mut rng);
            if actions_de.try_add_container(container).is_none() {
                expected.push_back(actions);
            }
        }
        let adv = create_adv_with_de(actions_de);
        let arena = deserialization_arena!();
        let cred_book = create_empty_cred_book();
        let contents = deser_into_v1_contents(arena, adv.as_slice(), &cred_book);
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
        assert_eq!(ActionsDataElement::DE_TYPE, de.de_type());
        let actions_de = DeserializedActionsDE::try_from(de)
            .expect("Should succeed since this de is an actions de");
        assert_eq!(actions_de.containers.len(), expected.len());
        let mut expected_action_ids =
            expected.iter().flatten().map(|action| action.as_u16()).collect::<Vec<_>>();
        expected_action_ids.sort();
        expected_action_ids.dedup();
        let mut decoded_action_ids =
            actions_de.collect_action_ids().iter().map(|x| x.unwrap().as_u16()).collect::<Vec<_>>();
        decoded_action_ids.sort();
        decoded_action_ids.dedup();
        assert_eq!(expected_action_ids, decoded_action_ids);
    }
}

#[test]
fn roundtrip_edge_case_actions_ids_in_last_bit_of_byte() {
    let mut actions =
        [1260u16, 1268, 1269, 1273, 1285, 1288, 1302, 1303, 1306, 1308, 1310, 1312, 1320, 1330]
            .map(|v| ActionId::try_from(v).unwrap());
    let container = BitVectorOffsetContainer::try_from_actions(actions.as_mut_slice()).unwrap();
    let mut de = ActionsDataElement::new_empty();
    assert!(de.try_add_container(container.into()).is_none());
    let adv = create_adv_with_de(de);
    let arena = deserialization_arena!();
    let cred_book = create_empty_cred_book();
    let contents = deser_into_v1_contents(arena, adv.as_slice(), &cred_book);
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
    assert_eq!(ActionsDataElement::DE_TYPE, de.de_type());
    let actions_de =
        DeserializedActionsDE::try_from(de).expect("Should succeed since this de is an actions de");
    let decoded_actions =
        actions_de.collect_action_ids().iter().map(|c| c.expect("")).collect::<Vec<ActionId>>();
    assert_eq!(actions.as_slice(), decoded_actions.as_slice())
}

fn create_adv_with_de(actions_de: ActionsDataElement) -> EncodedAdvertisement {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let mut section_builder = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();
    section_builder.add_de(|_salt| actions_de).unwrap();
    section_builder.add_to_advertisement::<CryptoProviderImpl>();
    adv_builder.into_advertisement()
}

fn create_empty_cred_book<'a>() -> CachedSliceCredentialBook<'a, EmptyMatchedCredential, 0, 0> {
    CredentialBookBuilder::<EmptyMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&[], &[])
}

pub fn deser_into_v1_contents<'adv, 'cred, B>(
    arena: DeserializationArena<'adv>,
    adv: &'adv [u8],
    cred_book: &'cred B,
) -> V1AdvertisementContents<'adv, B::Matched>
where
    B: CredentialBook<'cred>,
{
    deserialize_advertisement::<_, CryptoProviderImpl>(arena, adv, cred_book)
        .expect("Should be a valid advertisement")
        .into_v1()
        .expect("Should be V1")
}

impl Distribution<ContainerType> for Standard {
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> ContainerType {
        match rng.gen_range(0..=2) {
            0 => ContainerType::DeltaEncoded,
            1 => ContainerType::DeltaEncodedWithOffset,
            _ => ContainerType::BitVectorOffset,
        }
    }
}

fn gen_actions_in_rng<const N: usize, R: SampleRange<u16> + Clone>(
    rng: &mut StdRng,
    gen_range: R,
) -> ArrayVec<[ActionId; N]> {
    let mut actions = ArrayVec::new();
    for _ in 0..rng.gen_range(1..N) {
        let action_id: u16 = rng.gen_range(gen_range.clone());
        actions.push(ActionId::try_from(action_id).expect("range is a valid action id"))
    }
    actions
}

fn gen_delta_actions<const N: usize, R: SampleRange<u16> + Clone>(
    rng: &mut StdRng,
    gen_range: R,
) -> ArrayVec<[ActionId; N]> {
    let mut actions = ArrayVec::new();
    let mut previous = rng.gen_range(gen_range.clone());
    actions.push(ActionId::try_from(previous).unwrap());
    for _ in 0..rng.gen_range(1..N - 1) {
        // make sure generated actions are within a valid delta range of other actions
        let lower_bound = previous.saturating_sub(256);
        let upper_bound = if previous + 256 > 2047 { 2047 } else { previous + 256 };
        let action_id = rng.gen_range(lower_bound..=upper_bound);
        actions.push(ActionId::try_from(action_id).unwrap());
        previous = action_id
    }
    actions
}

fn gen_random_container(rng: &mut StdRng) -> (ActionsContainer, Vec<ActionId>) {
    let container_type: ContainerType = rng.gen();
    match container_type {
        ContainerType::DeltaEncoded => {
            let actions = gen_delta_actions(rng, 0..=255);
            (DeltaEncodedContainer::try_from_actions(actions).unwrap().into(), actions.to_vec())
        }
        ContainerType::DeltaEncodedWithOffset => {
            let actions = gen_delta_actions(rng, 0..=2047);
            (
                DeltaEncodedOffsetContainer::try_from_actions(actions).unwrap().into(),
                actions.to_vec(),
            )
        }
        ContainerType::BitVectorOffset => {
            let range_lower_bound = rng.gen_range(0u16..2047 - 495);
            let range_upper_bound = range_lower_bound + 495;
            let mut actions =
                gen_actions_in_rng::<512, _>(rng, range_lower_bound..=range_upper_bound);
            (
                BitVectorOffsetContainer::try_from_actions(actions.as_mut_slice()).unwrap().into(),
                actions.to_vec(),
            )
        }
    }
}

#[test]
fn parse_single_container_delta_encoded() {
    let bytes = [0x01, 0x64, 0xC7];
    let actions_de =
        DeserializedActionsDE::deserialize(&bytes).expect("bytes parse into valid actions DE");
    let action_containers = &actions_de.containers;
    assert_eq!(action_containers.len(), 1);
    let container = action_containers.first().unwrap();
    assert_eq!(container.container_type, ContainerType::DeltaEncoded);
    assert_eq!(container.payload, &[0x64, 0xC7]);

    let action_ids: Vec<_> = container
        .iter_action_ids()
        .map(|id| id.expect("test data contains action ids within valid range").as_u16())
        .collect();
    assert_eq!(action_ids, vec![100u16, 300]);
    assert_eq!(
        actions_de.collect_action_ids(),
        vec![Ok(100u16.try_into().unwrap()), Ok(300.try_into().unwrap())]
    );
}

#[test]
fn encode_single_container_delta_encoded() {
    let actions = array_vec!([ActionId; 64] => ActionId::try_from(100).unwrap(), ActionId::try_from(300).unwrap());
    let container = DeltaEncodedContainer::try_from_actions(actions).expect("Should succeed");
    assert_eq!(container.encoded_payload().as_slice(), &[0x64, 0xC7])
}

#[test]
fn encode_single_container_delta_encoded_delta_1() {
    let actions = array_vec!([ActionId; 64] => ActionId::try_from(100).unwrap(), ActionId::try_from(101).unwrap());
    let container = DeltaEncodedContainer::try_from_actions(actions).expect("Should succeed");
    assert_eq!(container.encoded_payload().as_slice(), &[0x64, 0x00])
}

#[test]
fn encode_single_container_delta_encoded_max_delta() {
    let actions = array_vec!([ActionId; 64] => ActionId::try_from(100).unwrap(), ActionId::try_from(356).unwrap());
    let container = DeltaEncodedContainer::try_from_actions(actions).expect("Should succeed");
    assert_eq!(container.encoded_payload().as_slice(), &[0x64, 0xFF])
}

#[test]
fn parse_single_container_delta_encoded_with_offset() {
    let bytes = [0x42, 0x06, 0x02, 0x66];
    let actions_de =
        DeserializedActionsDE::deserialize(&bytes).expect("provided bytes are valid actions de");
    assert_eq!(actions_de.containers.len(), 1);
    let container = actions_de.containers.first().unwrap();
    assert_eq!(container.container_type, ContainerType::DeltaEncodedWithOffset);
    assert_eq!(container.offset, 48);
    let actions: Vec<u16> = container
        .iter_action_ids()
        .map(|action| action.expect("Encoded id is in valid range").as_u16())
        .collect();
    assert_eq!(actions, vec![50, 153]);
    assert_eq!(
        actions_de.collect_action_ids(),
        vec![Ok(50.try_into().unwrap()), Ok(153.try_into().unwrap())]
    );
}

#[test]
fn encode_single_container_delta_encoded_with_offset() {
    let actions = array_vec!([ActionId; 63] => ActionId::try_from(50).unwrap(), ActionId::try_from(153).unwrap());
    let container = DeltaEncodedOffsetContainer::try_from_actions(actions).expect("Should succeed");
    assert_eq!(container.encoded_payload().as_slice(), &[0x06, 0x02, 0x66])
}

#[test]
fn encode_single_container_delta_encoded_with_offset_delta_of_1() {
    let actions = array_vec!([ActionId; 63] => ActionId::try_from(50).unwrap(), ActionId::try_from(51).unwrap());
    let container = DeltaEncodedOffsetContainer::try_from_actions(actions).expect("Should succeed");
    assert_eq!(container.encoded_payload().as_slice(), &[0x06, 0x02, 0x00])
}

#[test]
fn encode_single_container_delta_encoded_with_offset_max_delta() {
    let actions = array_vec!([ActionId; 63] => ActionId::try_from(50).unwrap(), ActionId::try_from(306).unwrap());
    let container = DeltaEncodedOffsetContainer::try_from_actions(actions).expect("Should succeed");
    assert_eq!(container.encoded_payload().as_slice(), &[0x06, 0x02, 0xFF])
}

#[test]
fn encode_single_container_delta_encoded_with_offset_delta_of_0() {
    let actions = array_vec!([ActionId; 63] => ActionId::try_from(48).unwrap(), ActionId::try_from(304).unwrap());
    let container = DeltaEncodedOffsetContainer::try_from_actions(actions).expect("Should succeed");
    assert_eq!(container.encoded_payload().as_slice(), &[0x06, 0x00, 0xFF])
}

#[test]
fn parse_single_container_bit_vector_offset() {
    let bytes = [0x82, 0x01, 0x5C, 0x08];
    let actions_de =
        DeserializedActionsDE::deserialize(&bytes).expect("provided bytes are valid actions de");
    assert_eq!(actions_de.containers.len(), 1);
    let container = actions_de.containers.first().unwrap();
    assert_eq!(container.container_type, ContainerType::BitVectorOffset);
    assert_eq!(container.offset, 8);
    let actions: Vec<u16> = container
        .iter_action_ids()
        .map(|action| action.expect("Encoded id is in valid range").as_u16())
        .collect();
    assert_eq!(actions, vec![9, 11, 12, 13, 20]);
}

#[test]
fn encode_single_container_bit_vector_offset() {
    let mut actions = [9, 11, 12, 13, 20].map(|x| ActionId::try_from(x).unwrap());
    let container =
        BitVectorOffsetContainer::try_from_actions(actions.as_mut_slice()).expect("Should succeed");
    assert_eq!(container.encoded_payload().as_slice(), &[0x01, 0x5C, 0x08])
}

#[test]
fn encode_single_container_bit_vector_offset_offset_of_0() {
    let mut actions = [0, 3, 5, 7].map(|x| ActionId::try_from(x).unwrap());
    let container =
        BitVectorOffsetContainer::try_from_actions(actions.as_mut_slice()).expect("Should succeed");
    assert_eq!(container.encoded_payload().as_slice(), &[0x00, 0b1001_0101])
}

#[test]
fn encode_single_container_bit_vector_offset_max_offset() {
    let mut actions = [2046, 2047].map(|x| ActionId::try_from(x).unwrap());
    let container =
        BitVectorOffsetContainer::try_from_actions(actions.as_mut_slice()).expect("Should succeed");
    assert_eq!(container.encoded_payload().as_slice(), &[0xFF, 0b0000_0011])
}

#[test]
fn encode_single_container_bit_vector_max_range() {
    // A container is at most 64 bytes. One byte is always used for the offset so that leaves 63 total bytes of
    // bit vector encoded action ids where each bit represents an action. Since the first byte represent 0-7,
    // byte 63 represents actions 496-503, making 503 the maximum possible action id which can be encoded
    // in this container
    let mut actions = [0, 503].map(|x| ActionId::try_from(x).unwrap());
    let container =
        BitVectorOffsetContainer::try_from_actions(actions.as_mut_slice()).expect("Should succeed");
    let mut expected = [0; 64];
    expected[1] = 0b1000_0000;
    expected[63] = 0b0000_0001;
    assert_eq!(container.encoded_payload().as_slice(), &expected);
}

#[test]
fn encode_single_container_bit_vector_invalid_range() {
    let mut actions = [1, 504].map(|x| ActionId::try_from(x).unwrap());
    let err = BitVectorOffsetContainer::try_from_actions(actions.as_mut_slice()).unwrap_err();
    assert_eq!(err, ActionsDataElementError::ActionIdOutOfRange)
}

#[test]
fn encode_single_container_bit_vector_max_range_with_offset() {
    // now 559 should still be in range since this is offset by 7 bytes
    let mut actions = [56, 559].map(|x| ActionId::try_from(x).unwrap());
    let container =
        BitVectorOffsetContainer::try_from_actions(actions.as_mut_slice()).expect("Should succeed");
    let mut expected = [0; 64];
    expected[0] = 7;
    expected[1] = 0b1000_0000;
    expected[63] = 0b0000_0001;
    assert_eq!(container.encoded_payload().as_slice(), &expected);
}

#[test]
fn encode_single_container_bit_vector_out_of_range_with_offset() {
    let mut actions = [56, 560].map(|x| ActionId::try_from(x).unwrap());
    let err = BitVectorOffsetContainer::try_from_actions(actions.as_mut_slice()).unwrap_err();
    assert_eq!(err, ActionsDataElementError::ActionIdOutOfRange);
}

#[test]
fn encode_bit_vector_max_amount_of_ids() {
    let mut actions_ids = [0xFF; 512].map(|x| ActionId::try_from(x).unwrap());
    assert!(BitVectorOffsetContainer::try_from_actions(actions_ids.as_mut_slice()).is_ok())
}

#[test]
fn encode_bit_vector_too_many_actionids() {
    let mut actions_ids = [0xFF; 513].map(|x| ActionId::try_from(x).unwrap());
    assert_eq!(
        BitVectorOffsetContainer::try_from_actions(actions_ids.as_mut_slice()).unwrap_err(),
        ActionsDataElementError::TooManyActions
    );
}

#[test]
fn parse_multiple_containers() {
    let bytes = [
        vec![0x01, 0x64, 0xC7],       // delta encoded container
        vec![0x42, 0x06, 0x02, 0x66], // delta encoded with offset
        vec![0x82, 0x01, 0x5C, 0x08], // bit vector offset
    ]
    .concat();
    let de = DeserializedActionsDE::deserialize(&bytes).expect("bytes are valid de");
    assert_eq!(de.containers.len(), 3);
    assert_eq!(de.containers[0].container_type, ContainerType::DeltaEncoded);
    assert_eq!(de.containers[1].container_type, ContainerType::DeltaEncodedWithOffset);
    assert_eq!(de.containers[2].container_type, ContainerType::BitVectorOffset);

    let action_ids = de.collect_action_ids();
    assert_eq!(
        action_ids
            .iter()
            .map(|a| { a.expect("action_ids encoded are in valid range").as_u16() })
            .collect::<Vec<_>>(),
        vec![100, 300, 50, 153, 9, 11, 12, 13, 20]
    );
}

#[test]
fn try_from_invalid_type_code() {
    let data = [];
    let de = DataElement::new(DataElementOffset::from(0), DeType::const_from(5), &data);
    assert_eq!(
        DeserializedActionsDE::try_from(&de).unwrap_err(),
        ActionsDeserializationError::InvalidTypeCode
    );
}

#[test]
fn try_from_invalid_container_length_empty_after_subheader() {
    let data = [0x40, 0x64];
    let de = DataElement::new(DataElementOffset::from(0), DeType::const_from(6), &data);
    assert_eq!(
        DeserializedActionsDE::try_from(&de).unwrap_err(),
        ActionsDeserializationError::InvalidContainerLength
    );
}

#[test]
fn try_from_invalid_container_length_not_enough_data() {
    let data = [0x02, 0x64, 0xC8];
    let de = DataElement::new(DataElementOffset::from(0), DeType::const_from(6), &data);
    assert_eq!(
        DeserializedActionsDE::try_from(&de).unwrap_err(),
        ActionsDeserializationError::InvalidContainerLength
    );
}

#[test]
fn try_from_invalid_container_type() {
    let data = [0xC0, 0x64];
    let de = DataElement::new(DataElementOffset::from(0), DeType::const_from(6), &data);
    assert_eq!(
        DeserializedActionsDE::try_from(&de).unwrap_err(),
        ActionsDeserializationError::InvalidContainerType
    );
}

#[test]
fn parse_single_byte_delta_encoding() {
    let bytes = [0x00, 0x64];
    let actions_de = DeserializedActionsDE::deserialize(&bytes).unwrap();
    let action_containers = &actions_de.containers;
    assert_eq!(action_containers.len(), 1);
    let container = action_containers.first().unwrap();
    assert_eq!(container.container_type, ContainerType::DeltaEncoded);
    assert_eq!(container.payload, &[0x64])
}

#[test]
fn parse_single_byte_delta_encoding_offset_should_fail() {
    let bytes = [0x40, 0x64];
    let err = DeserializedActionsDE::deserialize(&bytes).unwrap_err();
    assert_eq!(err, Error(nom::error::Error { input: &[100][..], code: ErrorKind::Verify }));
}

#[test]
fn parse_single_container_max_length() {
    let mut bytes = vec![0x3F];
    bytes.extend_from_slice(&[0x01; 64]);

    let actions_de =
        DeserializedActionsDE::deserialize(&bytes).expect("bytes parse into valid actions DE");
    let action_containers = &actions_de.containers;
    assert_eq!(action_containers.len(), 1);
    let container = action_containers.first().unwrap();
    assert_eq!(container.container_type, ContainerType::DeltaEncoded);
    assert_eq!(container.payload, &[0x01; 64]);
}

#[test]
fn parse_single_byte_bit_vector_offset_should_fail() {
    let bytes = [0x80, 0x64];
    let err = DeserializedActionsDE::deserialize(&bytes).unwrap_err();
    assert_eq!(err, Error(nom::error::Error { input: &[100][..], code: ErrorKind::Verify }));
}

#[test]
fn parse_actions_de_invalid_length_bytes() {
    let bytes = [0x02, 0x64, 0xC8];
    let err = DeserializedActionsDE::deserialize(&bytes).unwrap_err();
    assert_eq!(err, Error(nom::error::Error { input: &[100u8, 200][..], code: ErrorKind::Eof }))
}

#[test]
fn parse_actions_de_extra_bytes() {
    let bytes = [0x01, 0x64, 0xC8, 0xFF];
    let err = DeserializedActionsDE::deserialize(&bytes).unwrap_err();
    assert_eq!(err, Error(nom::error::Error { input: &[255u8][..], code: ErrorKind::Eof }))
}

#[test]
fn parse_actions_de_invalid_container_type() {
    let bytes = [0xC2, 0x64, 0xC8];
    let err = DeserializedActionsDE::deserialize(&bytes).unwrap_err();
    assert_eq!(
        err,
        Error(nom::error::Error { input: &[0xC2, 0x64, 0xC8][..], code: ErrorKind::MapOpt })
    )
}

#[test]
fn encode_empty_actions() {
    let actions = ArrayVec::new();
    let err = ActionsDataElement::try_from_actions(actions).unwrap_err();
    assert_eq!(err, ActionsDataElementError::EmptyActions);
}

#[test]
fn encode_empty_delta_container() {
    let actions = ArrayVec::new();
    let err = DeltaEncodedContainer::try_from_actions(actions).unwrap_err();
    assert_eq!(err, ActionsDataElementError::EmptyActions);
}

#[test]
fn encode_empty_delta_offset_container() {
    let actions = ArrayVec::new();
    let err = DeltaEncodedOffsetContainer::try_from_actions(actions).unwrap_err();
    assert_eq!(err, ActionsDataElementError::EmptyActions);
}

#[test]
fn encode_empty_bit_vector_offset_container() {
    let err = BitVectorOffsetContainer::try_from_actions(&mut []).unwrap_err();
    assert_eq!(err, ActionsDataElementError::EmptyActions);
}

#[test]
fn encode_basic_delta_encoding() {
    let mut actions = ArrayVec::<[ActionId; 64]>::new();
    actions.extend_from_slice([100u16, 500, 300, 600].map(|v| v.try_into().unwrap()).as_slice());
    let result = ActionsDataElement::try_from_actions(actions).expect("should succeed");
    assert_eq!(result.containers[0].container_type, ContainerType::DeltaEncoded);
    assert_eq!(result.containers[0].payload.as_slice(), &[100, 199, 199, 99]);
}

#[test]
fn encode_basic_delta_encoding_id_out_of_range() {
    let mut actions = ArrayVec::<[ActionId; 64]>::new();
    actions.extend_from_slice([256].map(|v| v.try_into().unwrap()).as_slice());
    let err = ActionsDataElement::try_from_actions(actions).unwrap_err();
    assert_eq!(err, ActionsDataElementError::ActionIdDeltaOverflow);
}

#[test]
fn encode_basic_delta_encoding_max_id() {
    let mut actions = ArrayVec::<[ActionId; 64]>::new();
    actions.extend_from_slice([255].map(|v| v.try_into().unwrap()).as_slice());
    let de = ActionsDataElement::try_from_actions(actions).expect("should succeed");
    assert_eq!(de.containers[0].container_type, ContainerType::DeltaEncoded);
    assert_eq!(de.containers[0].payload.as_slice(), &[255]);
}

#[test]
fn encode_basic_delta_encoding_duplicate_ids() {
    let mut actions = ArrayVec::<[ActionId; 64]>::new();
    actions
        .extend_from_slice([1, 1, 1, 6, 3, 3, 6, 3, 3].map(|v| v.try_into().unwrap()).as_slice());
    let de = ActionsDataElement::try_from_actions(actions).expect("should succeed");
    assert_eq!(de.containers[0].container_type, ContainerType::DeltaEncoded);
    assert_eq!(de.containers[0].payload.as_slice(), &[1, 1, 2]);
}

#[test]
fn encode_basic_delta_encoding_delta_out_of_range() {
    let mut actions = ArrayVec::<[ActionId; 64]>::new();
    actions.extend_from_slice([100, 400].map(|v| v.try_into().unwrap()).as_slice());
    let err = ActionsDataElement::try_from_actions(actions).unwrap_err();
    assert_eq!(err, ActionsDataElementError::ActionIdDeltaOverflow);
}
