// Copyright 2024 Google LLC
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

//! Fuzz test for actions data element encoding and decoding round trip

#![cfg_attr(fuzzing, no_main)]

use crypto_provider_default::CryptoProviderImpl;
use np_adv::credential::book::CredentialBookBuilder;
use np_adv::credential::matched::EmptyMatchedCredential;
use np_adv::extended::data_elements::{ActionsDataElement, DeserializedActionsDE};
use np_adv::extended::deserialize::{Section, V1DeserializedSection};
use np_adv::extended::serialize::{
    AdvBuilder, AdvertisementType, SingleTypeDataElement, UnencryptedSectionEncoder,
};
use np_adv::{deserialization_arena, deserialize_advertisement, ArrayVec};
use np_adv_fuzz::FuzzInput;

#[derive_fuzztest::fuzztest]
fn deserialize_actions_de(input: FuzzInput) {
    let mut actions = ArrayVec::new();
    actions.extend_from_slice(&input.data[..input.count]);
    let actions_de = match ActionsDataElement::try_from_actions(actions) {
        Ok(de) => de,
        Err(_) => return,
    };
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let mut section_builder = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();
    section_builder.add_de(|_salt| actions_de).unwrap();
    section_builder.add_to_advertisement::<CryptoProviderImpl>();
    let arena = deserialization_arena!();
    let adv = adv_builder.into_advertisement();
    let cred_book = CredentialBookBuilder::<EmptyMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&[], &[]);
    let contents =
        deserialize_advertisement::<_, CryptoProviderImpl>(arena, adv.as_slice(), &cred_book)
            .expect("Should be a valid advertisement")
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
    assert_eq!(ActionsDataElement::DE_TYPE, de.de_type());
    let actions_de =
        DeserializedActionsDE::try_from(de).expect("Should succeed since this de is an actions de");
    let decoded_actions = actions_de
        .collect_action_ids()
        .iter()
        .map(|res| res.expect("valid action ids"))
        .collect::<Vec<_>>();
    let mut expected = actions.to_vec();
    expected.sort();
    expected.dedup();
    assert_eq!(expected.as_slice(), decoded_actions.as_slice());
}
