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

use rand::{seq::SliceRandom as _, SeedableRng as _};

extern crate std;

use crate::credential::matched::{EmptyMatchedCredential, HasIdentityMatch, MatchedCredential};
use crate::credential::MatchableCredential;
use crate::legacy::serialize::UnencryptedEncoder;
use crate::{
    credential::{
        book::{CredentialBook, CredentialBookBuilder},
        v0::{V0BroadcastCredential, V0DiscoveryCredential, V0},
    },
    deserialization_arena,
    deserialization_arena::DeserializationArena,
    deserialize_advertisement,
    legacy::{
        data_elements::actions::{ActionBits, ActionsDataElement},
        deserialize::DeserializedDataElement,
        serialize::{AdvBuilder, AdvEncoder, LdtEncoder},
        BLE_4_ADV_SVC_MAX_CONTENT_LEN,
    },
    V0AdvertisementContents,
};
use array_view::ArrayView;
use crypto_provider::CryptoProvider;
use crypto_provider_default::CryptoProviderImpl;
use ldt_np_adv::{V0IdentityToken, V0Salt, V0_IDENTITY_TOKEN_LEN};
use std::{prelude::rust_2021::*, vec};

#[test]
fn v0_all_identities_resolvable() {
    let mut rng = rand::rngs::StdRng::from_entropy();
    for _ in 0..100 {
        let identities = (0..100).map(|_| TestIdentity::random(&mut rng)).collect::<Vec<_>>();

        let (adv, adv_config) = adv_random_identity(&mut rng, &identities);

        let creds = identities
            .iter()
            .map(|i| MatchableCredential {
                discovery_credential: i.discovery_credential::<CryptoProviderImpl>(),
                match_data: EmptyMatchedCredential,
            })
            .collect::<Vec<_>>();

        let arena = deserialization_arena!();
        let cred_book =
            CredentialBookBuilder::build_cached_slice_book::<0, 0, CryptoProviderImpl>(&creds, &[]);
        let contents = deser_v0::<_, CryptoProviderImpl>(arena, adv.as_slice(), &cred_book);

        assert_adv_equals(&adv_config, &contents);
    }
}

#[test]
fn v0_only_non_matching_identities_available() {
    let mut rng = rand::rngs::StdRng::from_entropy();
    for _ in 0..100 {
        let identities = (0..100).map(|_| TestIdentity::random(&mut rng)).collect::<Vec<_>>();

        let (adv, adv_config) = adv_random_identity(&mut rng, &identities);

        let credentials = identities
            .iter()
            .filter(|i| {
                // remove identity used, if any
                !adv_config.identity.map(|sci| sci.key_seed == i.key_seed).unwrap_or(false)
            })
            .map(|i| MatchableCredential {
                discovery_credential: i.discovery_credential::<CryptoProviderImpl>(),
                match_data: EmptyMatchedCredential,
            })
            .collect::<Vec<_>>();

        let arena = deserialization_arena!();
        let cred_book = CredentialBookBuilder::build_cached_slice_book::<0, 0, CryptoProviderImpl>(
            &credentials,
            &[],
        );
        let contents = deser_v0::<_, CryptoProviderImpl>(arena, adv.as_slice(), &cred_book);

        match adv_config.identity {
            // we ended up generating plaintext, so it's fine
            None => assert_adv_equals(&adv_config, &contents),
            Some(_) => {
                // we generated an encrypted adv, but didn't include the credential
                assert_eq!(V0AdvertisementContents::NoMatchingCredentials, contents);
            }
        }
    }
}

#[test]
fn v0_no_creds_available_error_if_encrypted() {
    let mut rng = rand::rngs::StdRng::from_entropy();
    for _ in 0..100 {
        let identities = (0..100).map(|_| TestIdentity::random(&mut rng)).collect::<Vec<_>>();

        let (adv, adv_config) = adv_random_identity(&mut rng, &identities);

        let creds = Vec::<MatchableCredential<V0, EmptyMatchedCredential>>::new();

        let arena = deserialization_arena!();
        let cred_book =
            CredentialBookBuilder::build_cached_slice_book::<0, 0, CryptoProviderImpl>(&creds, &[]);
        let contents = deser_v0::<_, CryptoProviderImpl>(arena, adv.as_slice(), &cred_book);

        match adv_config.identity {
            // we ended up generating plaintext, so it's fine
            None => assert_adv_equals(&adv_config, &contents),
            Some(_) => {
                // we generated an encrypted adv, but didn't include the credential
                assert_eq!(V0AdvertisementContents::NoMatchingCredentials, contents);
            }
        }
    }
}

/// Short-hand for asserting that the contents of two V0 advertisements
/// are the same for tests where we only ever have 0-1 broadcasting
/// identities in play.
fn assert_adv_equals<M: MatchedCredential + AsRef<EmptyMatchedCredential>>(
    adv_config: &AdvConfig,
    adv: &V0AdvertisementContents<M>,
) {
    match adv_config.identity {
        None => match adv {
            V0AdvertisementContents::Plaintext(p) => {
                let action_bits = ActionBits::default();
                let de = ActionsDataElement::from(action_bits);

                assert_eq!(
                    vec![DeserializedDataElement::Actions(de)],
                    p.data_elements().collect::<Result<Vec<_>, _>>().unwrap()
                )
            }
            _ => panic!("should be a plaintext adv"),
        },
        Some(_) => match adv {
            V0AdvertisementContents::Decrypted(wmc) => {
                // different generic type param, so can't re-use the DE from above
                let action_bits = ActionBits::default();
                let de = ActionsDataElement::from(action_bits);

                assert_eq!(
                    vec![DeserializedDataElement::Actions(de)],
                    wmc.contents().data_elements().collect::<Result<Vec<_>, _>>().unwrap()
                );
                assert_eq!(
                    adv_config.identity.unwrap().identity_token,
                    wmc.contents().identity_token()
                );
            }
            _ => panic!("should be an encrypted adv"),
        },
    }
}

fn deser_v0<'adv, B, P>(
    arena: DeserializationArena<'adv>,
    adv: &'adv [u8],
    cred_book: &'adv B,
) -> V0AdvertisementContents<'adv, B::Matched>
where
    B: CredentialBook<'adv>,
    P: CryptoProvider,
{
    deserialize_advertisement::<_, P>(arena, adv, cred_book)
        .expect("Should be a valid advertisement")
        .into_v0()
        .expect("Should be V0")
}

/// Populate an advertisement with a randomly chosen identity and a DE
fn adv_random_identity<'a, R: rand::Rng>(
    mut rng: &mut R,
    identities: &'a Vec<TestIdentity>,
) -> (ArrayView<u8, { BLE_4_ADV_SVC_MAX_CONTENT_LEN }>, AdvConfig<'a>) {
    let identity = identities.choose(&mut rng).unwrap();
    if rng.gen_bool(0.5) {
        let mut adv_builder = AdvBuilder::new(UnencryptedEncoder);
        add_de(&mut adv_builder);

        (adv_builder.into_advertisement().unwrap(), AdvConfig::new(None))
    } else {
        let broadcast_cred = V0BroadcastCredential::new(identity.key_seed, identity.identity_token);
        let mut adv_builder = AdvBuilder::new(LdtEncoder::<CryptoProviderImpl>::new(
            V0Salt::from(rng.gen::<[u8; 2]>()),
            &broadcast_cred,
        ));
        add_de(&mut adv_builder);

        (adv_builder.into_advertisement().unwrap(), AdvConfig::new(Some(identity)))
    }
}

fn add_de<E>(adv_builder: &mut AdvBuilder<E>)
where
    E: AdvEncoder,
{
    let action_bits = ActionBits::default();
    let de = ActionsDataElement::from(action_bits);
    adv_builder.add_data_element(de).unwrap();
}

struct TestIdentity {
    key_seed: [u8; 32],
    identity_token: V0IdentityToken,
}

impl TestIdentity {
    /// Generate a new identity with random crypto material
    fn random<R: rand::Rng + rand::CryptoRng>(rng: &mut R) -> Self {
        Self {
            key_seed: rng.gen(),
            identity_token: rng.gen::<[u8; V0_IDENTITY_TOKEN_LEN]>().into(),
        }
    }

    /// Returns a discovery-credential using crypto material from this identity
    fn discovery_credential<C: CryptoProvider>(&self) -> V0DiscoveryCredential {
        let hkdf = self.hkdf::<C>();
        V0DiscoveryCredential::new(
            self.key_seed,
            hkdf.v0_identity_token_hmac_key().calculate_hmac::<C>(&self.identity_token.bytes()),
        )
    }

    fn hkdf<C: CryptoProvider>(&self) -> np_hkdf::NpKeySeedHkdf<C> {
        np_hkdf::NpKeySeedHkdf::new(&self.key_seed)
    }
}

struct AdvConfig<'a> {
    /// `Some` iff an encrypted identity should be used
    identity: Option<&'a TestIdentity>,
}

impl<'a> AdvConfig<'a> {
    fn new(identity: Option<&'a TestIdentity>) -> Self {
        Self { identity }
    }
}
