// Copyright 2023 Google LLC
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

extern crate std;

use std::prelude::rust_2021::*;

use crate::{
    extended::deserialize::{CiphertextSection, PlaintextSection},
    IntermediateSection,
};

pub(crate) trait IntermediateSectionExt<'adv> {
    /// Returns `Some` if `self` is `Plaintext`
    fn as_plaintext(&self) -> Option<&PlaintextSection<'adv>>;
    /// Returns `Some` if `self` is `Ciphertext`
    fn as_ciphertext(&self) -> Option<&CiphertextSection<'adv>>;
}

impl<'adv> IntermediateSectionExt<'adv> for IntermediateSection<'adv> {
    fn as_plaintext(&self) -> Option<&PlaintextSection<'adv>> {
        match self {
            IntermediateSection::Plaintext(s) => Some(s),
            IntermediateSection::Ciphertext(_) => None,
        }
    }

    fn as_ciphertext(&self) -> Option<&CiphertextSection<'adv>> {
        match self {
            IntermediateSection::Plaintext(_) => None,
            IntermediateSection::Ciphertext(s) => Some(s),
        }
    }
}
