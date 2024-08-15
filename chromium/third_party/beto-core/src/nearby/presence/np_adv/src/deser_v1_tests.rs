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

#![allow(clippy::unwrap_used)]

use core::marker::PhantomData;
use rand::{rngs::StdRng, seq::SliceRandom as _, Rng as _, SeedableRng as _};

extern crate std;

use crate::{
    credential::{
        book::{CredentialBook, CredentialBookBuilder},
        v1::{
            SignedBroadcastCryptoMaterial, SimpleSignedBroadcastCryptoMaterial,
            V1DiscoveryCredential,
        },
        EmptyMatchedCredential, MatchableCredential, MatchedCredential,
    },
    de_type::EncryptedIdentityDataElementType,
    deserialization_arena,
    deserialization_arena::DeserializationArena,
    deserialize_advertisement,
    extended::{
        data_elements::GenericDataElement,
        deserialize::VerificationMode,
        serialize::{
            AdvBuilder, AdvertisementType, EncodedAdvertisement, MicEncryptedSectionEncoder,
            PublicSectionEncoder, SectionBuilder, SectionEncoder, SignedEncryptedSectionEncoder,
        },
    },
    AdvDeserializationError, AdvDeserializationErrorDetailsHazmat, HasIdentityMatch, MetadataKey,
    PlaintextIdentityMode, Section, V1AdvertisementContents, V1DeserializedSection,
};
use crypto_provider::{CryptoProvider, CryptoRng};
use std::{collections, prelude::rust_2021::*, vec};
use strum::IntoEnumIterator as _;

use crate::extended::NP_V1_ADV_MAX_PUBLIC_SECTION_COUNT;
use crypto_provider_default::CryptoProviderImpl;

#[test]
fn v1_plaintext() {
    let mut rng = StdRng::from_entropy();
    for _ in 0..100 {
        let identities = (0..100)
            .map(|_| TestIdentity::<CryptoProviderImpl>::random(&mut rng))
            .collect::<Vec<_>>();

        let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
        let section_configs: Vec<SectionConfig<CryptoProviderImpl>> =
            fill_plaintext_adv(&mut rng, &mut adv_builder);
        let adv = adv_builder.into_advertisement();
        let creds = identities
            .iter()
            .map(|i| MatchableCredential {
                discovery_credential: i.discovery_credential(),
                match_data: EmptyMatchedCredential,
            })
            .collect::<Vec<_>>();

        let arena = deserialization_arena!();
        // check if the section is empty or there is more than one public section
        let cred_book =
            CredentialBookBuilder::build_cached_slice_book::<0, 0, CryptoProviderImpl>(&[], &creds);
        if section_configs.is_empty() {
            let v1_error = deser_v1_error::<_, CryptoProviderImpl>(arena, &adv, &cred_book);
            assert_eq!(
                v1_error,
                AdvDeserializationError::ParseError {
                    details_hazmat:
                        AdvDeserializationErrorDetailsHazmat::AdvertisementDeserializeError
                }
            ); //assert a adv deserialization error
        } else {
            let v1_contents = deser_v1::<_, CryptoProviderImpl>(arena, &adv, &cred_book);
            assert_eq!(0, v1_contents.invalid_sections_count());
            assert_eq!(section_configs.len(), v1_contents.sections.len());
            for (section_config, section) in section_configs.iter().zip(v1_contents.sections.iter())
            {
                assert_section_equals(section_config, section);
            }
        }
    }
}

#[test]
fn v1_all_identities_resolvable_ciphertext() {
    let mut rng = StdRng::from_entropy();
    for _ in 0..100 {
        let identities = (0..100)
            .map(|_| TestIdentity::<CryptoProviderImpl>::random(&mut rng))
            .collect::<Vec<_>>();

        let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
        let section_configs = fill_encrypted_adv(&mut rng, &identities, &mut adv_builder);
        let adv = adv_builder.into_advertisement();
        let creds = identities
            .iter()
            .map(|i| MatchableCredential {
                discovery_credential: i.discovery_credential(),
                match_data: EmptyMatchedCredential,
            })
            .collect::<Vec<_>>();
        let arena = deserialization_arena!();
        // check if the section header would be 0 or if the section is empty
        let cred_book =
            CredentialBookBuilder::build_cached_slice_book::<0, 0, CryptoProviderImpl>(&[], &creds);
        if section_configs.is_empty() {
            let v1_error = deser_v1_error::<_, CryptoProviderImpl>(arena, &adv, &cred_book);
            assert_eq!(
                v1_error,
                AdvDeserializationError::ParseError {
                    details_hazmat:
                        AdvDeserializationErrorDetailsHazmat::AdvertisementDeserializeError
                }
            ); //assert a adv deserialization error
        } else {
            let v1_contents = deser_v1::<_, CryptoProviderImpl>(arena, &adv, &cred_book);
            assert_eq!(0, v1_contents.invalid_sections_count());
            assert_eq!(section_configs.len(), v1_contents.sections.len());
            for (section_config, section) in section_configs.iter().zip(v1_contents.sections.iter())
            {
                assert_section_equals(section_config, section);
            }
        }
    }
}

#[test]
fn v1_only_non_matching_identities_available_ciphertext() {
    let mut rng = StdRng::from_entropy();
    for _ in 0..100 {
        let identities = (0..100)
            .map(|_| TestIdentity::<CryptoProviderImpl>::random(&mut rng))
            .collect::<Vec<_>>();

        let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
        let section_configs = fill_encrypted_adv(&mut rng, &identities, &mut adv_builder);
        let adv = adv_builder.into_advertisement();
        let creds = identities
            .iter()
            .filter(|i| {
                // remove all identities used in sections
                !section_configs
                    .iter()
                    .any(|sc| sc.identity.map(|sci| sci.key_seed == i.key_seed).unwrap_or(false))
            })
            .map(|i| MatchableCredential {
                discovery_credential: i.discovery_credential(),
                match_data: EmptyMatchedCredential,
            })
            .collect::<Vec<_>>();

        let arena = deserialization_arena!();
        // check if the section header would be 0
        let cred_book =
            CredentialBookBuilder::build_cached_slice_book::<0, 0, CryptoProviderImpl>(&[], &creds);
        if section_configs.is_empty() {
            let v1_error = deser_v1_error::<_, CryptoProviderImpl>(arena, &adv, &cred_book);
            assert_eq!(
                v1_error,
                AdvDeserializationError::ParseError {
                    details_hazmat:
                        AdvDeserializationErrorDetailsHazmat::AdvertisementDeserializeError
                }
            ); //assert a adv deserialization error
        } else {
            let v1_contents = deser_v1::<_, CryptoProviderImpl>(arena, &adv, &cred_book);
            // all encrypted identity sections are invalid
            let encrypted_section_count =
                section_configs.iter().filter(|sc| sc.identity.is_some()).count();
            assert_eq!(encrypted_section_count, v1_contents.invalid_sections_count());
            assert_eq!(section_configs.len() - encrypted_section_count, v1_contents.sections.len());
            for (section_config, section) in section_configs
                .iter()
                // skip encrypted sections
                .filter(|sc| sc.identity.is_none())
                .zip(v1_contents.sections.iter())
            {
                assert_section_equals(section_config, section);
            }
        }
    }
}

#[test]
fn v1_no_creds_available_ciphertext() {
    let mut rng = StdRng::from_entropy();
    for _ in 0..100 {
        let identities = (0..100).map(|_| TestIdentity::random(&mut rng)).collect::<Vec<_>>();

        let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
        let section_configs = fill_encrypted_adv::<StdRng, CryptoProviderImpl>(
            &mut rng,
            &identities,
            &mut adv_builder,
        );
        let adv = adv_builder.into_advertisement();
        let arena = deserialization_arena!();
        // check if the section header would be 0
        let cred_book = CredentialBookBuilder::<EmptyMatchedCredential>::build_cached_slice_book::<
            0,
            0,
            CryptoProviderImpl,
        >(&[], &[]);
        if section_configs.is_empty() {
            let v1_error = deser_v1_error::<_, CryptoProviderImpl>(arena, &adv, &cred_book);
            assert_eq!(
                v1_error,
                AdvDeserializationError::ParseError {
                    details_hazmat:
                        AdvDeserializationErrorDetailsHazmat::AdvertisementDeserializeError
                }
            ); //assert a adv deserialization error
        } else {
            let v1_contents = deser_v1::<_, CryptoProviderImpl>(arena, &adv, &cred_book);
            // all encrypted identity sections are invalid
            let encrypted_section_count =
                section_configs.iter().filter(|sc| sc.identity.is_some()).count();
            assert_eq!(encrypted_section_count, v1_contents.invalid_sections_count());
            assert_eq!(section_configs.len() - encrypted_section_count, v1_contents.sections.len());

            for (section_config, section) in section_configs
                .iter()
                // skip encrypted sections
                .filter(|sc| sc.identity.is_none())
                .zip(v1_contents.sections.iter())
            {
                assert_section_equals(section_config, section);
            }
        }
    }
}

#[test]
fn v1_only_some_matching_identities_available_ciphertext() {
    let mut rng = StdRng::from_entropy();
    for _ in 0..100 {
        let identities = (0..100)
            .map(|_| TestIdentity::<CryptoProviderImpl>::random(&mut rng))
            .collect::<Vec<_>>();

        let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
        let section_configs = fill_encrypted_adv(&mut rng, &identities, &mut adv_builder);
        let adv = adv_builder.into_advertisement();
        // identities used in sections, which may be used in multiple sections too
        let identities_to_remove: collections::HashSet<_> = identities
            .iter()
            .filter(|i| {
                let identity_used = section_configs
                    .iter()
                    .any(|sc| sc.identity.map(|sci| sci.key_seed == i.key_seed).unwrap_or(false));

                // only remove half the identities that were used
                identity_used && rng.gen()
            })
            .map(|i| i.key_seed)
            .collect();

        let creds = identities
            .iter()
            .filter(|i| !identities_to_remove.contains(&i.key_seed))
            .map(|i| MatchableCredential {
                discovery_credential: i.discovery_credential(),
                match_data: EmptyMatchedCredential,
            })
            .collect::<Vec<_>>();

        let arena = deserialization_arena!();

        let cred_book =
            CredentialBookBuilder::build_cached_slice_book::<0, 0, CryptoProviderImpl>(&[], &creds);

        // check if the section header would be 0
        if section_configs.is_empty() {
            let v1_error = deser_v1_error::<_, CryptoProviderImpl>(arena, &adv, &cred_book);
            assert_eq!(
                v1_error,
                AdvDeserializationError::ParseError {
                    details_hazmat:
                        AdvDeserializationErrorDetailsHazmat::AdvertisementDeserializeError
                }
            ); //assert a adv deserialization error
        } else {
            let v1_contents = deser_v1::<_, CryptoProviderImpl>(arena, &adv, &cred_book);

            let affected_sections: Vec<_> = section_configs
                .iter()
                .filter(|sc| {
                    sc.identity
                        .map(|sci| identities_to_remove.iter().any(|ks| &sci.key_seed == ks))
                        .unwrap_or(false)
                })
                .collect();

            assert_eq!(affected_sections.len(), v1_contents.invalid_sections_count());
            assert_eq!(section_configs.len() - affected_sections.len(), v1_contents.sections.len());

            for (section_config, section) in section_configs
                .iter()
                // skip sections w/ removed identities
                .filter(|sc| {
                    sc.identity.map(|i| !identities_to_remove.contains(&i.key_seed)).unwrap_or(true)
                })
                .zip(v1_contents.sections.iter())
            {
                assert_section_equals(section_config, section);
            }
        }
    }
}

fn assert_section_equals<M: MatchedCredential, C: CryptoProvider>(
    section_config: &SectionConfig<C>,
    section: &V1DeserializedSection<M>,
) {
    match section_config.identity {
        None => match section {
            V1DeserializedSection::Plaintext(p) => {
                assert!(section_config.verification_mode.is_none());

                assert_eq!(section_config.plaintext_mode.unwrap(), p.identity());
                assert_eq!(
                    section_config.data_elements,
                    p.iter_data_elements().map(|de| (&de.unwrap()).into()).collect::<Vec<_>>()
                )
            }
            V1DeserializedSection::Decrypted(_) => panic!("no id, but decrypted section"),
        },
        Some(_) => match section {
            V1DeserializedSection::Plaintext(_) => panic!("id, but plaintext section"),
            V1DeserializedSection::Decrypted(wmc) => {
                assert!(section_config.plaintext_mode.is_none());

                assert_eq!(
                    section_config.data_elements,
                    wmc.contents()
                        .iter_data_elements()
                        .map(|de| (&de.unwrap()).into())
                        .collect::<Vec<GenericDataElement>>()
                );
                assert_eq!(
                    section_config.identity.unwrap().identity_type,
                    wmc.contents().identity_type()
                );
                assert_eq!(
                    section_config.identity.unwrap().extended_metadata_key,
                    wmc.contents().metadata_key()
                );
                assert_eq!(
                    section_config.verification_mode.unwrap(),
                    wmc.contents().verification_mode()
                );
            }
        },
    }
}

fn deser_v1_error<'a, B, P>(
    arena: DeserializationArena<'a>,
    adv: &'a EncodedAdvertisement,
    cred_book: &'a B,
) -> AdvDeserializationError
where
    B: CredentialBook<'a>,
    P: CryptoProvider,
{
    let v1_contents = match deserialize_advertisement::<_, P>(arena, adv.as_slice(), cred_book) {
        Err(e) => e,
        _ => panic!("Expecting an error!"),
    };
    v1_contents
}

fn deser_v1<'adv, B, P>(
    arena: DeserializationArena<'adv>,
    adv: &'adv EncodedAdvertisement,
    cred_book: &'adv B,
) -> V1AdvertisementContents<'adv, B::Matched>
where
    B: CredentialBook<'adv>,
    P: CryptoProvider,
{
    deserialize_advertisement::<_, P>(arena, adv.as_slice(), cred_book)
        .expect("Should be a valid advertisement")
        .into_v1()
        .expect("Should be V1")
}

/// Populate a random number of sections with randomly chosen identities and random DEs
fn fill_plaintext_adv<'a, R: rand::Rng, C: CryptoProvider>(
    mut rng: &mut R,
    adv_builder: &mut AdvBuilder,
) -> Vec<SectionConfig<'a, C>> {
    let mut expected = Vec::new();
    // build sections
    for _ in 0..rng.gen_range(0..=NP_V1_ADV_MAX_PUBLIC_SECTION_COUNT) {
        let res = adv_builder.section_builder(PublicSectionEncoder::default()).map(|s| {
            SectionConfig::new(
                None,
                Some(PlaintextIdentityMode::Public),
                None,
                add_des(s, &mut rng),
            )
        });
        match res {
            Ok(tuple) => expected.push(tuple),
            Err(_) => {
                // couldn't fit that section; maybe another smaller section will fit
                continue;
            }
        }
    }
    expected
}

/// Populate a random number of sections with randomly chosen identities and random DEs
fn fill_encrypted_adv<'a, R: rand::Rng, C: CryptoProvider>(
    mut rng: &mut R,
    identities: &'a Vec<TestIdentity<C>>,
    adv_builder: &mut AdvBuilder,
) -> Vec<SectionConfig<'a, C>> {
    let mut expected = Vec::new();
    let mut salt_rng = C::CryptoRng::new();
    // build sections
    for _ in 0..rng.gen_range(0..=6) {
        let chosen_index = rng.gen_range(0..identities.len());
        let identity = &identities[chosen_index];

        let broadcast_cm = identity.broadcast_credential();

        let res = if rng.gen_bool(0.5) {
            adv_builder
                .section_builder(MicEncryptedSectionEncoder::<C>::new_random_salt(
                    &mut salt_rng,
                    identity.identity_type,
                    &broadcast_cm,
                ))
                .map(|s| {
                    SectionConfig::new(
                        Some(identity),
                        None,
                        Some(VerificationMode::Mic),
                        add_des(s, &mut rng),
                    )
                })
        } else {
            adv_builder
                .section_builder(SignedEncryptedSectionEncoder::<C>::new_random_salt(
                    &mut salt_rng,
                    identity.identity_type,
                    &broadcast_cm,
                ))
                .map(|s| {
                    SectionConfig::new(
                        Some(identity),
                        None,
                        Some(VerificationMode::Signature),
                        add_des(s, &mut rng),
                    )
                })
        };
        match res {
            Ok(tuple) => expected.push(tuple),
            Err(_) => {
                // couldn't fit that section; maybe another smaller section will fit
                continue;
            }
        }
    }
    expected
}

struct TestIdentity<C: CryptoProvider> {
    identity_type: EncryptedIdentityDataElementType,
    key_seed: [u8; 32],
    extended_metadata_key: MetadataKey,
    key_pair: np_ed25519::KeyPair<C>,
    marker: PhantomData<C>,
}
impl<C: CryptoProvider> TestIdentity<C> {
    /// Generate a new identity with random crypto material
    fn random<R: rand::Rng + rand::CryptoRng>(rng: &mut R) -> Self {
        Self {
            identity_type: *EncryptedIdentityDataElementType::iter()
                .collect::<Vec<_>>()
                .choose(rng)
                .unwrap(),
            key_seed: rng.gen(),
            extended_metadata_key: MetadataKey(rng.gen()),
            key_pair: np_ed25519::KeyPair::<C>::generate(),
            marker: PhantomData,
        }
    }
    /// Returns a (simple, signed) broadcast credential using crypto material from this identity
    fn broadcast_credential(&self) -> SimpleSignedBroadcastCryptoMaterial {
        SimpleSignedBroadcastCryptoMaterial::new(
            self.key_seed,
            self.extended_metadata_key,
            self.key_pair.private_key(),
        )
    }
    /// Returns a discovery credential using crypto material from this identity
    fn discovery_credential(&self) -> V1DiscoveryCredential {
        self.broadcast_credential().derive_v1_discovery_credential::<C>()
    }
}
/// Add several DEs with random types and contents
fn add_des<I: SectionEncoder, R: rand::Rng>(
    mut sb: SectionBuilder<I>,
    rng: &mut R,
) -> Vec<GenericDataElement> {
    let mut des = Vec::new();
    for _ in 0..rng.gen_range(0..=2) {
        // not worried about multi byte type encoding here, so just sticking with what can sometimes
        // fit in the 1-byte header, and isn't an identity element
        let de_type = rng.gen_range(10_u32..=20);
        // covers lengths that fit or don't fit in 3 bits (1 byte header)
        let de_len = rng.gen_range(0..=10);
        let mut de_data = vec![0; de_len];
        rng.fill(&mut de_data[..]);
        let de = GenericDataElement::try_from(de_type.into(), &de_data).unwrap();
        if sb.add_de(|_| de.clone()).is_err() {
            // no more room in the section
            break;
        }
        des.push(de);
    }
    sb.add_to_advertisement();
    des
}
struct SectionConfig<'a, C: CryptoProvider> {
    /// `Some` iff an encrypted identity should be used
    identity: Option<&'a TestIdentity<C>>,
    /// `Some` iff `identity` is `None`
    plaintext_mode: Option<PlaintextIdentityMode>,
    /// `Some` iff `identity` is `Some`
    verification_mode: Option<VerificationMode>,
    data_elements: Vec<GenericDataElement>,
}
impl<'a, C: CryptoProvider> SectionConfig<'a, C> {
    pub fn new(
        identity: Option<&'a TestIdentity<C>>,
        plaintext_mode: Option<PlaintextIdentityMode>,
        verification_mode: Option<VerificationMode>,
        data_elements: Vec<GenericDataElement>,
    ) -> Self {
        Self { identity, plaintext_mode, verification_mode, data_elements }
    }
}
