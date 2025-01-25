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

mod unencrypted_encoder {
    use alloc::vec;
    use core::ops::Deref;

    extern crate std;

    use std::prelude::rust_2021::*;

    use crate::legacy::serialize::tests::helpers::{LongDataElement, ShortDataElement};
    use crate::legacy::serialize::{
        AddDataElementError, AdvBuilder, UnencryptedEncodeError, UnencryptedEncoder,
    };
    use crate::legacy::{BLE_4_ADV_SVC_MAX_CONTENT_LEN, NP_MAX_DE_CONTENT_LEN};

    #[test]
    fn build_empty_adv_error() {
        // empty isn't allowed, so 1 byte of payload is the smallest possible
        let builder = AdvBuilder::new(UnencryptedEncoder);

        assert_eq!(
            UnencryptedEncodeError::InvalidLength,
            builder.into_advertisement().unwrap_err()
        );
    }

    #[test]
    fn add_de_when_full_error() {
        let mut builder = AdvBuilder::new(UnencryptedEncoder);

        builder
            .add_data_element(LongDataElement::new(vec![1; NP_MAX_DE_CONTENT_LEN].clone()))
            .unwrap();

        // 1 more byte (DE header) is too much
        assert_eq!(
            AddDataElementError::InsufficientAdvSpace,
            builder.add_data_element(ShortDataElement::new(vec![].clone())).unwrap_err()
        );
    }

    #[test]
    fn add_too_much_de_when_almost_full_error() {
        let mut builder = AdvBuilder::new(UnencryptedEncoder);

        // leave 1 byte of room
        builder
            .add_data_element(LongDataElement::new(vec![1; NP_MAX_DE_CONTENT_LEN - 1].clone()))
            .unwrap();

        // DE header would fit, but 1 byte of DE content is too much
        assert_eq!(BLE_4_ADV_SVC_MAX_CONTENT_LEN - 1, builder.len);
        assert_eq!(
            AddDataElementError::InsufficientAdvSpace,
            builder.add_data_element(ShortDataElement::new(vec![2].clone())).unwrap_err()
        );
    }

    #[test]
    fn broken_de_impl_hits_expected_panic() {
        let mut builder = AdvBuilder::new(UnencryptedEncoder);

        let panic_payload = std::panic::catch_unwind(move || {
            // This DE type can't represent short lengths
            builder.add_data_element(LongDataElement::new(vec![]))
        })
        .unwrap_err();

        assert_eq!(
            "Couldn't encode actual len: DeLengthOutOfRange",
            panic_payload.downcast::<String>().unwrap().deref()
        );
    }
}

mod ldt_encoder {
    use alloc::string::String;
    use alloc::vec;
    use core::ops::Deref;

    extern crate std;

    use crypto_provider_default::CryptoProviderImpl;
    use ldt_np_adv::{V0IdentityToken, V0Salt};

    use crate::credential::v0::V0BroadcastCredential;
    use crate::legacy::serialize::tests::helpers::{LongDataElement, ShortDataElement};
    use crate::legacy::serialize::{AddDataElementError, AdvBuilder, LdtEncodeError, LdtEncoder};
    use crate::legacy::BLE_4_ADV_SVC_MAX_CONTENT_LEN;

    #[test]
    fn build_empty_adv_error() {
        let identity_token = V0IdentityToken::from([0x33; 14]);
        let salt = V0Salt::from([0x01, 0x02]);
        let key_seed = [0xFE; 32];
        let broadcast_cred = V0BroadcastCredential::new(key_seed, identity_token);

        let builder = AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(salt, &broadcast_cred));

        // not enough ciphertext
        assert_eq!(Err(LdtEncodeError::InvalidLength), builder.into_advertisement());
    }

    #[test]
    fn build_adv_one_byte_error() {
        let identity_token = V0IdentityToken::from([0x33; 14]);
        let salt = V0Salt::from([0x01, 0x02]);
        let key_seed = [0xFE; 32];
        let broadcast_cred = V0BroadcastCredential::new(key_seed, identity_token);

        let mut builder =
            AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(salt, &broadcast_cred));
        // 1 byte of DE header
        builder.add_data_element(ShortDataElement::new(vec![])).unwrap();

        // not enough ciphertext
        assert_eq!(Err(LdtEncodeError::InvalidLength), builder.into_advertisement());
    }

    #[test]
    fn add_de_when_full_error() {
        let identity_token = V0IdentityToken::from([0x33; 14]);
        let salt = V0Salt::from([0x01, 0x02]);
        let key_seed = [0xFE; 32];
        let broadcast_cred = V0BroadcastCredential::new(key_seed, identity_token);

        let mut builder =
            AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(salt, &broadcast_cred));
        // 7 bytes will fill it
        builder.add_data_element(ShortDataElement::new(vec![1; 6])).unwrap();

        // 1 more byte is too many
        assert_eq!(
            AddDataElementError::InsufficientAdvSpace,
            builder.add_data_element(ShortDataElement::new(vec![])).unwrap_err()
        );
    }

    #[test]
    fn add_too_much_de_when_almost_full_error() {
        let identity_token = V0IdentityToken::from([0x33; 14]);
        let salt = V0Salt::from([0x01, 0x02]);
        let key_seed = [0xFE; 32];
        let broadcast_cred = V0BroadcastCredential::new(key_seed, identity_token);

        let mut builder =
            AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(salt, &broadcast_cred));
        // 6 bytes will leave 1 byte
        builder.add_data_element(ShortDataElement::new(vec![1; 5])).unwrap();

        // 1 byte would fit, but 2 won't
        assert_eq!(BLE_4_ADV_SVC_MAX_CONTENT_LEN - 1, builder.len);
        assert_eq!(
            AddDataElementError::InsufficientAdvSpace,
            builder.add_data_element(ShortDataElement::new(vec![2])).unwrap_err()
        );
    }

    #[test]
    fn broken_de_impl_hits_expected_panic() {
        let identity_token = V0IdentityToken::from([0x33; 14]);
        let salt = V0Salt::from([0x01, 0x02]);
        let key_seed = [0xFE; 32];
        let broadcast_cred = V0BroadcastCredential::new(key_seed, identity_token);

        let mut builder =
            AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(salt, &broadcast_cred));

        let panic_payload = std::panic::catch_unwind(move || {
            // This DE type can't represent short lengths
            builder.add_data_element(LongDataElement::new(vec![]))
        })
        .unwrap_err();

        assert_eq!(
            "Couldn't encode actual len: DeLengthOutOfRange",
            panic_payload.downcast::<String>().unwrap().deref()
        );
    }
}
