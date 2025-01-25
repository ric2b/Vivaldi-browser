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

//! The first stage of deserialization: just header data (if any) and the bulk contents of the
//! advertisement, with no disaggregation into individual data elements.

use crate::header::V0Encoding;
use crate::helpers::parse_byte_array;
use crate::legacy::deserialize::{
    AdvDeserializeError, DeIterator, DecryptError, DecryptedAdvContents,
};
#[cfg(test)]
use crate::legacy::deserialize::{DataElementDeserializer, GenericDeIterator};
use crate::legacy::{Plaintext, NP_MIN_ADV_CONTENT_LEN};
use crypto_provider::CryptoProvider;
use ldt_np_adv::V0Salt;
use nom::combinator;

#[cfg(test)]
mod tests;

/// The header components, if any, and the bytes that will later be decrypted and/or parsed into DEs.
#[derive(Debug, PartialEq, Eq)]
pub(crate) enum IntermediateAdvContents<'d> {
    /// Plaintext advertisements
    Unencrypted(UnencryptedAdvContents<'d>),
    /// Ciphertext advertisements
    Ldt(LdtAdvContents<'d>),
}

impl<'d> IntermediateAdvContents<'d> {
    #[cfg(test)]
    pub(crate) fn as_unencrypted(&self) -> Option<&UnencryptedAdvContents<'d>> {
        match self {
            IntermediateAdvContents::Unencrypted(c) => Some(c),
            IntermediateAdvContents::Ldt(_) => None,
        }
    }

    #[cfg(test)]
    pub(crate) fn as_ldt(&self) -> Option<&LdtAdvContents<'d>> {
        match self {
            IntermediateAdvContents::Unencrypted(_) => None,
            IntermediateAdvContents::Ldt(c) => Some(c),
        }
    }

    /// Performs basic structural checks on header and content, but doesn't deserialize DEs or
    /// decrypt.
    pub(crate) fn deserialize<C: CryptoProvider>(
        encoding: V0Encoding,
        input: &[u8],
    ) -> Result<IntermediateAdvContents<'_>, AdvDeserializeError> {
        match encoding {
            V0Encoding::Unencrypted => {
                if input.len() < NP_MIN_ADV_CONTENT_LEN {
                    return Err(AdvDeserializeError::NoDataElements);
                }
                Ok(IntermediateAdvContents::Unencrypted(UnencryptedAdvContents { data: input }))
            }
            V0Encoding::Ldt => {
                let (ciphertext, salt) =
                    parse_v0_salt(input).map_err(|_| AdvDeserializeError::InvalidStructure)?;
                LdtAdvContents::new::<C>(salt, ciphertext)
                    .ok_or(AdvDeserializeError::InvalidStructure)
                    .map(IntermediateAdvContents::Ldt)
            }
        }
    }
}

/// The contents of a plaintext advertisement.
#[derive(Debug, PartialEq, Eq)]
pub struct UnencryptedAdvContents<'d> {
    /// Contents of the advertisement after the version header.
    ///
    /// Contents are at least [NP_MIN_ADV_CONTENT_LEN].
    data: &'d [u8],
}

impl<'d> UnencryptedAdvContents<'d> {
    /// Returns an iterator over the v0 data elements
    pub fn data_elements(&self) -> DeIterator<'d, Plaintext> {
        DeIterator::new(self.data)
    }

    #[cfg(test)]
    pub(in crate::legacy) fn generic_data_elements<D: DataElementDeserializer>(
        &self,
    ) -> GenericDeIterator<Plaintext, D> {
        GenericDeIterator::new(self.data)
    }
}

/// Contents of an encrypted advertisement before decryption.
#[derive(Debug, PartialEq, Eq)]
pub(crate) struct LdtAdvContents<'d> {
    /// Salt from the advertisement, converted into a padder.
    /// Pre-calculated so it's only derived once across multiple decrypt attempts.
    salt_padder: ldt::XorPadder<{ crypto_provider::aes::BLOCK_SIZE }>,
    /// The salt instance used for encryption of this advertisement.
    salt: V0Salt,
    /// Ciphertext containing the identity token and any data elements.
    /// Must be a valid length for LDT.
    ciphertext: &'d [u8],
}

impl<'d> LdtAdvContents<'d> {
    /// Returns `None` if `ciphertext` is not a valid LDT-XTS-AES ciphertext length.
    pub(crate) fn new<C: CryptoProvider>(salt: V0Salt, ciphertext: &'d [u8]) -> Option<Self> {
        if !ldt_np_adv::VALID_INPUT_LEN.contains(&ciphertext.len()) {
            return None;
        }
        Some(Self { salt_padder: ldt_np_adv::salt_padder::<C>(salt), salt, ciphertext })
    }

    /// Try decrypting with an identity's LDT cipher and deserializing the resulting data elements.
    ///
    /// Returns the decrypted data if decryption and verification succeeded and the resulting DEs could be parsed
    /// successfully, otherwise `Err`.
    pub(crate) fn try_decrypt<C: CryptoProvider>(
        &self,
        cipher: &ldt_np_adv::AuthenticatedNpLdtDecryptCipher<C>,
    ) -> Result<DecryptedAdvContents, DecryptError> {
        let (identity_token, plaintext) = cipher
            .decrypt_and_verify(self.ciphertext, &self.salt_padder)
            .map_err(|_e| DecryptError::DecryptOrVerifyError)?;

        Ok(DecryptedAdvContents::new(identity_token, self.salt, plaintext))
    }
}

fn parse_v0_salt(input: &[u8]) -> nom::IResult<&[u8], V0Salt> {
    combinator::map(parse_byte_array::<2>, V0Salt::from)(input)
}
