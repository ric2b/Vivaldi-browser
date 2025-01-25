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

use core::fmt::Debug;

use crate::deserialization_arena::DeserializationArena;
use array_view::ArrayView;
use crypto_provider::CryptoProvider;

use crate::extended::deserialize::section::intermediate::{
    parse_sections, CiphertextSection, IntermediateSection,
};
use crate::extended::NP_ADV_MAX_SECTION_LEN;
use crate::{credential::book::CredentialBook, header::V1AdvHeader};

/// Error in decryption operations for `deser_decrypt_v1_section_bytes_for_dev_tools`.
#[derive(Debug, Clone)]
pub enum AdvDecryptionError {
    /// Cannot decrypt because the input section is not encrypted.
    InputNotEncrypted,
    /// Error parsing the given section.
    ParseError,
    /// No suitable credential found to decrypt the given section.
    NoMatchingCredentials,
}

/// The encryption scheme used for a V1 advertisement.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum V1EncryptionScheme {
    /// Indicates MIC-based encryption and verification.
    Mic,
    /// Indicates signature-based encryption and verification.
    Signature,
}

/// Decrypt, but do not further deserialize the v1 bytes, intended for developer tooling uses only.
/// Production uses should use [crate::deserialize_advertisement] instead, which deserializes to a
/// structured format and provides extra type safety.
pub fn deser_decrypt_v1_section_bytes_for_dev_tools<'adv, 'cred, B, P>(
    arena: DeserializationArena<'adv>,
    cred_book: &'cred B,
    header_byte: u8,
    section_bytes: &'adv [u8],
) -> Result<(ArrayView<u8, NP_ADV_MAX_SECTION_LEN>, V1EncryptionScheme), AdvDecryptionError>
where
    B: CredentialBook<'cred>,
    P: CryptoProvider,
{
    let header = V1AdvHeader::new(header_byte);
    let int_sections =
        parse_sections(header, section_bytes).map_err(|_| AdvDecryptionError::ParseError)?;
    let cipher_section = match &int_sections[0] {
        IntermediateSection::Plaintext(_) => Err(AdvDecryptionError::InputNotEncrypted)?,
        IntermediateSection::Ciphertext(section) => section,
    };

    let mut allocator = arena.into_allocator();
    for (crypto_material, _) in cred_book.v1_iter() {
        if let Some(plaintext) = cipher_section
            .try_resolve_identity_and_decrypt::<_, P>(&mut allocator, &crypto_material)
        {
            let pt = plaintext.expect(concat!(
                "Should not run out of space because DeserializationArenaAllocator is big ",
                "enough to hold a single advertisement, and we exit immediately upon ",
                "successful decryption",
            ));

            let encryption_scheme = match cipher_section {
                CiphertextSection::SignatureEncrypted(_) => V1EncryptionScheme::Signature,
                CiphertextSection::MicEncrypted(_) => V1EncryptionScheme::Mic,
            };
            return Ok((pt, encryption_scheme));
        }
    }
    Err(AdvDecryptionError::NoMatchingCredentials)
}
