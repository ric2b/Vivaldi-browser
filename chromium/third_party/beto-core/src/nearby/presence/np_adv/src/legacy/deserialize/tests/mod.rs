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

#![allow(unused_results, clippy::unwrap_used)]

extern crate std;

use super::*;
use crate::header::V0Encoding;
use crate::legacy::deserialize::intermediate::{IntermediateAdvContents, LdtAdvContents};
use crate::{
    credential::v0::V0BroadcastCredential,
    legacy::{
        serialize::{AdvBuilder, LdtEncoder},
        BLE_4_ADV_SVC_MAX_CONTENT_LEN,
    },
    shared_data::TxPower,
};
use crypto_provider::CryptoProvider;
use crypto_provider_default::CryptoProviderImpl;
use ldt_np_adv::V0_IDENTITY_TOKEN_LEN;

mod error_conditions;
mod happy_path;

#[test]
fn decrypt_with_wrong_key_seed_error() {
    let salt = ldt_np_adv::V0Salt::from([0x22; 2]);
    let metadata_key: [u8; V0_IDENTITY_TOKEN_LEN] = [0x33; V0_IDENTITY_TOKEN_LEN];
    let correct_key_seed = [0x11_u8; 32];

    let (adv_content, correct_cipher) =
        build_ciphertext_adv_contents(salt, &metadata_key, correct_key_seed);
    let eac = parse_ciphertext_adv_contents(&correct_cipher, &adv_content.as_slice()[1..]);

    // wrong key seed doesn't work (derives wrong ldt key, wrong hmac key)
    let wrong_key_seed_cipher = {
        let key_seed = [0x22_u8; 32];

        let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&key_seed);
        let metadata_key_hmac: [u8; 32] =
            hkdf.v0_identity_token_hmac_key().calculate_hmac::<CryptoProviderImpl>(&metadata_key);
        ldt_np_adv::build_np_adv_decrypter_from_key_seed(&hkdf, metadata_key_hmac)
    };

    assert_eq!(
        DecryptError::DecryptOrVerifyError,
        eac.try_decrypt(&wrong_key_seed_cipher).unwrap_err()
    );
}

#[test]
fn decrypt_with_wrong_hmac_key_error() {
    let salt = ldt_np_adv::V0Salt::from([0x22; 2]);
    let metadata_key: [u8; V0_IDENTITY_TOKEN_LEN] = [0x33; V0_IDENTITY_TOKEN_LEN];
    let correct_key_seed = [0x11_u8; 32];

    let (adv_content, correct_cipher_config) =
        build_ciphertext_adv_contents(salt, &metadata_key, correct_key_seed);
    let eac = parse_ciphertext_adv_contents(&correct_cipher_config, &adv_content.as_slice()[1..]);

    let wrong_hmac_key_cipher = {
        let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&[0x10_u8; 32]);
        let metadata_key_hmac: [u8; 32] =
            hkdf.v0_identity_token_hmac_key().calculate_hmac::<CryptoProviderImpl>(&metadata_key);

        ldt_np_adv::build_np_adv_decrypter_from_key_seed(&hkdf, metadata_key_hmac)
    };

    assert_eq!(
        DecryptError::DecryptOrVerifyError,
        eac.try_decrypt::<CryptoProviderImpl>(&wrong_hmac_key_cipher).unwrap_err()
    );
}

#[test]
fn decrypt_with_wrong_hmac_error() {
    let salt = ldt_np_adv::V0Salt::from([0x22; 2]);
    let metadata_key: [u8; V0_IDENTITY_TOKEN_LEN] = [0x33; V0_IDENTITY_TOKEN_LEN];
    let correct_key_seed = [0x11_u8; 32];

    let (adv_content, correct_cipher_config) =
        build_ciphertext_adv_contents(salt, &metadata_key, correct_key_seed);
    let eac = parse_ciphertext_adv_contents(&correct_cipher_config, &adv_content.as_slice()[1..]);

    let wrong_hmac_key_cipher = {
        let hkdf = np_hkdf::NpKeySeedHkdf::<CryptoProviderImpl>::new(&correct_key_seed);

        ldt_np_adv::build_np_adv_decrypter_from_key_seed(&hkdf, [0x77; 32])
    };

    assert_eq!(
        DecryptError::DecryptOrVerifyError,
        eac.try_decrypt(&wrong_hmac_key_cipher).unwrap_err()
    );
}

fn build_ciphertext_adv_contents<C: CryptoProvider>(
    salt: ldt_np_adv::V0Salt,
    metadata_key: &[u8; 14],
    correct_key_seed: [u8; 32],
) -> (
    ArrayView<u8, { BLE_4_ADV_SVC_MAX_CONTENT_LEN }>,
    ldt_np_adv::AuthenticatedNpLdtDecryptCipher<C>,
) {
    let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&correct_key_seed);

    let metadata_key_hmac: [u8; 32] =
        hkdf.v0_identity_token_hmac_key().calculate_hmac::<C>(metadata_key.as_slice());

    let correct_cipher = ldt_np_adv::build_np_adv_decrypter_from_key_seed(&hkdf, metadata_key_hmac);

    let broadcast_cred =
        V0BroadcastCredential::new(correct_key_seed, V0IdentityToken::from(*metadata_key));

    let mut builder = AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(salt, &broadcast_cred));
    builder.add_data_element(TxPowerDataElement::from(TxPower::try_from(3).unwrap())).unwrap();
    (builder.into_advertisement().unwrap(), correct_cipher)
}

fn parse_ciphertext_adv_contents<'b>(
    cipher: &ldt_np_adv::AuthenticatedNpLdtDecryptCipher<CryptoProviderImpl>,
    adv_content: &'b [u8],
) -> LdtAdvContents<'b> {
    let eac = match IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
        V0Encoding::Ldt,
        adv_content,
    )
    .unwrap()
    {
        IntermediateAdvContents::Unencrypted(_) => panic!(),
        // quick confirmation that we did get something
        IntermediateAdvContents::Ldt(eac) => eac,
    };

    // correct cipher works
    assert!(eac.try_decrypt(cipher).is_ok());

    eac
}

mod coverage_gaming {
    use crate::legacy::data_elements::actions::{ActionBits, ActionsDataElement};
    use crate::legacy::deserialize::{
        DecryptError, DecryptedAdvContents, DeserializedDataElement, StandardDeserializer,
    };
    use crate::legacy::Plaintext;
    use alloc::format;
    use array_view::ArrayView;
    use ldt_np_adv::{V0_IDENTITY_TOKEN_LEN, V0_SALT_LEN};

    #[test]
    fn decrypt_error_debug_clone() {
        let _ = format!("{:?}", DecryptError::DecryptOrVerifyError.clone());
    }

    #[test]
    fn deserialized_data_element_debug() {
        let _ = format!(
            "{:?}",
            DeserializedDataElement::<Plaintext>::Actions(ActionsDataElement::from(
                ActionBits::default()
            ))
        );
    }

    #[test]
    fn decrypted_adv_contents_debug_partial_eq() {
        let dac = DecryptedAdvContents::new(
            [0; V0_IDENTITY_TOKEN_LEN].into(),
            [0; V0_SALT_LEN].into(),
            ArrayView::try_from_slice(&[]).unwrap(),
        );
        let _ = format!("{:?}", dac);
        assert_eq!(dac, dac)
    }

    #[test]
    fn standard_deserializer_debug_eq_clone() {
        assert_eq!(StandardDeserializer, StandardDeserializer);
        let _ = format!("{:?}", StandardDeserializer.clone());
    }
}
