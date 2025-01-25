// Copyright 2024 Google LLC
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

extern crate std;

use super::*;
use crate::{deserialization_arena, extended::salt::*};
use rand::SeedableRng;

#[test]
fn deserialize_rand_identities_single_section_finds_correct_one_mic_short_salt() {
    deserialize_rand_identities_finds_correct_one(
        |_rng, bc| {
            let salt = ShortV1Salt::from([0x3; 2]);
            MicEncryptedSectionEncoder::<_>::new_with_salt::<CryptoProviderImpl>(salt, bc)
        },
        VerificationMode::Mic,
    )
}

#[test]
fn deserialize_rand_identities_single_section_finds_correct_one_mic_extended_salt() {
    deserialize_rand_identities_finds_correct_one(
        |rng, bc| {
            let salt: ExtendedV1Salt = rng.gen();
            MicEncryptedSectionEncoder::<_>::new_with_salt::<CryptoProviderImpl>(salt, bc)
        },
        VerificationMode::Mic,
    )
}

#[test]
fn deserialize_rand_identities_single_section_finds_correct_one_signed() {
    deserialize_rand_identities_finds_correct_one(
        SignedEncryptedSectionEncoder::new_random_salt::<CryptoProviderImpl>,
        VerificationMode::Signature,
    )
}

#[test]
fn v1_plaintext() {
    let mut rng = StdRng::from_entropy();
    for _ in 0..100 {
        let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
        let section_config = fill_plaintext_adv(&mut rng, &mut adv_builder);
        let adv = adv_builder.into_advertisement();
        let arena = deserialization_arena!();
        let cred_book =
            CredentialBookBuilder::build_cached_slice_book::<0, 0, CryptoProviderImpl>(&[], &[]);

        let v1_contents = deser_v1::<_, CryptoProviderImpl>(arena, adv.as_slice(), &cred_book);
        assert_eq!(0, v1_contents.invalid_sections_count());
        assert_eq!(1, v1_contents.sections().len());
        let sections = v1_contents.into_sections();
        assert_section_equals(&section_config, &sections[0]);
    }
}

#[test]
fn v1_multiple_plaintext_sections() {
    let mut rng = StdRng::from_entropy();
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Plaintext);
    add_plaintext_section(&mut rng, &mut adv_builder).unwrap();

    // append an extra plaintext section
    let adv = [
        adv_builder.into_advertisement().as_slice(),
        &[
            0x00, // format unencrypted
            0x03, // section len
        ],
        &[0xDD; 3], // 3 bytes of de contents
    ]
    .concat();

    let arena = deserialization_arena!();
    let cred_book = build_empty_cred_book();
    let v1_contents = deser_v1::<_, CryptoProviderImpl>(arena, &adv, &cred_book);
    assert_eq!(0, v1_contents.invalid_sections_count());
    assert_eq!(2, v1_contents.sections().len());
}

#[test]
fn v1_all_identities_resolvable_ciphertext() {
    let mut rng = StdRng::from_entropy();
    for _ in 0..100 {
        let identities = TestIdentities::generate::<100, _, CryptoProviderImpl>(&mut rng);
        let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
        let section_configs = fill_with_encrypted_sections::<_, CryptoProviderImpl>(
            &mut rng,
            &identities,
            &mut adv_builder,
        );
        let adv = adv_builder.into_advertisement();

        let arena = deserialization_arena!();
        let cred_book = identities.build_cred_book::<CryptoProviderImpl>();

        let v1_contents = deser_v1::<_, CryptoProviderImpl>(arena, adv.as_slice(), &cred_book);
        assert_eq!(0, v1_contents.invalid_sections_count());
        assert_eq!(section_configs.len(), v1_contents.sections().len());
        for (section_config, section) in section_configs.iter().zip(v1_contents.sections()) {
            assert_section_equals(section_config, section);
        }
    }
}

#[test]
fn v1_only_some_matching_identities_available_ciphertext() {
    let mut rng = StdRng::from_entropy();
    for _ in 0..100 {
        let mut identities = TestIdentities::generate::<100, _, CryptoProviderImpl>(&mut rng);
        let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);
        let cloned_identities = identities.clone();
        let section_configs = fill_with_encrypted_sections::<_, CryptoProviderImpl>(
            &mut rng,
            &cloned_identities,
            &mut adv_builder,
        );
        let adv = adv_builder.into_advertisement();

        // Hopefully one day we can use extract_if instead: https://github.com/rust-lang/rust/issues/43244
        let mut removed = vec![];
        // Remove some of the identities which have been used to build the adv
        identities.0.retain(|identity| {
            let res = !section_configs.iter().any(|sc| match &sc.identity_kind {
                IdentityKind::Plaintext => panic!("There are no plaintext sections"),
                IdentityKind::Encrypted { identity: section_identity, verification_mode: _ } => {
                    // only remove half the identities that were used
                    (identity.key_seed == section_identity.key_seed) && rng.gen()
                }
            });
            if !(res) {
                removed.push(identity.clone());
            }
            res
        });

        // Need to account for how many sections were affected since the same identity could be used
        // to encode multiple different sections
        let affected_sections = section_configs
            .iter()
            .filter(|sc| match sc.identity_kind {
                IdentityKind::Plaintext => {
                    panic!("There are no plaintext sections")
                }
                IdentityKind::Encrypted { identity: si, verification_mode: _ } => {
                    removed.iter().any(|i| i.key_seed == si.key_seed)
                }
            })
            .count();

        let arena = deserialization_arena!();
        let cred_book = identities.build_cred_book::<CryptoProviderImpl>();
        let v1_contents = deser_v1::<_, CryptoProviderImpl>(arena, adv.as_slice(), &cred_book);

        assert_eq!(affected_sections, v1_contents.invalid_sections_count());
        assert_eq!(section_configs.len() - affected_sections, v1_contents.sections().len());

        for (section_config, section) in section_configs
            .iter()
            // skip sections w/ removed identities
            .filter(|sc| match sc.identity_kind {
                IdentityKind::Plaintext => {
                    panic!("There are no plaintext sections")
                }
                IdentityKind::Encrypted { identity: si, verification_mode: _ } => {
                    !removed.iter().any(|i| i.key_seed == si.key_seed)
                }
            })
            .zip(v1_contents.sections())
        {
            assert_section_equals(section_config, section);
        }
    }
}

#[test]
fn v1_decrypted_mic_short_salt_matches() {
    let mut rng = StdRng::from_entropy();
    let salt = MultiSalt::Short(rng.gen::<[u8; 2]>().into());
    v1_decrypted_adv_salt_matches(
        &mut rng,
        salt,
        add_mic_with_salt_to_adv::<_, CryptoProviderImpl, 0>,
    );
}

#[test]
fn v1_decrypted_mic_extended_salt_matches() {
    let mut rng = StdRng::from_entropy();
    let salt = MultiSalt::Extended(rng.gen::<[u8; 16]>().into());
    v1_decrypted_adv_salt_matches(
        &mut rng,
        salt,
        add_mic_with_salt_to_adv::<_, CryptoProviderImpl, 0>,
    );
}

#[test]
fn v1_decrypted_sig_extended_salt_matches() {
    let mut rng = StdRng::from_entropy();
    let salt = MultiSalt::Extended(rng.gen::<[u8; 16]>().into());
    v1_decrypted_adv_salt_matches(
        &mut rng,
        salt,
        add_sig_with_salt_to_adv::<_, CryptoProviderImpl, 0>,
    );
}

fn v1_decrypted_adv_salt_matches(
    rng: &mut StdRng,
    salt: MultiSalt,
    add_to_adv: impl for<'a> Fn(
        &mut StdRng,
        &'a TestIdentity,
        &mut AdvBuilder,
        MultiSalt,
    ) -> Result<SectionConfig<'a>, AddSectionError>,
) {
    let identities = TestIdentities::generate::<1, _, CryptoProviderImpl>(rng);
    let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

    let _ = add_to_adv(rng, &identities.0[0], &mut adv_builder, salt);

    let adv = adv_builder.into_advertisement();
    let arena = deserialization_arena!();
    let cred_book = identities.build_cred_book::<CryptoProviderImpl>();

    let sections =
        deser_v1::<_, CryptoProviderImpl>(arena, adv.as_slice(), &cred_book).into_sections();
    let section = &sections[0];
    let decrypted = match section {
        V1DeserializedSection::Plaintext(_) => {
            panic!("section is encrypted")
        }
        V1DeserializedSection::Decrypted(d) => d,
    };
    assert_eq!(salt, *decrypted.contents().salt())
}
