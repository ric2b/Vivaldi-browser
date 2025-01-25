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

use crate::credential::matched::{MatchedCredential, WithMatchedCredential};
use crate::legacy::deserialize::intermediate::{IntermediateAdvContents, UnencryptedAdvContents};
use crate::{
    credential::{
        book::CredentialBook, v0::V0DiscoveryCryptoMaterial, DiscoveryMetadataCryptoMaterial,
    },
    header::V0Encoding,
    legacy::deserialize::{DecryptError, DecryptedAdvContents},
    AdvDeserializationError,
};
use core::fmt;
use crypto_provider::CryptoProvider;

pub mod data_elements;
pub mod deserialize;
pub mod serialize;

#[cfg(test)]
mod random_data_elements;

/// Advertisement capacity after 5 bytes of BLE header and 2 bytes of svc UUID are reserved from a
/// 31-byte advertisement
pub const BLE_4_ADV_SVC_MAX_CONTENT_LEN: usize = 24;
/// Maximum possible advertisement NP-level content: packet size minus 1 for version header
const NP_MAX_ADV_CONTENT_LEN: usize = BLE_4_ADV_SVC_MAX_CONTENT_LEN - 1;
/// Minimum advertisement NP-level content.
/// Only meaningful for unencrypted advertisements, as LDT advertisements already have salt, token, etc.
const NP_MIN_ADV_CONTENT_LEN: usize = 1;
/// Max length of an individual DE's content
pub(crate) const NP_MAX_DE_CONTENT_LEN: usize = NP_MAX_ADV_CONTENT_LEN - 1;

/// Marker type to allow disambiguating between plaintext and encrypted packets at compile time.
///
/// See also [PacketFlavorEnum] for when runtime flavor checks are more suitable.
pub trait PacketFlavor: fmt::Debug + Clone + Copy + PartialEq + Eq {
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
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PacketFlavorEnum {
    /// Corresponds to [Plaintext].
    Plaintext,
    /// Corresponds to [Ciphertext].
    Ciphertext,
}

/// Deserialize and decrypt the contents of a v0 adv after the version header
pub(crate) fn deser_decrypt_v0<'adv, 'cred, B, P>(
    encoding: V0Encoding,
    cred_book: &'cred B,
    remaining: &'adv [u8],
) -> Result<V0AdvertisementContents<'adv, B::Matched>, AdvDeserializationError>
where
    B: CredentialBook<'cred>,
    P: CryptoProvider,
{
    match IntermediateAdvContents::deserialize::<P>(encoding, remaining)? {
        IntermediateAdvContents::Unencrypted(p) => Ok(V0AdvertisementContents::Plaintext(p)),
        IntermediateAdvContents::Ldt(c) => {
            for (crypto_material, matched) in cred_book.v0_iter() {
                let ldt = crypto_material.ldt_adv_cipher::<P>();
                match c.try_decrypt(&ldt) {
                    Ok(c) => {
                        let metadata_nonce = crypto_material.metadata_nonce::<P>();
                        return Ok(V0AdvertisementContents::Decrypted(WithMatchedCredential::new(
                            matched,
                            metadata_nonce,
                            c,
                        )));
                    }
                    Err(e) => match e {
                        DecryptError::DecryptOrVerifyError => continue,
                    },
                }
            }
            Ok(V0AdvertisementContents::NoMatchingCredentials)
        }
    }
}

/// Advertisement content that was either already plaintext or has been decrypted.
#[derive(Debug, PartialEq, Eq)]
pub enum V0AdvertisementContents<'adv, M: MatchedCredential> {
    /// Contents of a plaintext advertisement
    Plaintext(UnencryptedAdvContents<'adv>),
    /// Contents that was ciphertext in the original advertisement, and has been decrypted
    /// with the credential in the [MatchedCredential]
    Decrypted(WithMatchedCredential<M, DecryptedAdvContents>),
    /// The advertisement was encrypted, but no credentials matched
    NoMatchingCredentials,
}

#[cfg(test)]
mod tests {
    mod coverage_gaming {
        use crate::legacy::{Ciphertext, PacketFlavorEnum, Plaintext};

        extern crate std;

        use std::format;

        #[test]
        fn plaintext_flavor() {
            // debug
            let _ = format!("{:?}", Plaintext);
            // eq and clone
            assert_eq!(Plaintext, Plaintext.clone())
        }

        #[test]
        fn ciphertext_flavor() {
            // debug
            let _ = format!("{:?}", Ciphertext);
            // eq and clone
            assert_eq!(Ciphertext, Ciphertext.clone())
        }

        #[allow(clippy::clone_on_copy)]
        #[test]
        fn flavor_enum() {
            // clone
            let _ = PacketFlavorEnum::Plaintext.clone();
        }
    }
}
