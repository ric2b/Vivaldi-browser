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

//! Fuzz test for actions data element encoding logic

#![cfg_attr(fuzzing, no_main)]

use np_adv::extended::data_elements::ActionsDataElement;
use np_adv::ArrayVec;
use np_adv_fuzz::FuzzInput;

#[derive_fuzztest::fuzztest]
fn deserialize_actions_de(input: FuzzInput) {
    let mut actions = ArrayVec::new();
    actions.extend_from_slice(&input.data[..input.count]);
    let actions_de = ActionsDataElement::try_from_actions(actions);
}
