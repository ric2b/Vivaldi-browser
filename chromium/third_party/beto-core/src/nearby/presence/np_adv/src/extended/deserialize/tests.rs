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

//! Unit tests for top level credential iteration and adv deserialization

#![allow(clippy::unwrap_used)]

use crate::{
    credential::book::CredentialBook,
    deserialization_arena,
    deserialization_arena::ArenaOutOfSpace,
    extended::serialize::{AddSectionError, AdvBuilder, AdvertisementType},
    header::NpVersionHeader,
    tests::deser_v1_tests::{
        add_mic_rand_salt_to_adv, add_sig_rand_salt_to_adv, SectionConfig, TestIdentity,
    },
};
use crypto_provider_default::CryptoProviderImpl;
use rand::{prelude::StdRng, seq::IteratorRandom, SeedableRng};

#[test]
fn v1_arena_out_of_space_error_sig() {
    v1_arena_out_of_space_error_encrypted_adv(
        // Need to use many DE's to be sure we go over the arena limit
        add_sig_rand_salt_to_adv::<StdRng, CryptoProviderImpl, 5>,
    )
}

#[test]
fn v1_arena_out_of_space_error_mic() {
    v1_arena_out_of_space_error_encrypted_adv(
        // Need to use many DE's to be sure we go over the arena limit
        add_mic_rand_salt_to_adv::<StdRng, CryptoProviderImpl, 5>,
    )
}

fn v1_arena_out_of_space_error_encrypted_adv(
    add_to_adv: impl for<'a> Fn(
        &mut StdRng,
        &'a TestIdentity,
        &mut AdvBuilder,
    ) -> Result<SectionConfig<'a>, AddSectionError>,
) {
    let mut rng = StdRng::from_entropy();
    let mut builder = AdvBuilder::new(AdvertisementType::Encrypted);
    let identities =
        crate::tests::deser_v1_tests::TestIdentities::generate::<1, _, CryptoProviderImpl>(
            &mut rng,
        );
    let _ = add_to_adv(&mut rng, &identities.0[0], &mut builder);
    let adv = builder.into_advertisement().as_slice().to_vec();
    let cred_book = identities.build_cred_book::<CryptoProviderImpl>();

    let (remaining, header) = NpVersionHeader::parse(&adv).unwrap();

    let h =
        if let NpVersionHeader::V1(v1_header) = header { Some(v1_header) } else { None }.unwrap();

    let mut sections_in_processing =
        crate::extended::deserialize::SectionsInProcessing::<'_, _>::from_advertisement_contents::<
            CryptoProviderImpl,
        >(h, remaining)
        .unwrap();

    // fill up allocator so we will run out of space
    let arena = deserialization_arena!();
    let mut allocator = arena.into_allocator();
    let _ = allocator.allocate(250).unwrap();

    let (crypto_material, match_data) = cred_book.v1_iter().choose(&mut rng).unwrap();

    let res = sections_in_processing.try_decrypt_with_credential::<_, CryptoProviderImpl>(
        &mut allocator,
        crypto_material,
        match_data,
    );
    assert_eq!(Err(ArenaOutOfSpace), res);
}

mod coverage_gaming {
    use crate::{
        array_vec::ArrayVecOption,
        credential::matched::EmptyMatchedCredential,
        extended::{
            deserialize::{
                section::intermediate::PlaintextSection, DecryptedSection, SectionDeserializeError,
                V1AdvertisementContents, V1DeserializedSection, VerificationMode,
            },
            salt::MultiSalt,
        },
    };
    use alloc::format;

    #[test]
    fn decrypted_section_derives() {
        let d = DecryptedSection::new(
            VerificationMode::Mic,
            MultiSalt::Extended([0u8; 16].into()),
            [0u8; 16].into(),
            &[],
        );
        let _ = format!("{:?}", d);
    }

    #[test]
    fn section_deserialize_error_derives() {
        let e = SectionDeserializeError::IncorrectCredential;
        assert_eq!(e, e);
        let _ = format!("{:?}", e);
    }

    #[test]
    fn adv_contents_derives() {
        let c: V1AdvertisementContents<'_, EmptyMatchedCredential> =
            V1AdvertisementContents::new(ArrayVecOption::default(), 0);
        let _ = format!("{:?}", c);
        assert_eq!(c, c);
    }

    #[test]
    fn deserialized_section_derives() {
        let d: V1DeserializedSection<'_, EmptyMatchedCredential> =
            V1DeserializedSection::Plaintext(PlaintextSection::new(&[]));
        assert_eq!(d, d);
        let _ = format!("{:?}", d);
    }

    #[test]
    fn verification_mode_derives() {
        let m = VerificationMode::Signature;
        #[allow(clippy::clone_on_copy)]
        let _ = format!("{:?}", m.clone());
    }
}
