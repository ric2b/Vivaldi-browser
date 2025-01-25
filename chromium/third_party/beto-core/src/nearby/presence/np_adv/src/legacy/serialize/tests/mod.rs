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

#![allow(clippy::unwrap_used)]

extern crate std;

use crate::legacy::{
    data_elements::de_type::DataElementType, data_elements::*, serialize::*, PacketFlavorEnum,
};

mod error_conditions;
mod happy_path;
pub(in crate::legacy) mod helpers;

pub(crate) fn serialize<F: PacketFlavor, D: SerializeDataElement<F>>(
    de: &D,
) -> SerializedDataElement {
    serialize_de(de, NP_MAX_ADV_CONTENT_LEN).unwrap()
}

pub(in crate::legacy) fn supports_flavor(t: DataElementType, flavor: PacketFlavorEnum) -> bool {
    match t {
        DataElementType::Actions => match flavor {
            PacketFlavorEnum::Plaintext => true,
            PacketFlavorEnum::Ciphertext => true,
        },
        DataElementType::TxPower => match flavor {
            PacketFlavorEnum::Plaintext => true,
            PacketFlavorEnum::Ciphertext => true,
        },
    }
}

mod coverage_gaming {
    use crate::credential::v0::V0BroadcastCredential;
    use crate::legacy::serialize::{
        AddDataElementError, AdvBuilder, LdtEncodeError, LdtEncoder, UnencryptedEncodeError,
        UnencryptedEncoder,
    };
    use alloc::format;
    use crypto_provider_default::CryptoProviderImpl;
    use ldt_np_adv::{V0IdentityToken, V0Salt};

    #[test]
    fn unencrypted_encoder() {
        let _ = format!("{:?}", UnencryptedEncoder);
    }

    #[test]
    fn unencrypted_encoder_error() {
        let _ = format!("{:?}", UnencryptedEncodeError::InvalidLength);
    }

    #[test]
    fn ldt_encoder_display() {
        let identity_token = V0IdentityToken::from([0x33; 14]);
        let salt = V0Salt::from([0x01, 0x02]);
        let key_seed = [0xFE; 32];
        let broadcast_cred = V0BroadcastCredential::new(key_seed, identity_token);
        let ldt_encoder = LdtEncoder::<CryptoProviderImpl>::new(salt, &broadcast_cred);

        // doesn't leak crypto material
        assert_eq!("LdtEncoder { salt: V0Salt { bytes: [1, 2] } }", format!("{:?}", ldt_encoder));
    }

    #[test]
    fn ldt_encoder_error() {
        // debug, clone
        let _ = format!("{:?}", LdtEncodeError::InvalidLength.clone());
    }

    #[test]
    fn add_data_element_error() {
        // debug
        let _ = format!("{:?}", AddDataElementError::InsufficientAdvSpace);
    }

    #[test]
    fn adv_builder() {
        // debug
        let _ = format!("{:?}", AdvBuilder::new(UnencryptedEncoder));
    }
}
