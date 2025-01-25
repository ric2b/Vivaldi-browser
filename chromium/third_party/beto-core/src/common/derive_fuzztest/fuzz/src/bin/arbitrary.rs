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

#![cfg_attr(fuzzing, no_main)]

use arbitrary::{Arbitrary, Unstructured};
use derive_fuzztest::fuzztest;

#[derive(Debug, Clone)]
pub struct SmallU8(u8);

impl<'a> Arbitrary<'a> for SmallU8 {
    fn arbitrary(u: &mut Unstructured<'a>) -> arbitrary::Result<Self> {
        Ok(SmallU8(u.int_in_range(0..=127)?))
    }
}

#[fuzztest]
pub fn test(a: SmallU8, b: SmallU8) {
    let _ = a.0 + b.0; // Succeeds because our custom arbitrary impl only generates 0-127 so it never overflows
}
