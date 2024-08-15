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

use super::*;
use crate::deserialization_arena;
use crate::deserialization_arena::DeserializationArena;
use crate::extended::serialize::AdvertisementType;
use crate::extended::NP_V1_ADV_MAX_PUBLIC_SECTION_COUNT;
use crate::{
    credential::{
        source::{DiscoveryCredentialSource, SliceCredentialSource},
        v1::{SignedBroadcastCryptoMaterial, SimpleSignedBroadcastCryptoMaterial, V1},
        DiscoveryCryptoMaterial, EmptyMatchedCredential, MatchableCredential,
        MetadataMatchedCredential, SimpleBroadcastCryptoMaterial,
    },
    extended::{
        data_elements::GenericDataElement,
        deserialize::{test_stubs::IntermediateSectionExt, DataElement},
        serialize::{
            self, AdvBuilder, MicEncryptedSectionEncoder, PublicSectionEncoder, SectionBuilder,
            SignedEncryptedSectionEncoder, WriteDataElement,
        },
        MAX_DE_LEN,
    },
    parse_adv_header, AdvHeader, WithMatchedCredential,
};
use core::borrow::Borrow;
use core::convert::Into;
use crypto_provider::{CryptoProvider, CryptoRng};
use crypto_provider_default::CryptoProviderImpl;
use rand::{seq::SliceRandom as _, Rng as _, SeedableRng as _};
use std::prelude::rust_2021::*;
use std::vec;

type KeyPair = np_ed25519::KeyPair<CryptoProviderImpl>;

#[test]
fn deserialize_public_identity_section() {
    do_deserialize_section_unencrypted::<PublicSectionEncoder>(
        PublicSectionEncoder::default(),
        PlaintextIdentityMode::Public,
        1,
    );
}

// due to lifetime issues, this is somewhat challenging to share with the 90% identical signature
// test, but if someone feels like putting in the tinkering to make it happen, please do
#[test]
fn deserialize_mic_encrypted_rand_identities_finds_correct_one() {
    let mut rng = rand::rngs::StdRng::from_entropy();
    let mut crypto_rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();
    for _ in 0..100 {
        let identities = (0..100).map(|_| (rng.gen(), KeyPair::generate())).collect::<Vec<_>>();

        let chosen_index = rng.gen_range(0..identities.len());
        let (chosen_key_seed, _chosen_key_pair) = &identities[chosen_index];

        // share a metadata key to emphasize that we're _only_ using the identity to
        // differentiate
        let metadata_key: [u8; 16] = rng.gen();
        let metadata_key = MetadataKey(metadata_key);

        let creds = identities
            .iter()
            .map(|(key_seed, key_pair)| {
                SimpleSignedBroadcastCryptoMaterial::new(
                    *key_seed,
                    metadata_key,
                    key_pair.private_key(),
                )
            })
            .enumerate()
            .map(|(index, broadcast_cm)| {
                let match_data = MetadataMatchedCredential::<Vec<u8>>::encrypt_from_plaintext::<
                    _,
                    _,
                    CryptoProviderImpl,
                >(&broadcast_cm, &[index as u8]);

                let discovery_credential =
                    broadcast_cm.derive_v1_discovery_credential::<CryptoProviderImpl>();

                MatchableCredential { discovery_credential, match_data }
            })
            .collect::<Vec<_>>();

        let cred_source = SliceCredentialSource::new(&creds);

        let identity_type =
            *EncryptedIdentityDataElementType::iter().collect::<Vec<_>>().choose(&mut rng).unwrap();

        let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

        let broadcast_cm = SimpleBroadcastCryptoMaterial::<V1>::new(*chosen_key_seed, metadata_key);

        let mut section_builder = adv_builder
            .section_builder(MicEncryptedSectionEncoder::<CryptoProviderImpl>::new_random_salt(
                &mut crypto_rng,
                identity_type,
                &broadcast_cm,
            ))
            .unwrap();

        let mut expected_de_data = vec![];
        let (expected_des, orig_des) =
            fill_section_random_des(&mut rng, &mut expected_de_data, &mut section_builder, 2);

        section_builder.add_to_advertisement();

        let adv = adv_builder.into_advertisement();

        let (remaining, header) = parse_adv_header(adv.as_slice()).unwrap();

        let v1_header = if let AdvHeader::V1(h) = header {
            h
        } else {
            panic!("incorrect header");
        };

        let sections = parse_sections(v1_header, remaining).unwrap();
        assert_eq!(1, sections.len());

        let arena = deserialization_arena!();

        let section = sections[0].as_ciphertext().unwrap();
        let matched_section =
            try_deserialize_all_creds::<_, CryptoProviderImpl>(arena, section, &cred_source)
                .unwrap()
                .unwrap();

        // Verify that the decrypted metadata contains the chosen index
        let decrypted_metadata = matched_section.decrypt_metadata::<CryptoProviderImpl>().unwrap();
        assert_eq!([chosen_index as u8].as_slice(), &decrypted_metadata);

        // Verify that the section contents passed through unaltered
        let section = matched_section.contents();
        assert_eq!(section.identity_type(), identity_type);
        assert_eq!(section.verification_mode(), VerificationMode::Mic);
        assert_eq!(section.metadata_key(), metadata_key);
        assert_eq!(
            section.contents.section_header,
            (19 + 2 + 16 + total_de_len(&orig_des) + 16) as u8
        );
        let data_elements = section.collect_data_elements().unwrap();
        assert_eq!(data_elements, expected_des);
        assert_eq!(
            data_elements
                .iter()
                .map(|de| GenericDataElement::try_from(de.de_type(), de.contents()).unwrap())
                .collect::<Vec<_>>(),
            orig_des
        );
    }
}

#[test]
fn deserialize_signature_encrypted_rand_identities_finds_correct_one() {
    let mut rng = rand::rngs::StdRng::from_entropy();
    let mut crypto_rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();
    for _ in 0..100 {
        let identities = (0..100).map(|_| (rng.gen(), KeyPair::generate())).collect::<Vec<_>>();

        let chosen_index = rng.gen_range(0..identities.len());
        let (chosen_key_seed, chosen_key_pair) = &identities[chosen_index];

        // share a metadata key to emphasize that we're _only_ using the identity to
        // differentiate
        let metadata_key: [u8; 16] = rng.gen();
        let metadata_key = MetadataKey(metadata_key);

        let creds = identities
            .iter()
            .map(|(key_seed, key_pair)| {
                SimpleSignedBroadcastCryptoMaterial::new(
                    *key_seed,
                    metadata_key,
                    key_pair.private_key(),
                )
            })
            .enumerate()
            .map(|(index, broadcast_cm)| {
                let match_data = MetadataMatchedCredential::<Vec<u8>>::encrypt_from_plaintext::<
                    _,
                    _,
                    CryptoProviderImpl,
                >(&broadcast_cm, &[index as u8]);

                let discovery_credential =
                    broadcast_cm.derive_v1_discovery_credential::<CryptoProviderImpl>();
                MatchableCredential { discovery_credential, match_data }
            })
            .collect::<Vec<_>>();

        let cred_source = SliceCredentialSource::new(&creds);

        let identity_type =
            *EncryptedIdentityDataElementType::iter().collect::<Vec<_>>().choose(&mut rng).unwrap();

        let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

        let broadcast_cm = SimpleSignedBroadcastCryptoMaterial::new(
            *chosen_key_seed,
            metadata_key,
            chosen_key_pair.private_key(),
        );

        let mut section_builder = adv_builder
            .section_builder(SignedEncryptedSectionEncoder::<CryptoProviderImpl>::new_random_salt(
                &mut crypto_rng,
                identity_type,
                &broadcast_cm,
            ))
            .unwrap();

        let mut expected_de_data = vec![];
        let (expected_des, orig_des) =
            fill_section_random_des(&mut rng, &mut expected_de_data, &mut section_builder, 2);

        section_builder.add_to_advertisement();

        let adv = adv_builder.into_advertisement();

        let (remaining, header) = parse_adv_header(adv.as_slice()).unwrap();

        let v1_header = if let AdvHeader::V1(h) = header {
            h
        } else {
            panic!("incorrect header");
        };

        let arena = deserialization_arena!();

        let sections = parse_sections(v1_header, remaining).unwrap();
        assert_eq!(1, sections.len());

        let section = sections[0].as_ciphertext().unwrap();
        let matched_section =
            try_deserialize_all_creds::<_, CryptoProviderImpl>(arena, section, &cred_source)
                .unwrap()
                .unwrap();

        // Verify that the decrypted metadata contains the chosen index
        let decrypted_metadata = matched_section.decrypt_metadata::<CryptoProviderImpl>().unwrap();
        assert_eq!([chosen_index as u8].as_slice(), &decrypted_metadata);

        // Verify that the section contents passed through unaltered
        let section = matched_section.contents();
        assert_eq!(section.identity_type(), identity_type);
        assert_eq!(section.verification_mode(), VerificationMode::Signature);
        assert_eq!(section.metadata_key(), metadata_key);
        assert_eq!(
            section.contents.section_header,
            (19 + 2 + 16 + 64 + total_de_len(&orig_des)) as u8
        );
        let data_elements = section.collect_data_elements().unwrap();
        assert_eq!(data_elements, expected_des);
        assert_eq!(
            data_elements
                .iter()
                .map(|de| GenericDataElement::try_from(de.de_type(), de.contents()).unwrap())
                .collect::<Vec<_>>(),
            orig_des
        );
    }
}

#[test]
fn deserialize_encrypted_no_matching_identities_finds_nothing() {
    let mut rng = rand::rngs::StdRng::from_entropy();
    let mut crypto_rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();
    for _ in 0..100 {
        let signed = rng.gen();
        let mut identities = (0..100).map(|_| (rng.gen(), KeyPair::generate())).collect::<Vec<_>>();

        let chosen_index = rng.gen_range(0..identities.len());
        // remove so they won't be found later
        let (chosen_key_seed, chosen_key_pair) = identities.remove(chosen_index);

        // share a metadata key to emphasize that we're _only_ using the identity to
        // differentiate
        let metadata_key: [u8; 16] = rng.gen();
        let metadata_key = MetadataKey(metadata_key);

        let credentials = identities
            .iter()
            .map(|(key_seed, key_pair)| {
                SimpleSignedBroadcastCryptoMaterial::new(
                    *key_seed,
                    metadata_key,
                    key_pair.private_key(),
                )
                .derive_v1_discovery_credential::<CryptoProviderImpl>()
            })
            .map(|discovery_credential| MatchableCredential {
                discovery_credential,
                match_data: EmptyMatchedCredential,
            })
            .collect::<Vec<_>>();

        let cred_source = SliceCredentialSource::new(&credentials);

        let identity_type =
            *EncryptedIdentityDataElementType::iter().collect::<Vec<_>>().choose(&mut rng).unwrap();

        let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

        let broadcast_cm = SimpleSignedBroadcastCryptoMaterial::new(
            chosen_key_seed,
            metadata_key,
            chosen_key_pair.private_key(),
        );

        // awkward split because SectionEncoder isn't object-safe, so we can't just have a
        // Box<dyn SectionEncoder> and use that in one code path
        if signed {
            let identity = SignedEncryptedSectionEncoder::<CryptoProviderImpl>::new_random_salt(
                &mut crypto_rng,
                identity_type,
                &broadcast_cm,
            );
            let mut section_builder = adv_builder.section_builder(identity).unwrap();
            let mut expected_de_data = vec![];
            let _ =
                fill_section_random_des(&mut rng, &mut expected_de_data, &mut section_builder, 2);
            section_builder.add_to_advertisement();
        } else {
            let identity = MicEncryptedSectionEncoder::<CryptoProviderImpl>::new_random_salt(
                &mut crypto_rng,
                identity_type,
                &broadcast_cm,
            );
            let mut section_builder = adv_builder.section_builder(identity).unwrap();
            let mut expected_de_data = vec![];
            let _ =
                fill_section_random_des(&mut rng, &mut expected_de_data, &mut section_builder, 2);
            section_builder.add_to_advertisement();
        };

        let adv = adv_builder.into_advertisement();
        let (remaining, header) = parse_adv_header(adv.as_slice()).unwrap();
        let v1_header = if let AdvHeader::V1(h) = header {
            h
        } else {
            panic!("incorrect header");
        };

        let sections = parse_sections(v1_header, remaining).unwrap();
        assert_eq!(1, sections.len());

        assert!(try_deserialize_all_creds::<_, CryptoProviderImpl>(
            deserialization_arena!(),
            sections[0].as_ciphertext().unwrap(),
            &cred_source,
        )
        .unwrap()
        .is_none());
    }
}

#[test]
fn section_des_expose_correct_data() {
    // 2 sections, 3 DEs each
    let mut de_data = vec![];
    // de 1 byte header, type 5, len 5
    de_data.extend_from_slice(&[0x55, 0x01, 0x02, 0x03, 0x04, 0x05]);
    // de 2 byte header, type 16, len 1
    de_data.extend_from_slice(&[0x81, 0x10, 0x01]);

    let section = SectionContents {
        section_header: 99,
        de_region_excl_identity: &de_data,
        data_element_start_offset: 2,
    };

    // extract out the parts of the DE we care about
    let des = section.iter_data_elements().collect::<Result<Vec<_>, _>>().unwrap();
    assert_eq!(
        vec![
            DataElement {
                offset: 2.into(),
                de_type: 5_u32.into(),
                contents: &[0x01, 0x02, 0x03, 0x04, 0x05]
            },
            DataElement { offset: 3.into(), de_type: 16_u32.into(), contents: &[0x01] },
        ],
        des
    );
}

#[test]
fn do_deserialize_zero_section_header() {
    let mut adv: tinyvec::ArrayVec<[u8; 254]> = tinyvec::ArrayVec::new();
    adv.push(0x20); // V1 Advertisement
    adv.push(0x00); // Section header of 0
    let (remaining, header) = parse_adv_header(adv.as_slice()).unwrap();
    let v1_header = if let AdvHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };
    let _ = parse_sections(v1_header, remaining).expect_err("Expected an error");
}

#[test]
fn do_deserialize_empty_section() {
    let adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let adv = adv_builder.into_advertisement();
    let (remaining, header) = parse_adv_header(adv.as_slice()).unwrap();
    let v1_header = if let AdvHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };
    let _ = parse_sections(v1_header, remaining).expect_err("Expected an error");
}

#[test]
fn do_deserialize_max_number_of_public_sections() {
    let adv_builder = build_dummy_advertisement_sections(NP_V1_ADV_MAX_PUBLIC_SECTION_COUNT);
    let adv = adv_builder.into_advertisement();
    let (remaining, header) = parse_adv_header(adv.as_slice()).unwrap();

    let v1_header = if let AdvHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };
    let sections = parse_sections(v1_header, remaining).unwrap();
    assert_eq!(NP_V1_ADV_MAX_PUBLIC_SECTION_COUNT, sections.len());
}

#[test]
fn try_deserialize_over_max_number_of_public_sections() {
    let adv_builder = build_dummy_advertisement_sections(NP_V1_ADV_MAX_PUBLIC_SECTION_COUNT);
    let mut adv = adv_builder.into_advertisement().as_slice().to_vec();

    // Push an extra section
    adv.extend_from_slice(
        [
            0x01, // Section header
            0x03, // Public identity
        ]
        .as_slice(),
    );

    let (remaining, header) = parse_adv_header(&adv).unwrap();

    let v1_header = if let AdvHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };
    let _ = parse_sections(v1_header, remaining)
        .expect_err("Expected an error because number of sections is over limit");
}

pub(crate) fn random_de<R: rand::Rng>(rng: &mut R) -> GenericDataElement {
    let mut array = [0_u8; MAX_DE_LEN];
    rng.fill(&mut array[..]);
    let data: ArrayView<u8, MAX_DE_LEN> =
        ArrayView::try_from_array(array, rng.gen_range(0..=MAX_DE_LEN)).unwrap();
    // skip the first few DEs that Google uses
    GenericDataElement::try_from(rng.gen_range(20_u32..1000).into(), data.as_slice()).unwrap()
}

fn do_deserialize_section_unencrypted<I: serialize::SectionEncoder>(
    identity: I,
    expected_identity: PlaintextIdentityMode,
    de_offset: usize,
) {
    let mut rng = rand::rngs::StdRng::from_entropy();

    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    let mut section_builder = adv_builder.section_builder(identity).unwrap();

    let mut expected_de_data = vec![];
    let (expected_des, orig_des) =
        fill_section_random_des(&mut rng, &mut expected_de_data, &mut section_builder, de_offset);

    section_builder.add_to_advertisement();

    let adv = adv_builder.into_advertisement();

    let (remaining, header) = parse_adv_header(adv.as_slice()).unwrap();

    let v1_header = if let AdvHeader::V1(h) = header {
        h
    } else {
        panic!("incorrect header");
    };

    let sections = parse_sections(v1_header, remaining).unwrap();
    assert_eq!(1, sections.len());
    let section = sections[0].as_plaintext().unwrap();

    assert_eq!(section.identity(), expected_identity);
    let data_elements = section.collect_data_elements().unwrap();
    assert_eq!(data_elements, expected_des);
    assert_eq!(
        data_elements
            .iter()
            .map(|de| GenericDataElement::try_from(de.de_type(), de.contents()).unwrap())
            .collect::<Vec<_>>(),
        orig_des
    );
}

fn fill_section_random_des<'adv, R: rand::Rng, I: serialize::SectionEncoder>(
    mut rng: &mut R,
    sink: &'adv mut Vec<u8>,
    section_builder: &mut SectionBuilder<I>,
    de_offset: usize,
) -> (Vec<DataElement<'adv>>, Vec<GenericDataElement>) {
    let mut expected_des = vec![];
    let mut orig_des = vec![];
    let mut de_ranges = vec![];

    for _ in 0..rng.gen_range(1..10) {
        let de = random_de(&mut rng);

        let de_clone = de.clone();
        if section_builder.add_de(|_| de_clone).is_err() {
            break;
        }

        let orig_len = sink.len();
        de.write_de_contents(sink).unwrap();
        let contents_len = sink.len() - orig_len;
        de_ranges.push(orig_len..orig_len + contents_len);
        orig_des.push(de);
    }

    for (index, (de, range)) in orig_des.iter().zip(de_ranges).enumerate() {
        expected_des.push(DataElement {
            offset: u8::try_from(index + de_offset).unwrap().into(),
            de_type: de.de_header().de_type,
            contents: &sink[range],
        });
    }
    (expected_des, orig_des)
}

fn total_de_len(des: &[GenericDataElement]) -> usize {
    des.iter()
        .map(|de| {
            let mut buf = vec![];
            let _ = de.write_de_contents(&mut buf);
            de.de_header().serialize().len() + buf.len()
        })
        .sum()
}

type TryDeserOutput<'adv, M> = Option<WithMatchedCredential<M, DecryptedSection<'adv>>>;

/// Returns:
/// - `Ok(Some)` if a matching credential was found
/// - `Ok(None)` if no matching credential was found, or if `cred_source` provides no credentials
/// - `Err` if an error occurred.
fn try_deserialize_all_creds<'a, S, P>(
    arena: DeserializationArena<'a>,
    section: &'a CiphertextSection,
    cred_source: &'a S,
) -> Result<TryDeserOutput<'a, S::Matched>, BatchSectionDeserializeError>
where
    S: DiscoveryCredentialSource<'a, V1>,
    P: CryptoProvider,
{
    let mut allocator = arena.into_allocator();
    for (crypto_material, match_data) in cred_source.iter() {
        match section
            .try_resolve_identity_and_deserialize::<_, P>(&mut allocator, crypto_material.borrow())
        {
            Ok(s) => {
                let metadata_nonce = crypto_material.metadata_nonce::<P>();
                return Ok(Some(WithMatchedCredential::new(match_data, metadata_nonce, s)));
            }
            Err(e) => match e {
                SectionDeserializeError::IncorrectCredential => continue,
                SectionDeserializeError::ParseError => {
                    return Err(BatchSectionDeserializeError::ParseError)
                }
                SectionDeserializeError::ArenaOutOfSpace => {
                    return Err(BatchSectionDeserializeError::ArenaOutOfSpace)
                }
            },
        }
    }

    Ok(None)
}

fn build_dummy_advertisement_sections(number_of_sections: usize) -> AdvBuilder {
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    for _ in 0..number_of_sections {
        let section_builder = adv_builder.section_builder(PublicSectionEncoder::default()).unwrap();
        section_builder.add_to_advertisement();
    }
    adv_builder
}

#[derive(Debug, PartialEq, Eq)]
enum BatchSectionDeserializeError {
    /// Advertisement data is malformed
    ParseError,
    /// The given arena is not large enough to hold the decrypted data
    ArenaOutOfSpace,
}
