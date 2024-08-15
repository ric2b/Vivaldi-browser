// Copyright 2023 Google LLC
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
//! Core NP Rust FFI structures and methods for advertisement serialization.

pub mod v0;
pub mod v1;

/// Enum common to V0 and V1 serialization expressing
/// what kind of advertisement builder (public/encrypted)
/// is in use.
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum AdvertisementBuilderKind {
    /// The builder is for a public advertisement.
    Public = 0,
    /// The builder is for an encrypted advertisement.
    Encrypted = 1,
}

impl AdvertisementBuilderKind {
    pub(crate) fn as_internal_v1(&self) -> np_adv::extended::serialize::AdvertisementType {
        use np_adv::extended::serialize::AdvertisementType;
        match self {
            Self::Public => AdvertisementType::Plaintext,
            Self::Encrypted => AdvertisementType::Encrypted,
        }
    }
}
