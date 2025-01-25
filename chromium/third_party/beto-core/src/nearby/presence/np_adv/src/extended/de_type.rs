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

//! V1 DE type types

/// Data element types for extended advertisements
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct DeType {
    // 4 billion type codes should be enough for anybody
    code: u32,
}

impl DeType {
    /// A `const` equivalent to `From<u32>` since trait methods can't yet be const.
    pub const fn const_from(value: u32) -> Self {
        Self { code: value }
    }

    /// Returns the type as a u32
    pub fn as_u32(&self) -> u32 {
        self.code
    }
}

impl From<u8> for DeType {
    fn from(value: u8) -> Self {
        DeType { code: value.into() }
    }
}

impl From<u32> for DeType {
    fn from(value: u32) -> Self {
        DeType { code: value }
    }
}

impl From<DeType> for u32 {
    fn from(value: DeType) -> Self {
        value.code
    }
}

#[cfg(test)]
mod test {
    use crate::extended::de_type::DeType;

    #[test]
    fn u32_from_de_type() {
        let de = DeType::from(8u32);
        let _val: u32 = de.into();
    }
}
