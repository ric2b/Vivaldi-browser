// Copyright 2024 Google LLC
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

//! Tests for intermediate sections representation and logic - this is the initial stage of parsing
//! before any ciphertext is actually decrypted.

extern crate std;
use std::{prelude::rust_2021::*, vec};

use super::*;
use crate::{
    extended::{
        deserialize::{
            data_element::DataElement,
            encrypted_section::{
                EncryptedSectionContents, MicEncryptedSection, SignatureEncryptedSection,
            },
            SectionMic,
        },
        salt::{ShortV1Salt, SHORT_SALT_LEN},
        serialize::{AdvBuilder, AdvertisementType},
        NP_V1_ADV_MAX_SECTION_COUNT, V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN,
        V1_ENCODING_ENCRYPTED_MIC_WITH_SHORT_SALT_AND_TOKEN,
        V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN, V1_ENCODING_UNENCRYPTED,
        V1_IDENTITY_TOKEN_LEN,
    },
    header::V1AdvHeader,
    NpVersionHeader,
};
use crypto_provider_default::CryptoProviderImpl;
use nom::error;
use np_hkdf::v1_salt::EXTENDED_SALT_LEN;
use rand::{rngs::StdRng, seq::SliceRandom, Rng, SeedableRng as _};

/// Section header length after the length byte for ext salt + token
const EXT_SALT_SECTION_HEADER_LEN: usize = 1 + EXTENDED_SALT_LEN + V1_IDENTITY_TOKEN_LEN + 1;

/// Section header length after the length byte for ext salt + token
const SHORT_SALT_SECTION_HEADER_LEN: usize = 1 + SHORT_SALT_LEN + V1_IDENTITY_TOKEN_LEN + 1;

mod happy_path {
    use super::*;
    use crate::{
        extended::{data_elements::TxPowerDataElement, serialize::UnencryptedSectionEncoder},
        shared_data::TxPower,
    };
    use np_hkdf::v1_salt::ExtendedV1Salt;

    #[test]
    fn parse_adv_ext_public_identity() {
        let mut adv_body = vec![];
        // public identity
        adv_body.push(V1_ENCODING_UNENCRYPTED);
        // section len
        adv_body.push(6 + 3);
        // de 1 byte header, type 5, len 5
        adv_body.extend_from_slice(&[0x55, 0x01, 0x02, 0x03, 0x04, 0x05]);
        // de 2 byte header, type 22, len 1
        adv_body.extend_from_slice(&[0x81, 0x16, 0x01]);

        let parsed_sections = parse_sections(V1AdvHeader::new(0x20), &adv_body).unwrap().into_vec();
        assert_eq!(
            vec![IntermediateSection::from(PlaintextSection::new(&adv_body[2..]))],
            parsed_sections,
        );
        let expected_des = [
            // 1 byte header, len 5
            DataElement::new(0_u8.into(), 5_u8.into(), &[0x01, 0x02, 0x03, 0x04, 0x05]),
            // 2 byte header, len 1
            DataElement::new(1_u8.into(), 22_u8.into(), &[0x01]),
        ];

        assert_eq!(
            &expected_des[..],
            &parsed_sections[0].as_plaintext().unwrap().collect_data_elements().unwrap()
        );
    }

    #[test]
    fn do_deserialize_max_number_of_public_sections() {
        let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
        for _ in 0..NP_V1_ADV_MAX_SECTION_COUNT {
            let mut section_builder =
                adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();
            section_builder
                .add_de(|_salt| TxPowerDataElement::from(TxPower::try_from(7).unwrap()))
                .unwrap();
            section_builder.add_to_advertisement::<CryptoProviderImpl>();
        }

        let adv = adv_builder.into_advertisement();
        let (remaining, header) = NpVersionHeader::parse(adv.as_slice()).unwrap();

        let v1_header = if let NpVersionHeader::V1(h) = header {
            h
        } else {
            panic!("incorrect header");
        };
        let sections = parse_sections(v1_header, remaining).unwrap();
        assert_eq!(NP_V1_ADV_MAX_SECTION_COUNT, sections.len());
    }

    #[test]
    fn max_number_encrypted_sections_mic() {
        let mut adv_body = vec![];
        for _ in 0..NP_V1_ADV_MAX_SECTION_COUNT {
            let _ = add_mic_short_salt_section_to_adv(&mut adv_body);
        }
        let adv_header = V1AdvHeader::new(0x20);
        assert!(parse_sections(adv_header, &adv_body).is_ok());
    }

    #[test]
    fn max_number_encrypted_sections_sig() {
        let mut adv_body = vec![];
        for _ in 0..NP_V1_ADV_MAX_SECTION_COUNT {
            let _ = add_sig_encrpyted_section(&mut adv_body, 5, &[0x55; EXTENDED_SALT_LEN]);
        }
        let adv_header = V1AdvHeader::new(0x20);
        assert!(parse_sections(adv_header, &adv_body).is_ok());
    }

    #[test]
    fn both_mic_and_sig_sections() {
        let mut adv_body = vec![];
        let short_salt = ShortV1Salt::from([0x11; SHORT_SALT_LEN]);
        let token_bytes = [0x55; V1_IDENTITY_TOKEN_LEN];
        let extended_salt = ExtendedV1Salt::from([0x55; EXTENDED_SALT_LEN]);
        let total_section_1_len = add_mic_short_salt_section_to_adv(&mut adv_body);
        let _ = add_sig_encrpyted_section(&mut adv_body, 5, extended_salt.bytes());

        let adv_header = V1AdvHeader::new(0x20);
        let section1 = &adv_body[0..total_section_1_len];
        let section2 = &adv_body[total_section_1_len..];
        let expected_sections = [
            IntermediateSection::from(MicEncryptedSection {
                contents: EncryptedSectionContents::new(
                    adv_header,
                    &section1[..1],
                    short_salt.into(),
                    token_bytes.into(),
                    (10 + SectionMic::CONTENTS_LEN).try_into().unwrap(),
                    &[0xFF; 10],
                ),
                mic: SectionMic::from([0x33; SectionMic::CONTENTS_LEN]),
            }),
            IntermediateSection::from(SignatureEncryptedSection {
                contents: EncryptedSectionContents::new(
                    adv_header,
                    &section2[..1],
                    extended_salt,
                    [0x33; V1_IDENTITY_TOKEN_LEN].into(),
                    5,
                    &[0xFF; 5],
                ),
            }),
        ];
        let parsed_sections = parse_sections(adv_header, &adv_body).unwrap();
        assert_eq!(parsed_sections.into_vec(), expected_sections);
    }

    #[test]
    fn parse_adv_sig_encrypted_sections() {
        // 3 sections
        let mut adv_body = vec![];
        let salt_bytes = [0x11; EXTENDED_SALT_LEN];

        let section_1_len = add_sig_encrpyted_section(&mut adv_body, 10, &salt_bytes);
        let section_2_len = add_sig_encrpyted_section(&mut adv_body, 11, &salt_bytes);
        let _ = add_sig_encrpyted_section(&mut adv_body, 12, &salt_bytes);

        let adv_header = V1AdvHeader::new(0x20);
        let section1 = &adv_body[..section_1_len];
        let section2 = &adv_body[section_1_len..][..section_2_len];
        let section3 = &adv_body[(section_1_len + section_2_len)..];
        let expected_sections = [
            SignatureEncryptedSection {
                contents: EncryptedSectionContents::new(
                    adv_header,
                    &section1[..1],
                    salt_bytes.into(),
                    [0x33; V1_IDENTITY_TOKEN_LEN].into(),
                    10,
                    &[0xFF; 10],
                ),
            },
            SignatureEncryptedSection {
                contents: EncryptedSectionContents::new(
                    adv_header,
                    &section2[..1],
                    salt_bytes.into(),
                    [0x33; V1_IDENTITY_TOKEN_LEN].into(),
                    11,
                    &[0xFF; 11],
                ),
            },
            SignatureEncryptedSection {
                contents: EncryptedSectionContents::new(
                    adv_header,
                    &section3[..1],
                    salt_bytes.into(),
                    [0x33; V1_IDENTITY_TOKEN_LEN].into(),
                    12,
                    &[0xFF; 12],
                ),
            },
        ];
        let parsed_sections = parse_sections(adv_header, &adv_body).unwrap();
        assert_eq!(parsed_sections.into_vec(), expected_sections.map(IntermediateSection::from));
    }

    #[test]
    fn parse_adv_ext_salt_mic_sections() {
        // 3 sections
        let mut adv_body = vec![];
        // section
        adv_body.push(V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN);
        let salt = ExtendedV1Salt::from([0x11; EXTENDED_SALT_LEN]);
        adv_body.extend_from_slice(salt.bytes().as_slice());
        let token_bytes_1 = [0x55; V1_IDENTITY_TOKEN_LEN];
        adv_body.extend_from_slice(&token_bytes_1);
        let section_1_len = EXT_SALT_SECTION_HEADER_LEN as u8 + 10 + SectionMic::CONTENTS_LEN as u8;
        adv_body.push(10 + SectionMic::CONTENTS_LEN as u8);
        // 10 bytes of 0xFF ciphertext
        adv_body.extend_from_slice(&[0xFF; 10]);
        // mic - 16x 0x33
        adv_body.extend_from_slice(&[0x33; SectionMic::CONTENTS_LEN]);

        // section
        adv_body.push(V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN);
        adv_body.extend_from_slice(salt.bytes().as_slice());
        let token_bytes_2 = [0x77; V1_IDENTITY_TOKEN_LEN];
        adv_body.extend_from_slice(&token_bytes_2);
        let section_2_len = EXT_SALT_SECTION_HEADER_LEN as u8 + 11 + SectionMic::CONTENTS_LEN as u8;
        adv_body.push(11 + SectionMic::CONTENTS_LEN as u8);
        // 11 bytes of 0xFF ciphertext
        adv_body.extend_from_slice(&[0xFF; 11]);
        // mic - 16x 0x66
        adv_body.extend_from_slice(&[0x66; SectionMic::CONTENTS_LEN]);

        // section
        adv_body.push(V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN);
        adv_body.extend_from_slice(salt.bytes().as_slice());
        let token_bytes_3 = [0xAA; V1_IDENTITY_TOKEN_LEN];
        adv_body.extend_from_slice(&token_bytes_3);
        let _ = EXT_SALT_SECTION_HEADER_LEN as u8 + 12 + SectionMic::CONTENTS_LEN as u8;
        adv_body.push(12 + SectionMic::CONTENTS_LEN as u8);
        // 12 bytes of 0xFF ciphertext
        adv_body.extend_from_slice(&[0xFF; 12]);
        // mic - 16x 0x99
        adv_body.extend_from_slice(&[0x99; SectionMic::CONTENTS_LEN]);

        let adv_header = V1AdvHeader::new(0x20);
        let section1 = &adv_body[..section_1_len as usize];
        let section2 = &adv_body[section_1_len as usize..][..section_2_len as usize];
        let section3 = &adv_body[section_1_len as usize + section_2_len as usize..];
        let expected_sections = [
            MicEncryptedSection {
                contents: EncryptedSectionContents::new(
                    adv_header,
                    &section1[..1],
                    salt.into(),
                    token_bytes_1.into(),
                    (10 + SectionMic::CONTENTS_LEN).try_into().unwrap(),
                    &[0xFF; 10],
                ),
                mic: SectionMic::from([0x33; SectionMic::CONTENTS_LEN]),
            },
            MicEncryptedSection {
                contents: EncryptedSectionContents::new(
                    adv_header,
                    &section2[..1],
                    salt.into(),
                    token_bytes_2.into(),
                    (11 + SectionMic::CONTENTS_LEN).try_into().unwrap(),
                    &[0xFF; 11],
                ),
                mic: SectionMic::from([0x66; SectionMic::CONTENTS_LEN]),
            },
            MicEncryptedSection {
                contents: EncryptedSectionContents::new(
                    adv_header,
                    &section3[..1],
                    salt.into(),
                    token_bytes_3.into(),
                    (12 + SectionMic::CONTENTS_LEN).try_into().unwrap(),
                    &[0xFF; 12],
                ),
                mic: SectionMic::from([0x99; SectionMic::CONTENTS_LEN]),
            },
        ];
        let parsed_sections = parse_sections(adv_header, &adv_body).unwrap();
        assert_eq!(parsed_sections.into_vec(), &expected_sections.map(IntermediateSection::from));
    }

    #[test]
    fn parse_adv_short_salt_mic() {
        // 3 sections
        let short_salt = ShortV1Salt::from([0x11; SHORT_SALT_LEN]);
        let token_bytes = [0x55; V1_IDENTITY_TOKEN_LEN];
        let mut adv_body = vec![];
        let section_1_len = add_mic_short_salt_section_to_adv(&mut adv_body);
        let _ = add_mic_short_salt_section_to_adv(&mut adv_body);
        let _ = add_mic_short_salt_section_to_adv(&mut adv_body);

        let adv_header = V1AdvHeader::new(0x20);
        let section1 = &adv_body[..(1 + section_1_len)];
        let mut expected_sections = [None, None, None];
        for section in &mut expected_sections {
            *section = Some(MicEncryptedSection {
                contents: EncryptedSectionContents::new(
                    adv_header,
                    &section1[..1],
                    short_salt.into(),
                    token_bytes.into(),
                    (10 + SectionMic::CONTENTS_LEN).try_into().unwrap(),
                    &[0xFF; 10],
                ),
                mic: SectionMic::from([0x33; SectionMic::CONTENTS_LEN]),
            })
        }
        let expected: Vec<IntermediateSection> =
            expected_sections.into_iter().map(|x| IntermediateSection::from(x.unwrap())).collect();
        let parsed_sections = parse_sections(adv_header, &adv_body).unwrap();
        assert_eq!(parsed_sections.into_vec(), expected);
    }
}

mod error_condition {
    use super::*;
    #[test]
    fn parse_adv_too_many_unencrypted() {
        // 2 sections
        let mut adv_body = vec![];

        for _ in 0..=NP_V1_ADV_MAX_SECTION_COUNT {
            // section 1..=MAX+1 - plaintext - 11 bytes
            adv_body.push(V1_ENCODING_UNENCRYPTED);
            adv_body.push(6 + 3); // section len
                                  // de 1 byte header, type 5, len 5
            adv_body.extend_from_slice(&[0x55, 0x01, 0x02, 0x03, 0x04, 0x05]);
            // de 2 byte header, type 6, len 1
            adv_body.extend_from_slice(&[0x81, 0x06, 0x01]);
        }

        let adv_header = V1AdvHeader::new(0x20);

        assert_eq!(
            nom::Err::Error(error::Error {
                input: &adv_body[(NP_V1_ADV_MAX_SECTION_COUNT * 11)..],
                code: error::ErrorKind::Eof
            }),
            parse_sections(adv_header, &adv_body).unwrap_err()
        );
    }

    #[test]
    fn parse_adv_too_many_encrypted() {
        // 3 sections
        let mut adv_body = vec![];
        for _ in 0..NP_V1_ADV_MAX_SECTION_COUNT + 1 {
            let _ = add_mic_short_salt_section_to_adv(&mut adv_body);
        }
        let adv_header = V1AdvHeader::new(0x20);
        let _ = parse_sections(adv_header, &adv_body).expect_err("Expected an error");
    }

    #[test]
    fn do_deserialize_zero_section_header() {
        let mut adv: tinyvec::ArrayVec<[u8; 254]> = tinyvec::ArrayVec::new();
        adv.push(0x20); // V1 Advertisement
        adv.push(0x00); // Section header of 0
        let (remaining, header) = NpVersionHeader::parse(adv.as_slice()).unwrap();
        let v1_header = if let NpVersionHeader::V1(h) = header {
            h
        } else {
            panic!("incorrect header");
        };
        let _ = parse_sections(v1_header, remaining).expect_err("Expected an error");
    }

    #[test]
    fn invalid_public_section() {
        let mut rng = StdRng::from_entropy();
        for _ in 0..100 {
            let mut adv_body = vec![];
            // Add section length
            let remove_section_len = rng.gen_bool(0.5);
            // Add public identity
            let add_public_identity = rng.gen_bool(0.5);
            // Add DEs
            let add_des = rng.gen_bool(0.5);
            // Shuffle adv
            let shuffle = rng.gen_bool(0.5);

            adv_body.push(0);
            if add_public_identity {
                adv_body[0] += 1;
                adv_body.push(0x03);
            }
            if add_des {
                adv_body[0] += 1;
                adv_body.extend_from_slice(&[0x81]);
            }
            if remove_section_len {
                let _ = adv_body.remove(0);
            }

            let ordered_adv = adv_body.clone();

            if shuffle {
                adv_body.shuffle(&mut rng);
            }
            // A V1 public section is invalid if
            // * section length is missing
            // * the section is empty
            // * the section identity is missing
            // * the identity does not follow the section length
            // * the section length is 0
            if remove_section_len || !add_public_identity || (ordered_adv != adv_body) {
                let _ = parse_sections(V1AdvHeader::new(0x20), &adv_body)
                    .expect_err("Expected to fail because of missing section length or identity");
            }
        }
    }

    #[test]
    fn parse_empty_section_as_encrypted() {
        // empty section - should return an EOF error
        let input = [];
        assert_eq!(
            nom::Err::Error(error::Error {
                // attempted to read section contents
                input: input.as_slice(),
                code: error::ErrorKind::Eof,
            }),
            IntermediateSection::parser_encrypted_with_header(V1AdvHeader::new(0x20))(&input)
                .unwrap_err()
        );
    }

    #[test]
    fn parse_unencrypted_as_encrypted() {
        let mut input = vec![];
        // public identity
        input.push(V1_ENCODING_UNENCRYPTED);
        // section len
        input.push(6 + 3);
        // de 1 byte header, type 5, len 5
        input.extend_from_slice(&[0x55, 0x01, 0x02, 0x03, 0x04, 0x05]);
        // de 2 byte header, type 22, len 1
        input.extend_from_slice(&[0x81, 0x16, 0x01]);
        assert_eq!(
            nom::Err::Error(error::Error {
                // attempted to read section contents
                input: input.as_slice(),
                code: error::ErrorKind::MapOpt,
            }),
            IntermediateSection::parser_encrypted_with_header(V1AdvHeader::new(0x20))(&input)
                .unwrap_err()
        );
    }

    #[test]
    fn parse_section_length_overrun() {
        // section of length 0xF0 - legal but way longer than 2
        let input = [V1_ENCODING_UNENCRYPTED, 0xF0, 0x10, 0x11];
        assert_eq!(
            nom::Err::Error(error::Error {
                // attempted to read section contents after parsing header
                input: &input.as_slice()[2..],
                code: error::ErrorKind::Eof,
            }),
            SectionContents::parse(&input).unwrap_err()
        );
    }

    #[test]
    fn do_deserialize_empty_section() {
        let adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
        let adv = adv_builder.into_advertisement();
        let (remaining, header) = NpVersionHeader::parse(adv.as_slice()).unwrap();
        let v1_header = if let NpVersionHeader::V1(h) = header {
            h
        } else {
            panic!("incorrect header");
        };
        let _ = parse_sections(v1_header, remaining).expect_err("Expected an error");
    }

    #[test]
    fn parse_adv_sig_encrypted_section_with_short_salt() {
        // 3 sections
        let mut adv_body = vec![];
        let salt_bytes = [0x11; SHORT_SALT_LEN];
        let section_len = EXT_SALT_SECTION_HEADER_LEN as u8 + 10;
        adv_body.push(section_len);
        adv_body.push(V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN);
        adv_body.extend_from_slice(&salt_bytes);
        adv_body.extend_from_slice(&[0x33; V1_IDENTITY_TOKEN_LEN]);
        adv_body.extend_from_slice(&[0x22; 10]);
        let adv_header = V1AdvHeader::new(0x20);
        let parsed_sections = parse_sections(adv_header, &adv_body);
        assert!(parsed_sections.is_err());
    }

    // specify extended salt in the header but adv actual contains short salt
    #[test]
    fn parse_adv_mic_encrypted_wrong_salt_for_extended_header() {
        // 3 sections
        let mut adv = vec![];
        let section_len = SHORT_SALT_SECTION_HEADER_LEN as u8 + 10 + SectionMic::CONTENTS_LEN as u8;
        adv.push(section_len);
        adv.push(V1_ENCODING_ENCRYPTED_MIC_WITH_EXTENDED_SALT_AND_TOKEN);
        let salt = ShortV1Salt::from([0x11; SHORT_SALT_LEN]);
        adv.extend_from_slice(salt.bytes().as_slice());
        let token_bytes_1 = [0x55; V1_IDENTITY_TOKEN_LEN];
        adv.extend_from_slice(&token_bytes_1);
        // 10 bytes of 0xFF ciphertext
        adv.extend_from_slice(&[0xFF; 10]);
        // mic - 16x 0x33
        adv.extend_from_slice(&[0x33; SectionMic::CONTENTS_LEN]);

        let adv_header = V1AdvHeader::new(0x20);
        let parsed_sections = parse_sections(adv_header, &adv);
        assert!(parsed_sections.is_err());
    }

    // specify extended salt in the header but adv actual contains short salt
    #[test]
    fn encrypted_then_unencrpyted_fails_to_parse() {
        // 3 sections
        let mut adv = vec![];
        let section_len = SHORT_SALT_SECTION_HEADER_LEN as u8 + 10 + SectionMic::CONTENTS_LEN as u8;
        adv.push(section_len);
        adv.push(V1_ENCODING_ENCRYPTED_MIC_WITH_SHORT_SALT_AND_TOKEN);
        let salt = ShortV1Salt::from([0x11; SHORT_SALT_LEN]);
        adv.extend_from_slice(salt.bytes().as_slice());
        let token_bytes_1 = [0x55; V1_IDENTITY_TOKEN_LEN];
        adv.extend_from_slice(&token_bytes_1);
        // 10 bytes of 0xFF ciphertext
        adv.extend_from_slice(&[0xFF; 10]);
        // mic - 16x 0x33
        adv.extend_from_slice(&[0x33; SectionMic::CONTENTS_LEN]);

        // Now add public section
        // section len
        adv.push(1 + 6 + 3);
        // public identity
        adv.push(V1_ENCODING_UNENCRYPTED);
        // de 1 byte header, type 5, len 5
        adv.extend_from_slice(&[0x55, 0x01, 0x02, 0x03, 0x04, 0x05]);
        // de 2 byte header, type 6, len 1
        adv.extend_from_slice(&[0x81, 0x06, 0x01]);

        let adv_header = V1AdvHeader::new(0x20);
        let parsed_sections = parse_sections(adv_header, &adv);
        assert!(parsed_sections.is_err());
    }

    // specify extended salt in the header but adv actual contains short salt
    #[test]
    fn unencrypted_then_encrpyted_fails_to_parse() {
        let mut adv = vec![];

        // Public section
        // section len
        adv.push(1 + 6 + 3);
        // public identity
        adv.push(V1_ENCODING_UNENCRYPTED);
        // de 1 byte header, type 5, len 5
        adv.extend_from_slice(&[0x55, 0x01, 0x02, 0x03, 0x04, 0x05]);
        // de 2 byte header, type 6, len 1
        adv.extend_from_slice(&[0x81, 0x06, 0x01]);

        // mic encrypted section
        let section_len = SHORT_SALT_SECTION_HEADER_LEN as u8 + 10 + SectionMic::CONTENTS_LEN as u8;
        adv.push(section_len);
        adv.push(V1_ENCODING_ENCRYPTED_MIC_WITH_SHORT_SALT_AND_TOKEN);
        let salt = ShortV1Salt::from([0x11; SHORT_SALT_LEN]);
        adv.extend_from_slice(salt.bytes().as_slice());
        let token_bytes_1 = [0x55; V1_IDENTITY_TOKEN_LEN];
        adv.extend_from_slice(&token_bytes_1);
        // 10 bytes of 0xFF ciphertext
        adv.extend_from_slice(&[0xFF; 10]);
        // mic - 16x 0x33
        adv.extend_from_slice(&[0x33; SectionMic::CONTENTS_LEN]);

        let adv_header = V1AdvHeader::new(0x20);
        let parsed_sections = parse_sections(adv_header, &adv);
        assert!(parsed_sections.is_err());
    }
}

mod coverage_gaming {
    use super::*;
    use alloc::format;

    #[test]
    fn section_contents() {
        let sc = SectionContents {
            format_bytes: &[],
            header: SectionHeader::Unencrypted,
            contents: &[],
            contents_len: 0,
        };
        let _ = format!("{:?}", sc);
        assert_eq!(sc, sc);
    }

    #[test]
    fn intermediate_section() {
        let is = IntermediateSection::Plaintext(PlaintextSection::new(&[]));
        let _ = format!("{:?}", is);
    }
    #[test]
    fn ciphertext_section() {
        let ms = MicEncryptedSection {
            contents: EncryptedSectionContents::new(
                V1AdvHeader::new(1),
                &[],
                ShortV1Salt::from([0x00; 2]).into(),
                CiphertextExtendedIdentityToken([0x00; 16]),
                0,
                &[],
            ),
            mic: [0x00; 16].into(),
        };
        let cs = CiphertextSection::MicEncrypted(ms);
        let _ = format!("{:?}", cs);
    }
}

pub(crate) trait IntermediateSectionExt<'adv> {
    /// Returns `Some` if `self` is `Plaintext`
    fn as_plaintext(&self) -> Option<&PlaintextSection<'adv>>;
    /// Returns `Some` if `self` is `Ciphertext`
    fn as_ciphertext(&self) -> Option<&CiphertextSection<'adv>>;
}

impl<'adv> IntermediateSectionExt<'adv> for IntermediateSection<'adv> {
    fn as_plaintext(&self) -> Option<&PlaintextSection<'adv>> {
        match self {
            IntermediateSection::Plaintext(s) => Some(s),
            IntermediateSection::Ciphertext(_) => None,
        }
    }

    fn as_ciphertext(&self) -> Option<&CiphertextSection<'adv>> {
        match self {
            IntermediateSection::Plaintext(_) => None,
            IntermediateSection::Ciphertext(s) => Some(s),
        }
    }
}

// returns the number of bytes appended to adv
fn add_mic_short_salt_section_to_adv(adv: &mut Vec<u8>) -> usize {
    // section
    adv.push(V1_ENCODING_ENCRYPTED_MIC_WITH_SHORT_SALT_AND_TOKEN);
    let salt = ShortV1Salt::from([0x11; SHORT_SALT_LEN]);
    adv.extend_from_slice(salt.bytes().as_slice());
    let token_bytes_1 = [0x55; V1_IDENTITY_TOKEN_LEN];
    adv.extend_from_slice(&token_bytes_1);
    let section_len = 10 + SectionMic::CONTENTS_LEN as u8;
    adv.push(section_len);
    // 10 bytes of 0xFF ciphertext
    adv.extend_from_slice(&[0xFF; 10]);
    // mic - 16x 0x33
    adv.extend_from_slice(&[0x33; SectionMic::CONTENTS_LEN]);
    1 + SHORT_SALT_LEN + V1_IDENTITY_TOKEN_LEN + 1 + 10 + SectionMic::CONTENTS_LEN
}

// returns the total number of bytes appended to adv
fn add_sig_encrpyted_section(
    adv_body: &mut Vec<u8>,
    len: u8,
    salt_bytes: &[u8; EXTENDED_SALT_LEN],
) -> usize {
    adv_body.push(V1_ENCODING_ENCRYPTED_SIGNATURE_WITH_EXTENDED_SALT_AND_TOKEN);
    adv_body.extend_from_slice(salt_bytes);
    adv_body.extend_from_slice(&[0x33; V1_IDENTITY_TOKEN_LEN]);
    adv_body.push(len);
    // len bytes of 0xFF ciphertext -- in a real adv this would include the
    // signature, but for the purposes of this parser, it's all just ciphertext
    for _ in 0..len {
        adv_body.push(0xFF);
    }
    1 + EXTENDED_SALT_LEN + V1_IDENTITY_TOKEN_LEN + 1 + len as usize
}

// for convenient .into() in expected test data
impl<'a> From<SignatureEncryptedSection<'a>> for IntermediateSection<'a> {
    fn from(s: SignatureEncryptedSection<'a>) -> Self {
        IntermediateSection::Ciphertext(CiphertextSection::SignatureEncrypted(s))
    }
}

impl<'a> From<MicEncryptedSection<'a>> for IntermediateSection<'a> {
    fn from(s: MicEncryptedSection<'a>) -> Self {
        IntermediateSection::Ciphertext(CiphertextSection::MicEncrypted(s))
    }
}

impl<'a> From<PlaintextSection<'a>> for IntermediateSection<'a> {
    fn from(s: PlaintextSection<'a>) -> Self {
        IntermediateSection::Plaintext(s)
    }
}
