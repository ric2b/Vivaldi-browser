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
    use crypto_provider_default::CryptoProviderImpl;

    use crate::header::V0Encoding;
    use crate::legacy::data_elements::actions::{ActionBits, ActionsDataElement, ActiveUnlock};
    use crate::legacy::data_elements::de_type::{DeEncodedLength, DeTypeCode};
    use crate::legacy::data_elements::tx_power::TxPowerDataElement;
    use crate::legacy::data_elements::{DataElementDeserializeError, DeserializeDataElement};
    use crate::legacy::deserialize::intermediate::IntermediateAdvContents;
    use crate::legacy::serialize::tests::serialize;
    use crate::legacy::{Ciphertext, PacketFlavorEnum, Plaintext};

    #[test]
    fn iterate_tx_power_invalid_de_len_error() {
        assert_deser_error(
            // bogus 6-byte tx power de -- only allows length = 1
            &[0x65, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06],
            DataElementDeserializeError::InvalidDeLength {
                de_type: TxPowerDataElement::DE_TYPE_CODE,
                len: DeEncodedLength::from(6),
            },
        );
    }

    #[test]
    fn iterate_tx_power_invalid_power_error() {
        assert_deser_error(
            // power too high
            &[0x15, 0x7F],
            DataElementDeserializeError::DeserializeError {
                de_type: TxPowerDataElement::DE_TYPE_CODE,
            },
        );
    }

    #[test]
    fn iterate_actions_invalid_de_len_error() {
        assert_deser_error(
            // bogus 0-byte actions de
            &[0x06],
            DataElementDeserializeError::InvalidDeLength {
                de_type: ActionsDataElement::<Plaintext>::DE_TYPE_CODE,
                len: DeEncodedLength::from(0),
            },
        );
    }

    #[test]
    fn iterate_actions_ciphertext_only_bit_error() {
        let mut bits = ActionBits::default();
        bits.set_action(ActiveUnlock::from(true));
        assert_deser_error(
            serialize(&ActionsDataElement::<Ciphertext>::from(bits)).as_slice(),
            DataElementDeserializeError::FlavorNotSupported {
                de_type: ActionsDataElement::<Plaintext>::DE_TYPE_CODE,
                flavor: PacketFlavorEnum::Plaintext,
            },
        );
    }

    #[test]
    fn iterate_invalid_de_type_error() {
        assert_deser_error(
            &[0x0F],
            DataElementDeserializeError::InvalidDeType {
                de_type: DeTypeCode::try_from(0x0F).unwrap(),
            },
        );
    }

    #[test]
    fn iterate_multiple_de_types_same_value() {
        let input = &[0x15, 0x00, 0x15, 0x00];
        let mut it = IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
            V0Encoding::Unencrypted,
            input,
        )
        .unwrap()
        .as_unencrypted()
        .unwrap()
        .data_elements();

        // first element will be valid
        let _ = it.next();
        // second will be an error since it is the same type as the first
        let err = it.next().unwrap().unwrap_err();

        assert_eq!(err, DataElementDeserializeError::DuplicateDeTypes);
    }

    #[test]
    fn iterate_truncated_contents_error() {
        assert_deser_error(
            // length 3, but only 2 bytes provided
            &[0x36, 0x01, 0x02],
            DataElementDeserializeError::InvalidStructure,
        );
    }

    fn assert_deser_error(input: &[u8], err: DataElementDeserializeError) {
        let contents = IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
            V0Encoding::Unencrypted,
            input,
        )
        .unwrap();

        assert_eq!(
            err,
            contents.as_unencrypted().unwrap().data_elements().next().unwrap().unwrap_err()
        );
    }
}

mod ldt {

    // see unencrypted tests above for basic things that are the same between unencrypted and ldt,
    // like how an invalid de type is handled

    use crate::credential::matched::HasIdentityMatch;
    use crate::credential::v0::V0BroadcastCredential;
    use crate::header::V0Encoding;
    use crate::legacy::data_elements::actions::tests::PlaintextOnly;
    use crate::legacy::data_elements::actions::{ActionBits, ActionsDataElement};
    use crate::legacy::data_elements::tx_power::TxPowerDataElement;
    use crate::legacy::data_elements::{DataElementDeserializeError, DeserializeDataElement};
    use crate::legacy::deserialize::intermediate::IntermediateAdvContents;
    use crate::legacy::deserialize::DecryptError;
    use crate::legacy::serialize::{AdvBuilder, LdtEncoder};
    use crate::legacy::{Ciphertext, PacketFlavorEnum};
    use crate::shared_data::TxPower;
    use alloc::vec::Vec;
    use crypto_provider_default::CryptoProviderImpl;
    use ldt_np_adv::{
        build_np_adv_decrypter, AuthenticatedNpLdtDecryptCipher, V0IdentityToken, V0Salt,
        V0_IDENTITY_TOKEN_LEN,
    };

    #[test]
    fn iterate_actions_invalid_flavor_error() {
        let mut bits = ActionBits::default();
        bits.set_action(PlaintextOnly::from(true));

        let key_seed = [0; 32];
        let identity_token = V0IdentityToken::from([0x33; V0_IDENTITY_TOKEN_LEN]);
        let salt = V0Salt::from([0x01, 0x02]);
        let broadcast_cred = V0BroadcastCredential::new(key_seed, identity_token);
        let mut builder =
            AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(salt, &broadcast_cred));

        builder.add_data_element(ActionsDataElement::from(bits)).unwrap();

        let adv = builder.into_advertisement().unwrap();

        let contents = IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
            V0Encoding::Ldt,
            &adv.as_slice()[1..],
        )
        .unwrap();
        let ldt = contents.as_ldt().unwrap();
        let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
        let identity_token_hmac: [u8; 32] = hkdf
            .v0_identity_token_hmac_key()
            .calculate_hmac::<CryptoProviderImpl>(identity_token.as_slice());
        let decrypter =
            ldt_np_adv::build_np_adv_decrypter_from_key_seed(&hkdf, identity_token_hmac);
        let decrypted = ldt.try_decrypt(&decrypter).unwrap();

        assert_eq!(salt, decrypted.salt());
        assert_eq!(identity_token, decrypted.identity_token());

        assert_eq!(
            DataElementDeserializeError::FlavorNotSupported {
                de_type: ActionsDataElement::<Ciphertext>::DE_TYPE_CODE,
                flavor: PacketFlavorEnum::Ciphertext,
            },
            decrypted.data_elements().next().unwrap().unwrap_err()
        )
    }

    #[test]
    fn decrypter_wrong_identity_token_hmac_no_match() {
        build_and_deser_with_invalid_decrypter_error(
            |adv| adv,
            |key_seed, _identity_token| {
                ldt_np_adv::build_np_adv_decrypter_from_key_seed(
                    &np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(key_seed),
                    [0xFA; 32],
                )
            },
        )
    }

    #[test]
    fn decrypter_wrong_key_seed_no_match() {
        build_and_deser_with_invalid_decrypter_error(
            |adv| adv,
            |key_seed, identity_token| {
                let correct_hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(key_seed);
                ldt_np_adv::build_np_adv_decrypter_from_key_seed(
                    // wrong key seed
                    &np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&[0xFA; 32]),
                    correct_hkdf
                        .v0_identity_token_hmac_key()
                        .calculate_hmac::<CryptoProviderImpl>(identity_token.as_slice()),
                )
            },
        )
    }

    #[test]
    fn decrypter_wrong_ldt_key_no_match() {
        build_and_deser_with_invalid_decrypter_error(
            |adv| adv,
            |key_seed, identity_token| {
                let hkdf = &np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(key_seed);
                let bogus_key = ldt::LdtKey::from_concatenated(&[0xFA; 64]);
                build_np_adv_decrypter(
                    &bogus_key,
                    hkdf.v0_identity_token_hmac_key()
                        .calculate_hmac::<CryptoProviderImpl>(identity_token.as_slice()),
                    hkdf.v0_identity_token_hmac_key(),
                )
            },
        )
    }

    #[test]
    fn decrypter_wrong_identity_token_hmac_key_no_match() {
        build_and_deser_with_invalid_decrypter_error(
            |adv| adv,
            |key_seed, identity_token| {
                let hkdf = &np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(key_seed);
                build_np_adv_decrypter(
                    &hkdf.v0_ldt_key(),
                    hkdf.v0_identity_token_hmac_key()
                        .calculate_hmac::<CryptoProviderImpl>(identity_token.as_slice()),
                    [0xFA; 32].into(),
                )
            },
        )
    }

    #[test]
    fn mangled_de_ciphertext_no_match() {
        build_and_deser_with_invalid_decrypter_error(
            |mut adv| {
                *adv.last_mut().unwrap() ^= 0x01;
                adv
            },
            build_correct_decrypter,
        )
    }

    #[test]
    fn mangled_token_ciphertext_no_match() {
        build_and_deser_with_invalid_decrypter_error(
            |mut adv| {
                adv[10] ^= 0x01;
                adv
            },
            build_correct_decrypter,
        )
    }

    #[test]
    fn mangled_salt_no_match() {
        build_and_deser_with_invalid_decrypter_error(
            |mut adv| {
                adv[1] ^= 0x01;
                adv
            },
            build_correct_decrypter,
        )
    }

    #[test]
    fn extended_ciphertext_no_match() {
        build_and_deser_with_invalid_decrypter_error(
            |mut adv| {
                adv.push(0xEE);
                adv
            },
            build_correct_decrypter,
        )
    }

    fn build_correct_decrypter(
        key_seed: &[u8; 32],
        identity_token: &V0IdentityToken,
    ) -> AuthenticatedNpLdtDecryptCipher<CryptoProviderImpl> {
        let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(key_seed);
        ldt_np_adv::build_np_adv_decrypter_from_key_seed(
            &hkdf,
            hkdf.v0_identity_token_hmac_key()
                .calculate_hmac::<CryptoProviderImpl>(identity_token.as_slice()),
        )
    }

    fn build_and_deser_with_invalid_decrypter_error(
        alter_adv: impl Fn(Vec<u8>) -> Vec<u8>,
        build_decrypter: impl Fn(
            &[u8; 32],
            &V0IdentityToken,
        ) -> AuthenticatedNpLdtDecryptCipher<CryptoProviderImpl>,
    ) {
        let key_seed = [0; 32];
        let identity_token = V0IdentityToken::from([0x33; V0_IDENTITY_TOKEN_LEN]);
        let salt = V0Salt::from([0x01, 0x02]);
        let broadcast_cred = V0BroadcastCredential::new(key_seed, identity_token);
        let mut builder =
            AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(salt, &broadcast_cred));

        builder.add_data_element(TxPowerDataElement::from(TxPower::try_from(7).unwrap())).unwrap();

        let adv = builder.into_advertisement().unwrap();
        let altered_adv = alter_adv(adv.as_slice().to_vec());

        let contents = IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
            V0Encoding::Ldt,
            &altered_adv.as_slice()[1..],
        )
        .unwrap();
        let ldt = contents.as_ldt().unwrap();
        let decrypter = build_decrypter(&key_seed, &identity_token);
        assert_eq!(DecryptError::DecryptOrVerifyError, ldt.try_decrypt(&decrypter).unwrap_err());
    }
}
