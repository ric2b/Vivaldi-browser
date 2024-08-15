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

//! V0 advertisement support.

use crate::MetadataKey;
use core::fmt;
use crypto_provider::CryptoProvider;
use ldt_np_adv::NP_LEGACY_METADATA_KEY_LEN;

pub mod actions;
pub mod data_elements;
pub mod de_type;
pub mod deserialize;
pub mod serialize;

#[cfg(test)]
mod random_data_elements;

/// Advertisement capacity after 5 bytes of BLE header and 2 bytes of svc UUID are reserved from a
/// 31-byte advertisement
pub const BLE_ADV_SVC_CONTENT_LEN: usize = 24;
/// Maximum possible DE content: packet size minus 2 for adv header & DE header
const NP_MAX_DE_CONTENT_LEN: usize = BLE_ADV_SVC_CONTENT_LEN - 2;

/// "Short" 14-byte metadata key type employed for V0, which needs to be
/// expanded to a regular-size 16-byte metadata key to decrypt metadata.
#[derive(Debug, Clone, Copy, Hash, PartialEq, Eq)]
pub struct ShortMetadataKey(pub [u8; NP_LEGACY_METADATA_KEY_LEN]);

impl AsRef<[u8]> for ShortMetadataKey {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl ShortMetadataKey {
    /// Expand this short 14-byte metadata key to a 16-byte metadata key
    /// which may be used to decrypt metadata.
    pub fn expand<C: CryptoProvider>(&self) -> MetadataKey {
        let expanded_bytes = np_hkdf::legacy_metadata_expanded_key::<C>(&self.0);
        MetadataKey(expanded_bytes)
    }
}

/// Marker type to allow disambiguating between plaintext and encrypted packets at compile time.
///
/// See also [PacketFlavorEnum] for when runtime flavor checks are more suitable.
pub trait PacketFlavor: fmt::Debug + Clone + Copy {
    /// The corresponding [PacketFlavorEnum] variant.
    const ENUM_VARIANT: PacketFlavorEnum;
}

/// Marker type for plaintext packets (public identity and no identity).
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct Plaintext;

impl PacketFlavor for Plaintext {
    const ENUM_VARIANT: PacketFlavorEnum = PacketFlavorEnum::Plaintext;
}

/// Marker type for ciphertext packets (private, trusted, and provisioned identity).
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct Ciphertext;

impl PacketFlavor for Ciphertext {
    const ENUM_VARIANT: PacketFlavorEnum = PacketFlavorEnum::Ciphertext;
}

/// An enum version of the implementors of [PacketFlavor] for use cases where runtime checking is
/// a better fit than compile time checking.
#[derive(Debug, Clone, Copy, strum_macros::EnumIter, PartialEq, Eq)]
pub enum PacketFlavorEnum {
    /// Corresponds to [Plaintext].
    Plaintext,
    /// Corresponds to [Ciphertext].
    Ciphertext,
}
