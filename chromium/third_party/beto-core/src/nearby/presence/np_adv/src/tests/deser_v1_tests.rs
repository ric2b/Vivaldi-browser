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

extern crate std;

use std::{prelude::rust_2021::*, vec};

use rand::{
    distributions::{Distribution, Standard},
    random,
    rngs::StdRng,
    Rng, SeedableRng as _,
};

use array_view::ArrayView;
use crypto_provider::{ed25519, CryptoRng};
use crypto_provider_default::CryptoProviderImpl;
use np_hkdf::v1_salt::ExtendedV1Salt;

use crate::{
    credential::{book::*, matched::*, v0::*, v1::*, *},
    extended::{
        data_elements::GenericDataElement,
        deserialize::{data_element::DataElement, section::intermediate::PlaintextSection, *},
        salt::MultiSalt,
        serialize::*,
        *,
    },
    *,
};

mod happy_path;

mod error_condition;

impl<'adv, M: MatchedCredential> V1DeserializedSection<'adv, M> {
    fn as_plaintext_section(&self) -> &PlaintextSection {
        match self {
            V1DeserializedSection::Plaintext(c) => c,
            V1DeserializedSection::Decrypted(_) => {
                panic!("Casting into invalid enum variant")
            }
        }
    }

    fn as_ciphertext_section(&self) -> &WithMatchedCredential<M, DecryptedSection<'adv>> {
        match self {
            V1DeserializedSection::Plaintext(_) => panic!("Casting into invalid enum variant"),
            V1DeserializedSection::Decrypted(wmc) => wmc,
        }
    }
}

fn assert_section_equals(
    section_config: &SectionConfig,
    section: &V1DeserializedSection<
        ReferencedMatchedCredential<MetadataMatchedCredential<Vec<u8>>>,
    >,
) {
    match &section_config.identity_kind {
        IdentityKind::Plaintext => {
            let plaintext_section = section.as_plaintext_section();
            assert_eq!(
                section_config.data_elements,
                plaintext_section
                    .iter_data_elements()
                    .map(|de| (&de.unwrap()).into())
                    .collect::<Vec<_>>()
            )
        }
        IdentityKind::Encrypted { verification_mode, identity } => {
            let enc_section = section.as_ciphertext_section();

            let decrypted_metadata = enc_section.decrypt_metadata::<CryptoProviderImpl>().unwrap();
            assert_eq!(&identity.plaintext_metadata, &decrypted_metadata);

            let expected_contents =
                section_config.data_elements.clone().iter().fold(Vec::new(), |mut buf, de| {
                    buf.extend_from_slice(de.de_header().serialize().as_slice());
                    de.write_de_contents(&mut buf).unwrap();
                    buf
                });
            let contents = enc_section.contents();
            assert_eq!(&expected_contents, contents.plaintext());

            assert_eq!(
                section_config.data_elements,
                contents
                    .iter_data_elements()
                    .map(|de| (&de.unwrap()).into())
                    .collect::<Vec<GenericDataElement>>()
            );
            assert_eq!(&identity.identity_token, enc_section.contents().identity_token());
            assert_eq!(verification_mode, &enc_section.contents().verification_mode());
        }
    }
}

fn deser_v1_error<'a, B, P>(
    arena: DeserializationArena<'a>,
    adv: &'a [u8],
    cred_book: &'a B,
) -> AdvDeserializationError
where
    B: CredentialBook<'a>,
    P: CryptoProvider,
{
    let v1_contents = match crate::deserialize_advertisement::<_, P>(arena, adv, cred_book) {
        Err(e) => e,
        _ => panic!("Expecting an error!"),
    };
    v1_contents
}

fn deser_v1<'adv, B, P>(
    arena: DeserializationArena<'adv>,
    adv: &'adv [u8],
    cred_book: &'adv B,
) -> V1AdvertisementContents<'adv, B::Matched>
where
    B: CredentialBook<'adv>,
    P: CryptoProvider,
{
    crate::deserialize_advertisement::<_, P>(arena, adv, cred_book)
        .expect("Should be a valid advertisement")
        .into_v1()
        .expect("Should be V1")
}

fn build_empty_cred_book() -> PrecalculatedOwnedCredentialBook<EmptyMatchedCredential> {
    PrecalculatedOwnedCredentialBook::new(
        PrecalculatedOwnedCredentialSource::<V0, EmptyMatchedCredential>::new::<CryptoProviderImpl>(
            Vec::new(),
        ),
        PrecalculatedOwnedCredentialSource::<V1, EmptyMatchedCredential>::new::<CryptoProviderImpl>(
            Vec::new(),
        ),
    )
}

/// Populate a random number of sections with randomly chosen identities and random DEs
fn fill_plaintext_adv<'a, R: rand::Rng>(
    rng: &mut R,
    adv_builder: &mut AdvBuilder,
) -> SectionConfig<'a> {
    adv_builder
        .section_builder(UnencryptedSectionEncoder)
        .map(|mut s| {
            let mut sink = Vec::new();
            // plaintext sections cannot be empty so we need to be sure at least 1 DE is generated
            let (_, des) = fill_section_random_des::<_, _, 1>(rng, &mut sink, &mut s);
            s.add_to_advertisement::<CryptoProviderImpl>();
            SectionConfig::new(IdentityKind::Plaintext, des)
        })
        .unwrap()
}

impl Distribution<VerificationMode> for Standard {
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> VerificationMode {
        match rng.gen_range(0..=2) {
            0 => VerificationMode::Signature,
            _ => VerificationMode::Mic,
        }
    }
}

pub(crate) fn add_sig_rand_salt_to_adv<'a, R: rand::Rng, C: CryptoProvider, const M: usize>(
    rng: &mut R,
    identity: &'a TestIdentity,
    adv_builder: &mut AdvBuilder,
) -> Result<SectionConfig<'a>, AddSectionError> {
    let salt: ExtendedV1Salt = rng.gen::<[u8; 16]>().into();
    add_sig_with_salt_to_adv::<_, C, M>(rng, identity, adv_builder, salt.into())
}

fn add_sig_with_salt_to_adv<'a, R: rand::Rng, C: CryptoProvider, const M: usize>(
    rng: &mut R,
    identity: &'a crate::tests::deser_v1_tests::TestIdentity,
    adv_builder: &mut AdvBuilder,
    salt: MultiSalt,
) -> Result<crate::tests::deser_v1_tests::SectionConfig<'a>, AddSectionError> {
    let broadcast_cred = identity.broadcast_credential();
    let salt = match salt {
        MultiSalt::Short(_) => {
            panic!("Invalid salt type for signature encrpted adv")
        }
        MultiSalt::Extended(e) => e,
    };
    adv_builder.section_builder(SignedEncryptedSectionEncoder::new::<C>(salt, &broadcast_cred)).map(
        |mut s| {
            let mut sink = Vec::new();
            let (_, des) = fill_section_random_des::<_, _, M>(rng, &mut sink, &mut s);
            s.add_to_advertisement::<C>();
            SectionConfig::new(
                IdentityKind::Encrypted {
                    verification_mode: VerificationMode::Signature,
                    identity,
                },
                des,
            )
        },
    )
}

pub(crate) fn add_mic_rand_salt_to_adv<'a, R: rand::Rng, C: CryptoProvider, const M: usize>(
    rng: &mut R,
    identity: &'a TestIdentity,
    adv_builder: &mut AdvBuilder,
) -> Result<SectionConfig<'a>, AddSectionError> {
    let salt = if rng.gen_bool(0.5) {
        MultiSalt::Short(rng.gen::<[u8; 2]>().into())
    } else {
        MultiSalt::Extended(rng.gen::<[u8; 16]>().into())
    };

    add_mic_with_salt_to_adv::<_, C, M>(rng, identity, adv_builder, salt)
}

pub(crate) fn add_mic_with_salt_to_adv<'a, R: rand::Rng, C: CryptoProvider, const M: usize>(
    rng: &mut R,
    identity: &'a TestIdentity,
    adv_builder: &mut AdvBuilder,
    salt: MultiSalt,
) -> Result<crate::tests::deser_v1_tests::SectionConfig<'a>, AddSectionError> {
    let broadcast_cred = identity.broadcast_credential();
    adv_builder
        .section_builder(MicEncryptedSectionEncoder::<_>::new_with_salt::<C>(salt, &broadcast_cred))
        .map(|mut s| {
            let mut sink = Vec::new();
            let (_, des) = fill_section_random_des::<_, _, M>(rng, &mut sink, &mut s);
            s.add_to_advertisement::<C>();
            SectionConfig::new(
                IdentityKind::Encrypted { verification_mode: VerificationMode::Mic, identity },
                des,
            )
        })
}

/// Populate a random number of sections with randomly chosen identities and random DEs
fn fill_with_encrypted_sections<'a, R: rand::Rng, C: CryptoProvider>(
    mut rng: &mut R,
    identities: &'a TestIdentities,
    adv_builder: &mut AdvBuilder,
) -> Vec<SectionConfig<'a>> {
    let mut expected = Vec::new();
    for _ in 0..rng.gen_range(1..=NP_V1_ADV_MAX_SECTION_COUNT) {
        let identity = identities.pick_random_identity(&mut rng);
        let mode: VerificationMode = random();
        let res = match mode {
            VerificationMode::Signature => {
                add_sig_rand_salt_to_adv::<_, C, 0>(&mut rng, identity, adv_builder)
            }
            VerificationMode::Mic => {
                add_mic_rand_salt_to_adv::<_, C, 0>(&mut rng, identity, adv_builder)
            }
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

#[derive(Clone)]
pub(crate) struct TestIdentity {
    key_seed: [u8; 32],
    identity_token: V1IdentityToken,
    private_key: ed25519::PrivateKey,
    plaintext_metadata: Vec<u8>,
}

impl TestIdentity {
    /// Generate a new identity with random crypto material
    fn random<R: rand::Rng, C: CryptoProvider>(rng: &mut R) -> Self {
        Self {
            key_seed: rng.gen(),
            identity_token: rng.gen(),
            private_key: ed25519::PrivateKey::generate::<C::Ed25519>(),
            // varying length vec of random bytes
            plaintext_metadata: (0..rng.gen_range(50..200)).map(|_| rng.gen()).collect(),
        }
    }
    /// Returns a (simple, signed) broadcast credential using crypto material from this identity
    fn broadcast_credential(&self) -> V1BroadcastCredential {
        V1BroadcastCredential::new(self.key_seed, self.identity_token, self.private_key.clone())
    }
    /// Returns a discovery credential using crypto material from this identity
    fn discovery_credential<C: CryptoProvider>(&self) -> V1DiscoveryCredential {
        self.broadcast_credential().derive_discovery_credential::<C>()
    }
}

pub(crate) struct SectionConfig<'a> {
    identity_kind: IdentityKind<'a>,
    data_elements: Vec<GenericDataElement>,
}

pub(crate) enum IdentityKind<'a> {
    Plaintext,
    Encrypted { verification_mode: VerificationMode, identity: &'a TestIdentity },
}

impl<'a> SectionConfig<'a> {
    pub fn new(identity_kind: IdentityKind<'a>, data_elements: Vec<GenericDataElement>) -> Self {
        Self { identity_kind, data_elements }
    }
}

/// Returns the DEs created in both deserialized form ([DataElement]) and
/// input form ([GenericDataElement]) where `M` is the minimum number of DE's
/// generated
fn fill_section_random_des<'adv, R: rand::Rng, I: SectionEncoder, const M: usize>(
    mut rng: &mut R,
    sink: &'adv mut Vec<u8>,
    section_builder: &mut SectionBuilder<&mut AdvBuilder, I>,
) -> (Vec<DataElement<'adv>>, Vec<GenericDataElement>) {
    let mut expected_des = vec![];
    let mut orig_des = vec![];
    let mut de_ranges = vec![];

    for _ in 0..rng.gen_range(M..=5) {
        let de = random_de::<MAX_DE_LEN, _>(&mut rng);

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
        expected_des.push(DataElement::new(
            u8::try_from(index).unwrap().into(),
            de.de_header().de_type,
            &sink[range],
        ));
    }
    (expected_des, orig_des)
}

/// generates a random DE, where `N` is the max length size of the DE
fn random_de<const N: usize, R: rand::Rng>(rng: &mut R) -> GenericDataElement {
    let mut array = [0_u8; MAX_DE_LEN];
    rng.fill(&mut array[..]);
    let data: ArrayView<u8, MAX_DE_LEN> =
        ArrayView::try_from_array(array, rng.gen_range(0..=MAX_DE_LEN)).unwrap();
    // skip the first few DEs that Google uses
    GenericDataElement::try_from(rng.gen_range(0_u32..1000).into(), data.as_slice()).unwrap()
}

#[derive(Clone)]
pub(crate) struct TestIdentities(pub(crate) Vec<TestIdentity>);

impl TestIdentities {
    pub(crate) fn generate<const N: usize, R: rand::Rng, C: CryptoProvider>(
        rng: &mut R,
    ) -> TestIdentities {
        TestIdentities((0..N).map(|_| TestIdentity::random::<_, C>(rng)).collect::<Vec<_>>())
    }

    fn pick_random_identity<R: rand::Rng>(&self, rng: &mut R) -> &TestIdentity {
        let chosen_index = rng.gen_range(0..self.0.len());
        &self.0[chosen_index]
    }

    pub(crate) fn build_cred_book<C: CryptoProvider>(
        &self,
    ) -> PrecalculatedOwnedCredentialBook<MetadataMatchedCredential<Vec<u8>>> {
        let creds = self
            .0
            .iter()
            .map(|identity| {
                let match_data = MetadataMatchedCredential::<Vec<u8>>::encrypt_from_plaintext::<
                    V1,
                    CryptoProviderImpl,
                >(
                    &np_hkdf::NpKeySeedHkdf::new(&identity.key_seed),
                    identity.identity_token,
                    &identity.plaintext_metadata,
                );
                let discovery_credential = identity.discovery_credential::<C>();
                MatchableCredential { discovery_credential, match_data }
            })
            .collect::<Vec<_>>();
        let cred_source = PrecalculatedOwnedCredentialSource::new::<CryptoProviderImpl>(creds);
        PrecalculatedOwnedCredentialBook::new(
            PrecalculatedOwnedCredentialSource::<V0, MetadataMatchedCredential<Vec<u8>>>::new::<
                CryptoProviderImpl,
            >(Vec::new()),
            cred_source,
        )
    }
}

fn deserialize_rand_identities_finds_correct_one<E, F>(
    build_encoder: F,
    expected_mode: VerificationMode,
) where
    E: SectionEncoder,
    F: Fn(&mut <CryptoProviderImpl as CryptoProvider>::CryptoRng, &V1BroadcastCredential) -> E,
{
    let mut rng = StdRng::from_entropy();
    let mut crypto_rng = <CryptoProviderImpl as CryptoProvider>::CryptoRng::new();

    for _ in 0..100 {
        let identities = TestIdentities::generate::<100, _, CryptoProviderImpl>(&mut rng);
        let identity = identities.pick_random_identity(&mut rng);
        let book = identities.build_cred_book::<CryptoProviderImpl>();

        let mut adv_builder = AdvBuilder::new(AdvertisementType::Encrypted);

        let broadcast_cm = identity.broadcast_credential();
        let mut section_builder =
            adv_builder.section_builder(build_encoder(&mut crypto_rng, &broadcast_cm)).unwrap();

        let mut expected_de_data = vec![];
        let (expected_des, orig_des) = fill_section_random_des::<_, _, 0>(
            &mut rng,
            &mut expected_de_data,
            &mut section_builder,
        );
        section_builder.add_to_advertisement::<CryptoProviderImpl>();

        let section_config = SectionConfig::new(
            IdentityKind::Encrypted { verification_mode: expected_mode, identity },
            orig_des.clone(),
        );

        let adv = adv_builder.into_advertisement();

        let arena = deserialization_arena!();
        let decrypted_contents = deser_v1::<_, CryptoProviderImpl>(arena, adv.as_slice(), &book);
        assert_eq!(0, decrypted_contents.invalid_sections_count());
        let sections = decrypted_contents.into_sections();
        assert_eq!(1, sections.len());

        assert_section_equals(&section_config, &sections[0]);

        // verify data elements match original after collecting them from the section
        let data_elements =
            sections[0].as_ciphertext_section().contents().collect_data_elements().unwrap();
        assert_eq!(expected_des, data_elements);
    }
}

fn add_plaintext_section<R: rand::Rng>(
    rng: &mut R,
    builder: &mut AdvBuilder,
) -> Result<(), AddSectionError> {
    builder.section_builder(UnencryptedSectionEncoder).map(|mut sb| {
        // use an upper bound on the De size to leave room for another section
        sb.add_de(|_| random_de::<20, _>(rng)).unwrap();
        sb.add_to_advertisement::<CryptoProviderImpl>();
    })
}

fn append_mock_encrypted_section(adv: &mut Vec<u8>) {
    adv.push(0b0000_0001u8); // format
    adv.extend_from_slice(&[0xAA; 2]); // short salt
    adv.extend_from_slice(&[0xBB; 16]); // identity token
    adv.push(3); // payload length
    adv.extend_from_slice(&[0xCC; 3]); // payload contents
}
