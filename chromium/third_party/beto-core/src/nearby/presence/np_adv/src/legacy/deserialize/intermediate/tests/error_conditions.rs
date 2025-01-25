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
    use crate::legacy::deserialize::intermediate::IntermediateAdvContents;
    use crate::legacy::deserialize::AdvDeserializeError;
    use crypto_provider_default::CryptoProviderImpl;

    #[test]
    fn parse_zero_len_error() {
        assert_eq!(
            AdvDeserializeError::NoDataElements,
            IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
                V0Encoding::Unencrypted,
                &[],
            )
            .unwrap_err()
        );
    }
}

mod ldt_encoder {
    use crate::header::V0Encoding;
    use crate::legacy::deserialize::intermediate::IntermediateAdvContents;
    use crate::legacy::deserialize::AdvDeserializeError;
    use alloc::vec;
    use crypto_provider_default::CryptoProviderImpl;
    use ldt_np_adv::{V0_IDENTITY_TOKEN_LEN, V0_SALT_LEN};

    #[test]
    fn all_below_min_ldt_len_error() {
        for len in 0..(V0_SALT_LEN + ldt_np_adv::VALID_INPUT_LEN.start) {
            assert_eq!(
                AdvDeserializeError::InvalidStructure,
                IntermediateAdvContents::deserialize::<CryptoProviderImpl>(
                    V0Encoding::Ldt,
                    &vec![0; len],
                )
                .unwrap_err()
            );
        }

        // 1 more byte is enough
        let data = &[0; ldt_np_adv::VALID_INPUT_LEN.start + V0_SALT_LEN];
        assert!(IntermediateAdvContents::deserialize::<CryptoProviderImpl>(V0Encoding::Ldt, data)
            .is_ok())
    }

    #[test]
    fn above_max_ldt_len_error() {
        let salt = [0x55; V0_SALT_LEN];
        // this is longer than can fit in a BLE 4.2 adv, but we're just testing LDT limits here
        let payload_len = 2 + crypto_provider::aes::BLOCK_SIZE;
        let data =
            &[&salt, [0x11; V0_IDENTITY_TOKEN_LEN].as_slice(), &vec![0xCC; payload_len]].concat();
        // 1 byte long
        assert_eq!(V0_SALT_LEN + ldt_np_adv::VALID_INPUT_LEN.end, data.len());

        assert_eq!(
            AdvDeserializeError::InvalidStructure,
            IntermediateAdvContents::deserialize::<CryptoProviderImpl>(V0Encoding::Ldt, data)
                .unwrap_err()
        );

        // 1 fewer byte is enough
        let data = &[&salt, [0x11; V0_IDENTITY_TOKEN_LEN].as_slice(), &vec![0xCC; payload_len - 1]]
            .concat();
        assert!(IntermediateAdvContents::deserialize::<CryptoProviderImpl>(V0Encoding::Ldt, data)
            .is_ok())
    }
}
