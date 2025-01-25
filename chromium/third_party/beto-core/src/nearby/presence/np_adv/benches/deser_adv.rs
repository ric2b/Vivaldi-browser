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

#![allow(
    missing_docs,
    unused_results,
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::indexing_slicing,
    clippy::panic
)]

use criterion::{black_box, criterion_group, criterion_main, Bencher, Criterion};
use crypto_provider::{ed25519, CryptoProvider, CryptoRng};
use crypto_provider_default::CryptoProviderImpl;
use ldt_np_adv::{V0IdentityToken, V0Salt, V0_IDENTITY_TOKEN_LEN};

use np_adv::credential::matched::EmptyMatchedCredential;
use np_adv::deserialization_arena;
use np_adv::extended::serialize::AdvertisementType;
use np_adv::extended::V1IdentityToken;
use np_adv::legacy::serialize::UnencryptedEncoder;
use np_adv::{
    credential::{book::*, v0::*, v1::*, *},
    deserialize_advertisement,
    extended::{
        data_elements::{GenericDataElement, TxPowerDataElement},
        deserialize::VerificationMode,
        serialize::{
            AdvBuilder as ExtendedAdvBuilder, MicEncryptedSectionEncoder, SectionBuilder,
            SectionEncoder, SignedEncryptedSectionEncoder, UnencryptedSectionEncoder,
        },
    },
    legacy::{
        data_elements::actions::{ActionBits, ActionsDataElement},
        serialize::{AdvBuilder as LegacyAdvBuilder, LdtEncoder},
    },
    shared_data::TxPower,
};
use np_hkdf::{DerivedSectionKeys, NpKeySeedHkdf};
use rand::{Rng as _, SeedableRng as _};
use strum::IntoEnumIterator;

pub fn deser_adv_v1_encrypted(c: &mut Criterion) {
    let mut crypto_rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();

    for crypto_type in CryptoMaterialType::iter() {
        for &identity_type in &[VerificationMode::Mic, VerificationMode::Signature] {
            for &num_identities in &[10, 100, 1000] {
                for &num_sections in &[1, 2] {
                    // measure worst-case performance -- the correct identities will be the last
                    // num_sections of the identities to be tried
                    c.bench_function(
                        &format!(
                            "Deser V1 encrypted: crypto={crypto_type:?}/mode={identity_type:?}/ids={num_identities}/sections={num_sections}"
                        ),
                        |b| {
                            let identities = (0..num_identities)
                                .map(|_| V1Identity::random::<CryptoProviderImpl>(&mut crypto_rng))
                                .collect::<Vec<_>>();

                            let mut adv_builder = ExtendedAdvBuilder::new(AdvertisementType::Encrypted);

                            // take the first n identities, one section per identity
                            for identity in identities.iter().take(num_sections) {
                                let broadcast_cm = V1BroadcastCredential::new(
                                    identity.key_seed,
                                    identity.identity_token,
                                    identity.private_key.clone(),
                                );
                                match identity_type {
                                    VerificationMode::Mic => {
                                        let mut sb = adv_builder
                                            .section_builder(MicEncryptedSectionEncoder::<_>::new_random_salt::<CryptoProviderImpl>(
                                                &mut crypto_rng,
                                                &broadcast_cm,
                                            ))
                                            .unwrap();

                                        add_des(&mut sb);
                                        sb.add_to_advertisement::<CryptoProviderImpl>();
                                    }
                                    VerificationMode::Signature => {
                                        let mut sb = adv_builder
                                            .section_builder(SignedEncryptedSectionEncoder::new_random_salt::<CryptoProviderImpl>(
                                                &mut crypto_rng,
                                                &broadcast_cm,
                                            ))
                                            .unwrap();

                                        add_des(&mut sb);
                                        sb.add_to_advertisement::<CryptoProviderImpl>();
                                    }
                                }
                            }

                            let adv = adv_builder.into_advertisement();

                            run_with_v1_creds::<
                                CryptoProviderImpl
                            >(
                                b, crypto_type, identities, adv.as_slice(),
                            )
                        },
                    );
                }
            }
        }
    }
}

pub fn deser_adv_v1_plaintext(c: &mut Criterion) {
    c.bench_function("Deser V1 plaintext: sections=1", |b| {
        let mut adv_builder = ExtendedAdvBuilder::new(AdvertisementType::Plaintext);

        let mut sb = adv_builder.section_builder(UnencryptedSectionEncoder).unwrap();

        add_des(&mut sb);
        sb.add_to_advertisement::<CryptoProviderImpl>();

        let adv = adv_builder.into_advertisement();

        run_with_v1_creds::<CryptoProviderImpl>(
            b,
            CryptoMaterialType::MinFootprint,
            vec![],
            adv.as_slice(),
        )
    });
}

pub fn deser_adv_v0_encrypted(c: &mut Criterion) {
    let mut rng = rand::rngs::StdRng::from_entropy();
    for crypto_type in CryptoMaterialType::iter() {
        for &num_identities in &[10, 100, 1000] {
            // measure worst-case performance -- the correct identities will be the last
            // num_sections of the identities to be tried
            c.bench_function(
                &format!("Deser V0 encrypted: crypto={crypto_type:?}/ids={num_identities}"),
                |b| {
                    let identities = (0..num_identities)
                        .map(|_| V0Identity::random(&mut rng))
                        .collect::<Vec<_>>();

                    let identity = &identities[0];

                    let broadcast_cm =
                        V0BroadcastCredential::new(identity.key_seed, identity.identity_token);

                    let mut adv_builder =
                        LegacyAdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(
                            V0Salt::from(rng.gen::<[u8; 2]>()),
                            &broadcast_cm,
                        ));

                    let action_bits = ActionBits::default();
                    adv_builder.add_data_element(ActionsDataElement::from(action_bits)).unwrap();

                    let adv = adv_builder.into_advertisement().unwrap();

                    run_with_v0_creds::<CryptoProviderImpl>(
                        b,
                        crypto_type,
                        identities,
                        adv.as_slice(),
                    )
                },
            );
        }
    }
}

pub fn deser_adv_v0_plaintext(c: &mut Criterion) {
    let mut adv_builder = LegacyAdvBuilder::new(UnencryptedEncoder);

    let action_bits = ActionBits::default();
    adv_builder.add_data_element(ActionsDataElement::from(action_bits)).unwrap();
    let adv = adv_builder.into_advertisement().unwrap();

    let cred_book = CredentialBookBuilder::<EmptyMatchedCredential>::build_cached_slice_book::<
        0,
        0,
        CryptoProviderImpl,
    >(&[], &[]);

    for &num_advs in &[1, 10, 100, 1000] {
        c.bench_function(
            format!("Deser V0 plaintext with {num_advs} advertisements").as_str(),
            |b| {
                b.iter(|| {
                    for _ in 0..num_advs {
                        black_box(
                            deserialize_advertisement::<_, CryptoProviderImpl>(
                                deserialization_arena!(),
                                black_box(adv.as_slice()),
                                black_box(&cred_book),
                            )
                            .expect("Should succeed"),
                        );
                    }
                })
            },
        );
    }
}

/// Benchmark decrypting a V0 advertisement with credentials built from the reversed list of
/// identities
fn run_with_v0_creds<C>(
    b: &mut Bencher,
    crypto_material_type: CryptoMaterialType,
    identities: Vec<V0Identity>,
    adv: &[u8],
) where
    C: CryptoProvider,
{
    let mut creds = identities
        .into_iter()
        .map(|identity| identity.into_discovery_credential::<C>())
        .map(|crypto_material| MatchableCredential {
            discovery_credential: crypto_material,
            match_data: EmptyMatchedCredential,
        })
        .collect::<Vec<_>>();

    // reverse the identities so that we're scanning to the end of the
    // cred source for predictably bad performance
    creds.reverse();

    match crypto_material_type {
        CryptoMaterialType::MinFootprint => {
            // Cache size of 0 => only min-footprint creds
            let cred_book = CredentialBookBuilder::<_>::build_cached_slice_book::<
                0,
                0,
                CryptoProviderImpl,
            >(&creds, &[]);

            b.iter(|| {
                black_box(
                    deserialize_advertisement::<_, C>(deserialization_arena!(), adv, &cred_book)
                        .map(|_| 0_u8)
                        .unwrap(),
                )
            });
        }
        CryptoMaterialType::Precalculated => {
            let cred_book = CredentialBookBuilder::<_>::build_precalculated_owned_book::<C>(
                creds,
                core::iter::empty(),
            );
            b.iter(|| {
                black_box(
                    deserialize_advertisement::<_, C>(deserialization_arena!(), adv, &cred_book)
                        .map(|_| 0_u8)
                        .unwrap(),
                )
            });
        }
    }
}

/// Benchmark decrypting a V1 advertisement with credentials built from the reversed list of
/// identities
fn run_with_v1_creds<C>(
    b: &mut Bencher,
    crypto_material_type: CryptoMaterialType,
    identities: Vec<V1Identity>,
    adv: &[u8],
) where
    C: CryptoProvider,
{
    let mut creds = identities
        .into_iter()
        .map(|identity| identity.into_discovery_credential::<C>())
        .map(|crypto_material| MatchableCredential {
            discovery_credential: crypto_material,
            match_data: EmptyMatchedCredential,
        })
        .collect::<Vec<_>>();

    // reverse the identities so that we're scanning to the end of the
    // cred source for predictably bad performance
    creds.reverse();

    match crypto_material_type {
        CryptoMaterialType::MinFootprint => {
            // Cache size of 0 => only min-footprint creds
            let cred_book = CredentialBookBuilder::<_>::build_cached_slice_book::<
                0,
                0,
                CryptoProviderImpl,
            >(&[], &creds);

            b.iter(|| {
                black_box(
                    deserialize_advertisement::<_, C>(deserialization_arena!(), adv, &cred_book)
                        .map(|_| 0_u8)
                        .unwrap(),
                )
            });
        }
        CryptoMaterialType::Precalculated => {
            let cred_book = CredentialBookBuilder::<_>::build_precalculated_owned_book::<C>(
                core::iter::empty(),
                creds,
            );
            b.iter(|| {
                black_box(
                    deserialize_advertisement::<_, C>(deserialization_arena!(), adv, &cred_book)
                        .map(|_| 0_u8)
                        .unwrap(),
                )
            });
        }
    }
}

fn add_des<I: SectionEncoder>(
    sb: &mut SectionBuilder<&mut np_adv::extended::serialize::AdvBuilder, I>,
) {
    sb.add_de_res(|_| TxPower::try_from(17).map(TxPowerDataElement::from)).unwrap();
    sb.add_de_res(|_| GenericDataElement::try_from(100_u32.into(), &[0; 10])).unwrap();
}
criterion_group!(
    benches,
    deser_adv_v1_encrypted,
    deser_adv_v1_plaintext,
    deser_adv_v0_encrypted,
    deser_adv_v0_plaintext
);
criterion_main!(benches);

struct V0Identity {
    key_seed: [u8; 32],
    identity_token: V0IdentityToken,
}

impl V0Identity {
    /// Generate a new identity with random crypto material
    fn random<R: rand::Rng + rand::CryptoRng>(rng: &mut R) -> Self {
        Self {
            key_seed: rng.gen(),
            identity_token: V0IdentityToken::from(rng.gen::<[u8; V0_IDENTITY_TOKEN_LEN]>()),
        }
    }
    /// Convert this `V0Identity` into a V0 discovery credential.
    fn into_discovery_credential<C: CryptoProvider>(self) -> V0DiscoveryCredential {
        let hkdf = NpKeySeedHkdf::<C>::new(&self.key_seed);
        V0DiscoveryCredential::new(
            self.key_seed,
            hkdf.v0_identity_token_hmac_key().calculate_hmac::<C>(self.identity_token.as_slice()),
        )
    }
}

struct V1Identity {
    key_seed: [u8; 32],
    identity_token: V1IdentityToken,
    private_key: ed25519::PrivateKey,
}

impl V1Identity {
    /// Generate a new identity with random crypto material
    fn random<C: CryptoProvider>(rng: &mut C::CryptoRng) -> Self {
        Self {
            key_seed: rng.gen(),
            identity_token: rng.gen(),
            private_key: ed25519::PrivateKey::generate::<C::Ed25519>(),
        }
    }
    /// Convert this `V1Identity` into a `V1DiscoveryCredential`.
    fn into_discovery_credential<C: CryptoProvider>(self) -> V1DiscoveryCredential {
        let hkdf = NpKeySeedHkdf::<C>::new(&self.key_seed);

        V1DiscoveryCredential::new(
            self.key_seed,
            hkdf.v1_mic_short_salt_keys()
                .identity_token_hmac_key()
                .calculate_hmac::<C>(self.identity_token.as_slice()),
            hkdf.v1_mic_extended_salt_keys()
                .identity_token_hmac_key()
                .calculate_hmac::<C>(self.identity_token.as_slice()),
            hkdf.v1_signature_keys()
                .identity_token_hmac_key()
                .calculate_hmac::<C>(self.identity_token.as_slice()),
            self.private_key.derive_public_key::<C::Ed25519>(),
        )
    }
}

#[derive(strum_macros::EnumIter, Clone, Copy, Debug)]
enum CryptoMaterialType {
    MinFootprint,
    Precalculated,
}
