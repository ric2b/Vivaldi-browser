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

//! Fuzz test for actions data element parsing logic

#![cfg_attr(fuzzing, no_main)]

use arbitrary::Unstructured;
use np_adv::extended::data_elements::DeserializedActionsDE;
use np_adv::extended::deserialize::data_element::DataElement;

#[derive(arbitrary::Arbitrary, Clone, Debug)]
struct ActionsDeserFuzzInput {
    data: [u8; 127],
    #[arbitrary(with = arbitrary_de_len)]
    de_len: usize,
}

fn arbitrary_de_len(u: &mut Unstructured) -> arbitrary::Result<usize> {
    u.int_in_range(0..=127).map(|val| usize::try_from(val).expect("range is valid for a usize"))
}

#[derive_fuzztest::fuzztest]
fn deserialize_actions_de(input: ActionsDeserFuzzInput) {
    let de = DataElement::new_for_testing(0.into(), 6_u32.into(), &input.data[..input.de_len]);
    let actions_de = DeserializedActionsDE::try_from(&de).map(|actions| {
        // collect actions to trigger iterator parsing logic
        let action_ids = actions.collect_action_ids();
    });
}
