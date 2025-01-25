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

use arbitrary::Unstructured;
use np_adv::extended::data_elements::ActionId;

#[derive(arbitrary::Arbitrary, Clone, Debug)]
pub struct FuzzInput {
    #[arbitrary(with = arbitrary_action_ids)]
    pub data: [ActionId; 64],
    #[arbitrary(with = arbitrary_actions_count)]
    pub count: usize,
}

fn arbitrary_actions_count(u: &mut Unstructured) -> arbitrary::Result<usize> {
    u.int_in_range(0..=64).map(|val| usize::try_from(val).unwrap())
}

fn arbitrary_action_ids(u: &mut Unstructured) -> arbitrary::Result<[ActionId; 64]> {
    Ok(std::array::from_fn(|_| {
        let next = u16::try_from(
            u.int_in_range(0..=2047).expect("fuzzer should generate enough data for a u16"),
        )
        .expect("the range will always be a valid u16");
        ActionId::try_from(next).expect("rang is valid for action_ids")
    }))
}
