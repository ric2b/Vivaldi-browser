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

    use crate::header::VERSION_HEADER_V0_UNENCRYPTED;
    use crate::legacy::data_elements::actions::{ActionBits, ActionsDataElement, NearbyShare};
    use crate::legacy::data_elements::tx_power::TxPowerDataElement;
    use crate::legacy::serialize::tests::helpers::{LongDataElement, ShortDataElement};
    use crate::legacy::serialize::{AdvBuilder, UnencryptedEncoder};
    use crate::legacy::{
        BLE_4_ADV_SVC_MAX_CONTENT_LEN, NP_MAX_ADV_CONTENT_LEN, NP_MAX_DE_CONTENT_LEN,
    };
    use crate::shared_data::TxPower;

    #[test]
    fn adv_min_size() {
        // empty isn't allowed, so 1 byte of payload is the smallest possible
        let mut builder = AdvBuilder::new(UnencryptedEncoder);

        builder.add_data_element(ShortDataElement::new(vec![])).unwrap();
        let adv = builder.into_advertisement().unwrap();
        assert_eq!(&[VERSION_HEADER_V0_UNENCRYPTED, 0x0E], adv.as_slice());
    }

    #[test]
    fn adv_max_size_multi_de() {
        let mut builder = AdvBuilder::new(UnencryptedEncoder);

        builder.add_data_element(ShortDataElement::new(vec![1; 10])).unwrap();
        // max len after previous DE & headers
        let de2_contents = vec![2; NP_MAX_ADV_CONTENT_LEN - 1 - 10 - 1];
        builder.add_data_element(LongDataElement::new(de2_contents.clone())).unwrap();
        let adv = builder.into_advertisement().unwrap();
        assert_eq!(BLE_4_ADV_SVC_MAX_CONTENT_LEN, adv.len());
        assert_eq!(
            &[
                [VERSION_HEADER_V0_UNENCRYPTED, 0xAE].as_slice(),
                [1; 10].as_slice(),
                &[0x4F],
                &de2_contents
            ]
            .concat(),
            adv.as_slice()
        );
    }

    #[test]
    fn adv_max_size_single_de() {
        let mut builder = AdvBuilder::new(UnencryptedEncoder);

        builder
            .add_data_element(LongDataElement::new(vec![1; NP_MAX_DE_CONTENT_LEN].clone()))
            .unwrap();
        let adv = builder.into_advertisement().unwrap();
        assert_eq!(BLE_4_ADV_SVC_MAX_CONTENT_LEN, adv.len());
        assert_eq!(
            &[[VERSION_HEADER_V0_UNENCRYPTED, 0xFF].as_slice(), &[1; NP_MAX_DE_CONTENT_LEN],]
                .concat(),
            adv.as_slice()
        );
    }

    #[rustfmt::skip]
    #[test]
    fn typical_tx_power_and_actions() {
        let mut builder = AdvBuilder::new(UnencryptedEncoder);
        builder.add_data_element(TxPowerDataElement::from(TxPower::try_from(3).unwrap())).unwrap();

        let mut action = ActionBits::default();
        action.set_action(NearbyShare::from(true));
        builder.add_data_element(ActionsDataElement::from(action)).unwrap();

        let packet = builder.into_advertisement().unwrap();
        assert_eq!(
            &[
                VERSION_HEADER_V0_UNENCRYPTED,
                0x15, 0x03, // Tx Power DE with value 3
                0x26, 0x00, 0x40, // Actions DE w/ bit 9
            ],
            packet.as_slice()
        );
    }
}

mod ldt_encoder {
    use alloc::vec;

    use crypto_provider_default::CryptoProviderImpl;
    use ldt::LdtCipher;
    use ldt_np_adv::{
        salt_padder, NpLdtEncryptCipher, V0IdentityToken, V0Salt, V0_IDENTITY_TOKEN_LEN,
    };

    use crate::credential::v0::V0BroadcastCredential;
    use crate::header::VERSION_HEADER_V0_LDT;
    use crate::legacy::data_elements::actions::tests::LastBit;
    use crate::legacy::data_elements::actions::{ActionBits, ActionsDataElement, PhoneHub};
    use crate::legacy::data_elements::tx_power::TxPowerDataElement;
    use crate::legacy::serialize::tests::helpers::ShortDataElement;
    use crate::legacy::serialize::{AdvBuilder, LdtEncoder, SerializedAdv};
    use crate::legacy::BLE_4_ADV_SVC_MAX_CONTENT_LEN;
    use crate::shared_data::TxPower;

    #[test]
    fn adv_min_size() {
        // need at least 2 bytes to make 1 full block
        let _ = assert_ldt_adv(&[0x1E, 0xAA], |builder| {
            builder.add_data_element(ShortDataElement::new(vec![0xAA])).unwrap();
        });
    }

    #[test]
    fn adv_max_size_multi_de() {
        let adv = assert_ldt_adv(&[0x15, 0x03, 0x4E, 0x01, 0x01, 0x01, 0x01], |builder| {
            builder
                .add_data_element(TxPowerDataElement::from(TxPower::try_from(3).unwrap()))
                .unwrap();
            builder.add_data_element(ShortDataElement::new(vec![1; 4])).unwrap();
        });
        assert_eq!(BLE_4_ADV_SVC_MAX_CONTENT_LEN, adv.len());
    }

    #[test]
    fn adv_max_size_single_de() {
        let adv = assert_ldt_adv(&[0x6E, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01], |builder| {
            builder.add_data_element(ShortDataElement::new(vec![1; 6])).unwrap();
        });
        assert_eq!(BLE_4_ADV_SVC_MAX_CONTENT_LEN, adv.len());
    }

    #[test]
    fn adv_tx_power_max_length_actions() {
        let adv = assert_ldt_adv(&[0x15, 0x03, 0x36, 0x00, 0x00, 0x01], |builder| {
            builder
                .add_data_element(TxPowerDataElement::from(TxPower::try_from(3).unwrap()))
                .unwrap();
            let mut action = ActionBits::default();
            action.set_action(LastBit::from(true));
            builder.add_data_element(ActionsDataElement::from(action)).unwrap();
        });
        // TODO this should be the max adv size once action size gets increased
        assert_eq!(BLE_4_ADV_SVC_MAX_CONTENT_LEN - 1, adv.len());
    }

    #[test]
    fn adv_typical_tx_power_and_actions() {
        let _ = assert_ldt_adv(&[0x15, 0x03, 0x26, 0x00, 0x10], |builder| {
            builder
                .add_data_element(TxPowerDataElement::from(TxPower::try_from(3).unwrap()))
                .unwrap();
            let mut action = ActionBits::default();
            action.set_action(PhoneHub::from(true));
            builder.add_data_element(ActionsDataElement::from(action)).unwrap();
        });
    }

    /// Build an LDT advertisement using the DEs provided by `add_des`, and assert that the resulting
    /// advertisement matches the result of encrypting `identity token || after_identity_token`
    fn assert_ldt_adv(
        after_identity_token: &[u8],
        add_des: impl Fn(&mut AdvBuilder<LdtEncoder<CryptoProviderImpl>>),
    ) -> SerializedAdv {
        let key_seed = [0; 32];
        let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
        let ldt = NpLdtEncryptCipher::<CryptoProviderImpl>::new(&hkdf.v0_ldt_key());
        let identity_token = V0IdentityToken::from([0x33; V0_IDENTITY_TOKEN_LEN]);
        let salt = V0Salt::from([0x01, 0x02]);

        let mut plaintext = identity_token.as_slice().to_vec();
        plaintext.extend_from_slice(after_identity_token);
        ldt.encrypt(&mut plaintext, &salt_padder::<CryptoProviderImpl>(salt)).unwrap();
        let ciphertext = plaintext;

        let broadcast_cred = V0BroadcastCredential::new(key_seed, identity_token);

        let mut builder =
            AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(salt, &broadcast_cred));
        add_des(&mut builder);

        let adv = builder.into_advertisement().unwrap();

        let mut expected = vec![VERSION_HEADER_V0_LDT];
        expected.extend_from_slice(&salt.bytes());
        expected.extend_from_slice(&ciphertext);
        assert_eq!(&expected, adv.as_slice());
        adv
    }
}

mod tx_de {
    use crate::header::VERSION_HEADER_V0_UNENCRYPTED;
    use crate::legacy::data_elements::tx_power::TxPowerDataElement;
    use crate::legacy::serialize::{AdvBuilder, UnencryptedEncoder};
    use crate::shared_data::TxPower;

    #[test]
    fn tx_power_produces_1_byte() {
        let mut builder = AdvBuilder::new(UnencryptedEncoder);

        builder.add_data_element(TxPowerDataElement::from(TxPower::try_from(3).unwrap())).unwrap();
        let adv = builder.into_advertisement().unwrap();
        assert_eq!(&[VERSION_HEADER_V0_UNENCRYPTED, 0x15, 0x03], adv.as_slice());
    }
}

mod actions_de {
    use crate::header::VERSION_HEADER_V0_UNENCRYPTED;
    use crate::legacy::data_elements::actions::tests::LastBit;
    use crate::legacy::data_elements::actions::{ActionBits, ActionsDataElement};
    use crate::legacy::serialize::{AdvBuilder, UnencryptedEncoder};

    #[test]
    fn no_bits_set_produces_1_byte() {
        let mut builder = AdvBuilder::new(UnencryptedEncoder);

        builder.add_data_element(ActionsDataElement::from(ActionBits::default())).unwrap();
        let adv = builder.into_advertisement().unwrap();
        assert_eq!(&[VERSION_HEADER_V0_UNENCRYPTED, 0x16, 0x00], adv.as_slice());
    }

    #[test]
    fn lowest_bit_set_produces_max_len() {
        let mut builder = AdvBuilder::new(UnencryptedEncoder);

        let mut bits = ActionBits::default();
        bits.set_action(LastBit::from(true));
        builder.add_data_element(ActionsDataElement::from(bits)).unwrap();
        let adv = builder.into_advertisement().unwrap();
        assert_eq!(&[VERSION_HEADER_V0_UNENCRYPTED, 0x36, 0x00, 0x00, 0x01], adv.as_slice());
    }
}
