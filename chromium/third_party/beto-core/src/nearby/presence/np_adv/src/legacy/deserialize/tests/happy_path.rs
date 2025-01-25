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

mod unencrypted {
    extern crate std;

    use rand::seq::IteratorRandom;
    use std::{prelude::rust_2021::*, vec};
    use strum::IntoEnumIterator;

    use crypto_provider_default::CryptoProviderImpl;

    use crate::{
        header::V0Encoding,
        legacy::{
            data_elements::{
                actions::{ActionBits, ActionsDataElement, NearbyShare},
                de_type::DataElementType,
                tests::test_des::{
                    random_test_de, TestDataElement, TestDataElementType, TestDeDeserializer,
                },
                tx_power::TxPowerDataElement,
                DynamicSerializeDataElement, SerializeDataElement,
            },
            deserialize::{
                intermediate::IntermediateAdvContents, DataElementDeserializer,
                DeserializedDataElement, StandardDeserializer,
            },
            random_data_elements::random_de_plaintext,
            serialize::{
                tests::helpers::{LongDataElement, ShortDataElement},
                tests::supports_flavor,
                AddDataElementError, AdvBuilder, SerializedAdv, UnencryptedEncoder,
            },
            PacketFlavorEnum, Plaintext, BLE_4_ADV_SVC_MAX_CONTENT_LEN, NP_MAX_DE_CONTENT_LEN,
            NP_MIN_ADV_CONTENT_LEN,
        },
        shared_data::TxPower,
    };

    #[test]
    fn parse_min_len_adv() {
        // 1 byte
        let de = ShortDataElement::new(vec![]);
        let mut builder = AdvBuilder::new(UnencryptedEncoder);
        builder.add_data_element(de.clone()).unwrap();
        let data = builder.into_advertisement().unwrap();
        // extra byte for version header
        assert_eq!(1 + NP_MIN_ADV_CONTENT_LEN, data.len());

        let contents = IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
            V0Encoding::Unencrypted,
            &data.as_slice()[1..],
        )
        .unwrap();
        let unencrypted = contents.as_unencrypted().unwrap();

        assert_eq!(
            vec![TestDataElement::Short(de)],
            unencrypted
                .generic_data_elements::<TestDeDeserializer>()
                .collect::<Result<Vec<_>, _>>()
                .unwrap()
        );
    }

    #[test]
    fn parse_max_len_adv() {
        let de = LongDataElement::new(vec![0x22; NP_MAX_DE_CONTENT_LEN]);
        let mut builder = AdvBuilder::new(UnencryptedEncoder);
        builder.add_data_element(de.clone()).unwrap();
        let data = builder.into_advertisement().unwrap();
        assert_eq!(BLE_4_ADV_SVC_MAX_CONTENT_LEN, data.len());

        let contents = IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
            V0Encoding::Unencrypted,
            &data.as_slice()[1..],
        )
        .unwrap();
        let unencrypted = contents.as_unencrypted().unwrap();

        assert_eq!(
            vec![TestDataElement::Long(de)],
            unencrypted
                .generic_data_elements::<TestDeDeserializer>()
                .collect::<Result<Vec<_>, _>>()
                .unwrap()
        );
    }

    #[test]
    fn tx_power() {
        let de = TxPowerDataElement::from(TxPower::try_from(7).unwrap());
        let boxed: Box<dyn SerializeDataElement<Plaintext>> = Box::new(de.clone());
        let _ = assert_build_adv_and_deser(
            vec![boxed.as_ref().into()],
            &[DeserializedDataElement::TxPower(de)],
        );
    }

    #[test]
    fn actions_min_len() {
        let de = ActionsDataElement::from(ActionBits::default());

        let boxed: Box<dyn SerializeDataElement<Plaintext>> = Box::new(de.clone());
        let data = assert_build_adv_and_deser(
            vec![boxed.as_ref().into()],
            &[DeserializedDataElement::Actions(de)],
        );
        // version, de header, de contents
        assert_eq!(3, data.len());
    }

    #[test]
    fn typical_tx_power_and_actions() {
        let tx = TxPowerDataElement::from(TxPower::try_from(7).unwrap());
        let mut action_bits = ActionBits::default();
        action_bits.set_action(NearbyShare::from(true));
        let actions = ActionsDataElement::from(action_bits);

        let tx_boxed: Box<dyn SerializeDataElement<Plaintext>> = Box::new(tx.clone());
        let actions_boxed: Box<dyn SerializeDataElement<Plaintext>> = Box::new(actions.clone());
        let expected =
            vec![DeserializedDataElement::TxPower(tx), DeserializedDataElement::Actions(actions)];
        let adv = assert_build_adv_and_deser(
            vec![tx_boxed.as_ref().into(), actions_boxed.as_ref().into()],
            &expected,
        );

        let contents = assert_deserialized_contents::<StandardDeserializer>(&expected, &adv);
        assert_eq!(
            expected,
            contents
                .as_unencrypted()
                .unwrap()
                .data_elements()
                .collect::<Result<Vec<_>, _>>()
                .unwrap()
        );
    }

    #[test]
    fn random_normal_des_rountrip() {
        do_random_roundtrip_test::<StandardDeserializer, _>(
            DataElementType::iter()
                .filter(|t| supports_flavor(*t, PacketFlavorEnum::Plaintext))
                .collect(),
            random_de_plaintext,
            |builder, de| match de {
                DeserializedDataElement::Actions(a) => builder.add_data_element(a),
                DeserializedDataElement::TxPower(tx) => builder.add_data_element(tx),
            },
        )
    }

    #[test]
    fn random_test_des_rountrip() {
        do_random_roundtrip_test::<TestDeDeserializer, _>(
            TestDataElementType::iter().collect::<Vec<_>>(),
            random_test_de,
            |builder, de| match de {
                TestDataElement::Short(s) => builder.add_data_element(s),
                TestDataElement::Long(l) => builder.add_data_element(l),
            },
        )
    }

    fn do_random_roundtrip_test<D, F>(
        de_types: Vec<D::DeTypeDisambiguator>,
        build_de: F,
        add_de: impl Fn(
            &mut AdvBuilder<UnencryptedEncoder>,
            D::Deserialized<Plaintext>,
        ) -> Result<(), AddDataElementError>,
    ) where
        D: DataElementDeserializer,
        F: Fn(D::DeTypeDisambiguator, &mut rand_ext::rand_pcg::Pcg64) -> D::Deserialized<Plaintext>,
    {
        let mut rng = rand_ext::seeded_rng();

        for _ in 0..10_000 {
            let mut des = Vec::new();
            let mut builder = AdvBuilder::new(UnencryptedEncoder);

            let mut possible_de_types = de_types.clone();
            loop {
                if possible_de_types.is_empty() {
                    break;
                }
                let (i, _) = possible_de_types.iter().enumerate().choose(&mut rng).unwrap();
                let de_type = possible_de_types.remove(i);
                let de = build_de(de_type, &mut rng);

                let add_res = add_de(&mut builder, de.clone());

                if let Err(e) = add_res {
                    match e {
                        AddDataElementError::InsufficientAdvSpace => {
                            // out of room
                            break;
                        }
                    }
                }

                des.push(de);
            }

            let serialized = builder.into_advertisement().unwrap();
            assert_deserialized_contents::<D>(&des, &serialized);
        }
    }

    fn assert_build_adv_and_deser(
        des: Vec<DynamicSerializeDataElement<Plaintext>>,
        expected: &[DeserializedDataElement<Plaintext>],
    ) -> SerializedAdv {
        let mut builder = AdvBuilder::new(UnencryptedEncoder);
        for de in des {
            builder.add_data_element(de).unwrap();
        }
        let adv = builder.into_advertisement().unwrap();

        assert_deserialized_contents::<StandardDeserializer>(expected, &adv);

        adv
    }

    fn assert_deserialized_contents<'a, D: DataElementDeserializer>(
        expected: &[D::Deserialized<Plaintext>],
        adv: &'a SerializedAdv,
    ) -> IntermediateAdvContents<'a> {
        let contents = IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
            V0Encoding::Unencrypted,
            &adv.as_slice()[1..],
        )
        .unwrap();

        let unencrypted = contents.as_unencrypted().unwrap();
        assert_eq!(
            expected,
            unencrypted
                .generic_data_elements::<D>()
                .collect::<Result<Vec<_>, _>>()
                .unwrap()
                .as_slice()
        );
        contents
    }
}

mod ldt {
    use crate::credential::matched::HasIdentityMatch;
    use crate::legacy::data_elements::actions::CallTransfer;
    use crate::legacy::data_elements::de_type::MAX_DE_ENCODED_LEN;
    use crate::{
        credential::v0::V0BroadcastCredential,
        header::V0Encoding,
        legacy::{
            data_elements::{
                actions::{ActionBits, ActionsDataElement, NearbyShare},
                de_type::DataElementType,
                tests::test_des::TestDataElementType,
                tests::test_des::{TestDataElement, TestDeDeserializer},
                tx_power::TxPowerDataElement,
                DataElementSerializationBuffer, DynamicSerializeDataElement, SerializeDataElement,
            },
            deserialize::{
                intermediate::IntermediateAdvContents, DecryptedAdvContents,
                DeserializedDataElement,
            },
            deserialize::{DataElementDeserializer, StandardDeserializer},
            random_data_elements::random_de_ciphertext,
            serialize::{
                tests::helpers::ShortDataElement, tests::supports_flavor, AddDataElementError,
                AdvBuilder, LdtEncoder, SerializedAdv,
            },
            Ciphertext, PacketFlavorEnum, BLE_4_ADV_SVC_MAX_CONTENT_LEN, NP_MAX_ADV_CONTENT_LEN,
        },
        shared_data::TxPower,
    };
    use alloc::boxed::Box;
    use alloc::vec;
    use alloc::vec::Vec;
    use crypto_provider_default::CryptoProviderImpl;
    use ldt_np_adv::{V0IdentityToken, V0Salt, V0_IDENTITY_TOKEN_LEN};
    use rand::seq::IteratorRandom;
    use rand::Rng;
    use strum::IntoEnumIterator;

    #[test]
    fn parse_min_len() {
        // 2 bytes total is the minimum
        let de = ShortDataElement::new(vec![7]);
        let boxed: Box<dyn SerializeDataElement<Ciphertext>> = Box::new(de.clone());
        let (adv, _decrypted) = build_and_assert_deserialized_matches::<TestDeDeserializer>(
            vec![boxed.as_ref().into()],
            &[TestDataElement::Short(de)],
        );

        // version and salt
        assert_eq!(1 + 2 + ldt_np_adv::VALID_INPUT_LEN.start, adv.len());
    }

    #[test]
    fn parse_max_len() {
        // 7 bytes total
        let de = ShortDataElement::new(vec![1; 6]);
        let boxed: Box<dyn SerializeDataElement<Ciphertext>> = Box::new(de.clone());
        let (adv, _decrypted) = build_and_assert_deserialized_matches::<TestDeDeserializer>(
            vec![boxed.as_ref().into()],
            &[TestDataElement::Short(de)],
        );

        assert_eq!(BLE_4_ADV_SVC_MAX_CONTENT_LEN, adv.len());
    }

    #[test]
    fn tx_power() {
        let de = TxPowerDataElement::from(TxPower::try_from(3).unwrap());
        let boxed: Box<dyn SerializeDataElement<Ciphertext>> = Box::new(de.clone());
        let _adv = build_and_assert_deserialized_matches::<StandardDeserializer>(
            vec![boxed.as_ref().into()],
            &[DeserializedDataElement::TxPower(de)],
        );
    }

    #[test]
    fn actions_min_len() {
        let de = ActionsDataElement::from(ActionBits::default());
        let boxed: Box<dyn SerializeDataElement<Ciphertext>> = Box::new(de.clone());
        let (adv, _decrypted) = build_and_assert_deserialized_matches::<StandardDeserializer>(
            vec![boxed.as_ref().into()],
            &[DeserializedDataElement::Actions(de)],
        );

        // 2 byte actions DE
        assert_eq!(1 + 2 + 14 + 2, adv.len());
    }

    #[test]
    fn typical_tx_power_and_actions() {
        let tx = TxPowerDataElement::from(TxPower::try_from(7).unwrap());
        let mut action_bits = ActionBits::default();
        action_bits.set_action(NearbyShare::from(true));
        action_bits.set_action(CallTransfer::from(true));
        let actions = ActionsDataElement::from(action_bits);
        let tx_boxed: Box<dyn SerializeDataElement<Ciphertext>> = Box::new(tx.clone());
        let actions_boxed: Box<dyn SerializeDataElement<Ciphertext>> = Box::new(actions.clone());
        let expected =
            [DeserializedDataElement::TxPower(tx), DeserializedDataElement::Actions(actions)];
        let (_adv, decrypted) = build_and_assert_deserialized_matches::<StandardDeserializer>(
            vec![tx_boxed.as_ref().into(), actions_boxed.as_ref().into()],
            &expected,
        );
        assert_eq!(
            &expected,
            decrypted.data_elements().collect::<Result<Vec<_>, _>>().unwrap().as_slice()
        );
    }

    #[test]
    fn random_normal_des_roundtrip() {
        do_random_roundtrip_test::<StandardDeserializer, _>(
            DataElementType::iter()
                .filter(|t| supports_flavor(*t, PacketFlavorEnum::Ciphertext))
                .collect(),
            random_de_ciphertext,
            |builder, de| match de {
                DeserializedDataElement::Actions(a) => builder.add_data_element(a),
                DeserializedDataElement::TxPower(tx) => builder.add_data_element(tx),
            },
            |de| match de {
                DeserializedDataElement::Actions(a) => serialized_len(a),
                DeserializedDataElement::TxPower(tx) => serialized_len(tx),
            },
        )
    }

    #[test]
    fn random_test_des_roundtrip() {
        do_random_roundtrip_test::<TestDeDeserializer, _>(
            vec![TestDataElementType::Short],
            |_, rng| {
                let len = rng.gen_range(3_usize..MAX_DE_ENCODED_LEN.into());
                let mut data = vec![0; len];
                rng.fill(&mut data[..]);
                TestDataElement::Short(ShortDataElement::new(data))
            },
            |builder, de| builder.add_data_element(de),
            serialized_len,
        )
    }

    fn do_random_roundtrip_test<D, F>(
        de_types: Vec<D::DeTypeDisambiguator>,
        build_de: F,
        add_de: impl Fn(
            &mut AdvBuilder<LdtEncoder<CryptoProviderImpl>>,
            D::Deserialized<Ciphertext>,
        ) -> Result<(), AddDataElementError>,
        serialized_len: impl Fn(&D::Deserialized<Ciphertext>) -> usize,
    ) where
        D: DataElementDeserializer,
        F: Fn(
            D::DeTypeDisambiguator,
            &mut rand_ext::rand_pcg::Pcg64,
        ) -> D::Deserialized<Ciphertext>,
    {
        let mut rng = rand_ext::seeded_rng();

        for _ in 0..1_000 {
            let mut added_des = Vec::new();
            let mut current_len = 0;

            let key_seed: [u8; 32] = rng.gen();
            let salt: V0Salt = rng.gen::<[u8; 2]>().into();
            let identity_token = V0IdentityToken::from(rng.gen::<[u8; V0_IDENTITY_TOKEN_LEN]>());

            let broadcast_cred = V0BroadcastCredential::new(key_seed, identity_token);

            let mut builder =
                AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(salt, &broadcast_cred));

            let mut possible_de_types = de_types.clone();
            loop {
                if possible_de_types.is_empty() {
                    break;
                }
                let (i, _) = possible_de_types.iter().enumerate().choose(&mut rng).unwrap();
                let de_type = possible_de_types.remove(i);
                let de = build_de(de_type, &mut rng);

                if let Err(e) = add_de(&mut builder, de.clone()) {
                    match e {
                        AddDataElementError::InsufficientAdvSpace => {
                            if current_len
                                < ldt_np_adv::VALID_INPUT_LEN.start - V0_IDENTITY_TOKEN_LEN
                            {
                                // keep trying, not enough for LDT
                                possible_de_types.push(de_type);
                                continue;
                            }
                            // out of room
                            break;
                        }
                    }
                }

                current_len += serialized_len(&de);
                added_des.push(de);
            }

            let adv = builder.into_advertisement().unwrap();
            assert_deserialized_contents::<D>(&key_seed, identity_token, salt, &adv, &added_des);
        }
    }

    fn build_and_assert_deserialized_matches<D: DataElementDeserializer>(
        des: Vec<DynamicSerializeDataElement<Ciphertext>>,
        expected: &[D::Deserialized<Ciphertext>],
    ) -> (SerializedAdv, DecryptedAdvContents) {
        let key_seed = [0; 32];
        let identity_token = V0IdentityToken::from([0x33; V0_IDENTITY_TOKEN_LEN]);
        let salt = V0Salt::from([0x01, 0x02]);
        let broadcast_cred = V0BroadcastCredential::new(key_seed, identity_token);
        let mut builder =
            AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(salt, &broadcast_cred));

        for de in des {
            builder.add_data_element(de).unwrap();
        }
        let adv = builder.into_advertisement().unwrap();

        let decrypted =
            assert_deserialized_contents::<D>(&key_seed, identity_token, salt, &adv, expected);
        (adv, decrypted)
    }

    fn assert_deserialized_contents<D: DataElementDeserializer>(
        key_seed: &[u8; 32],
        identity_token: V0IdentityToken,
        salt: V0Salt,
        adv: &SerializedAdv,
        expected: &[D::Deserialized<Ciphertext>],
    ) -> DecryptedAdvContents {
        let contents = IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
            V0Encoding::Ldt,
            &adv.as_slice()[1..],
        )
        .unwrap();
        let ldt = contents.as_ldt().unwrap();
        let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(key_seed);
        let identity_token_hmac: [u8; 32] = hkdf
            .v0_identity_token_hmac_key()
            .calculate_hmac::<CryptoProviderImpl>(identity_token.as_slice());
        let decrypter =
            ldt_np_adv::build_np_adv_decrypter_from_key_seed(&hkdf, identity_token_hmac);
        let decrypted = ldt.try_decrypt(&decrypter).unwrap();

        assert_eq!(salt, decrypted.salt());
        assert_eq!(identity_token, decrypted.identity_token());

        assert_eq!(
            expected,
            decrypted.generic_data_elements::<D>().collect::<Result<Vec<_>, _>>().unwrap()
        );

        decrypted
    }

    /// serialized length including header
    fn serialized_len<S: SerializeDataElement<Ciphertext>>(de: &S) -> usize {
        let mut buf = DataElementSerializationBuffer::new(NP_MAX_ADV_CONTENT_LEN).unwrap();
        de.serialize_contents(&mut buf).unwrap();
        buf.len() + 1
    }
}

mod coverage_gaming {
    use alloc::format;

    use crypto_provider_default::CryptoProviderImpl;

    use crate::header::V0Encoding;
    use crate::legacy::data_elements::de_type::DataElementType;
    use crate::legacy::deserialize::intermediate::{IntermediateAdvContents, LdtAdvContents};
    use crate::legacy::deserialize::{
        AdvDeserializeError, DeIterator, RawDataElement, StandardDeserializer,
    };
    use crate::legacy::Plaintext;

    #[test]
    fn iac_debug_eq_test_helpers() {
        let iac = IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
            V0Encoding::Unencrypted,
            &[0xFF],
        )
        .unwrap();
        let _ = format!("{:?}", iac);
        assert_eq!(iac, iac);
    }

    #[test]
    fn iac_test_helpers() {
        assert_eq!(
            None,
            IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
                V0Encoding::Unencrypted,
                &[0xFF],
            )
            .unwrap()
            .as_ldt()
        );
        assert_eq!(
            None,
            IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
                V0Encoding::Ldt,
                &[0xFF; 18],
            )
            .unwrap()
            .as_unencrypted()
        );
    }

    #[test]
    fn ade_debug_clone() {
        let _ = format!("{:?}", AdvDeserializeError::NoDataElements.clone());
    }

    #[test]
    fn rde_debug_eq() {
        let rde = RawDataElement::<'_, StandardDeserializer> {
            de_type: DataElementType::Actions,
            contents: &[],
        };

        let _ = format!("{:?}", rde);
        assert_eq!(rde, rde);
    }

    #[test]
    fn de_iterator_debug_clone_eq() {
        let i = DeIterator::<'_, Plaintext>::new(&[]);
        let _ = format!("{:?}", i.clone());
        assert_eq!(i, i);
    }

    #[test]
    fn ldt_adv_contents_debug() {
        let lac = LdtAdvContents::new::<CryptoProviderImpl>([0; 2].into(), &[0; 16]).unwrap();
        let _ = format!("{:?}", lac);
    }
}
