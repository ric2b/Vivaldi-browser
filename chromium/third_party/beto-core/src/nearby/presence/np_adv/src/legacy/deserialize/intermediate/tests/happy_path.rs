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

mod unencrypted_encoder {
    use crate::header::V0Encoding;
    use crate::legacy::data_elements::de_type::{DeEncodedLength, DeTypeCode, MAX_DE_ENCODED_LEN};
    use crate::legacy::deserialize::intermediate::IntermediateAdvContents;
    use crate::legacy::serialize::encode_de_header;
    use crate::legacy::NP_MAX_DE_CONTENT_LEN;
    use crypto_provider_default::CryptoProviderImpl;

    #[test]
    fn parse_min_len() {
        let header = encode_de_header(DeTypeCode::try_from(14).unwrap(), DeEncodedLength::from(0));
        assert_eq!(
            &[header],
            IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
                V0Encoding::Unencrypted,
                &[header],
            )
            .unwrap()
            .as_unencrypted()
            .unwrap()
            .data
        )
    }

    #[test]
    fn parse_max_len() {
        let header = encode_de_header(
            DeTypeCode::try_from(15).unwrap(),
            DeEncodedLength::try_from(MAX_DE_ENCODED_LEN).unwrap(),
        );
        let data = &[&[header], [0x22; NP_MAX_DE_CONTENT_LEN].as_slice()].concat();
        assert_eq!(
            data,
            IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
                V0Encoding::Unencrypted,
                data,
            )
            .unwrap()
            .as_unencrypted()
            .unwrap()
            .data
        )
    }
}

mod ldt_encoder {
    use crate::header::V0Encoding;
    use crate::legacy::deserialize::intermediate::{IntermediateAdvContents, LdtAdvContents};
    use crate::legacy::NP_MAX_ADV_CONTENT_LEN;
    use crypto_provider_default::CryptoProviderImpl;
    use ldt::LdtCipher;
    use ldt_np_adv::{salt_padder, V0_IDENTITY_TOKEN_LEN, V0_SALT_LEN};

    #[test]
    fn parse_min_len() {
        // not going to bother to make it look like encrypted DEs since this layer doesn't care
        let salt = [0x55; V0_SALT_LEN];
        let data = &[&salt, [0x11; V0_IDENTITY_TOKEN_LEN].as_slice(), &[0xCC; 2]].concat();
        assert_eq!(
            V0_SALT_LEN
                + ldt_np_adv::NpLdtEncryptCipher::<CryptoProviderImpl>::VALID_INPUT_LEN.start,
            data.len()
        );

        assert_ldt_contents(salt, data);
    }

    #[test]
    fn parse_max_len() {
        let salt = [0x55; V0_SALT_LEN];
        let data = &[&salt, [0x11; V0_IDENTITY_TOKEN_LEN].as_slice(), &[0xCC; 7]].concat();
        assert_eq!(NP_MAX_ADV_CONTENT_LEN, data.len());

        assert_ldt_contents(salt, data);
    }

    fn assert_ldt_contents(salt: [u8; V0_SALT_LEN], data: &[u8]) {
        assert_eq!(
            &LdtAdvContents {
                salt_padder: salt_padder::<CryptoProviderImpl>(salt.into()),
                salt: salt.into(),
                ciphertext: &data[V0_SALT_LEN..],
            },
            IntermediateAdvContents::deserialize::<CryptoProviderImpl>(V0Encoding::Ldt, data)
                .unwrap()
                .as_ldt()
                .unwrap()
        );
    }
}
