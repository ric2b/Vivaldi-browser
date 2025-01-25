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

//! Covers the first half of section parsing before decryption, if relevant, is
//! attempted.

use crate::{
    array_vec::ArrayVecOption,
    extended::{
        deserialize::{
            encrypted_section::{
                EncryptedSectionContents, MicEncryptedSection, SectionIdentityResolutionContents,
                SignatureEncryptedSection,
            },
            section::header::{
                CiphertextExtendedIdentityToken, EncryptedSectionHeader, SectionHeader,
            },
            DataElementParsingIterator, Section, SectionMic,
        },
        salt::MultiSalt,
        NP_V1_ADV_MAX_SECTION_COUNT,
    },
    header::V1AdvHeader,
};
use crypto_provider::CryptoProvider;
use nom::{branch, bytes, combinator, error, multi};

#[cfg(feature = "devtools")]
use crate::{
    credential::v1::V1DiscoveryCryptoMaterial, deserialization_arena::DeserializationArenaAllocator,
};
#[cfg(feature = "devtools")]
use crate::{deserialization_arena::ArenaOutOfSpace, extended::NP_ADV_MAX_SECTION_LEN};
#[cfg(feature = "devtools")]
use array_view::ArrayView;

#[cfg(test)]
pub(crate) mod tests;

/// Parse into [IntermediateSection]s, exposing the underlying parsing errors.
/// Consumes all of `adv_body`.
pub(crate) fn parse_sections(
    adv_header: V1AdvHeader,
    adv_body: &[u8],
) -> Result<
    ArrayVecOption<IntermediateSection, NP_V1_ADV_MAX_SECTION_COUNT>,
    nom::Err<error::Error<&[u8]>>,
> {
    combinator::all_consuming(branch::alt((
        // Public advertisement
        multi::fold_many_m_n(
            1,
            NP_V1_ADV_MAX_SECTION_COUNT,
            IntermediateSection::parser_unencrypted_section,
            ArrayVecOption::default,
            |mut acc, item| {
                acc.push(item);
                acc
            },
        ),
        // Encrypted advertisement
        multi::fold_many_m_n(
            1,
            NP_V1_ADV_MAX_SECTION_COUNT,
            IntermediateSection::parser_encrypted_with_header(adv_header),
            ArrayVecOption::default,
            |mut acc, item| {
                acc.push(item);
                acc
            },
        ),
    )))(adv_body)
    .map(|(_rem, sections)| sections)
}

/// A partially processed section that hasn't been decrypted (if applicable) yet.
#[derive(PartialEq, Eq, Debug)]
pub(crate) enum IntermediateSection<'a> {
    /// A section that was not encrypted, e.g. a public identity or no-identity section.
    Plaintext(PlaintextSection<'a>),
    /// A section whose contents were encrypted, e.g. a private identity section.
    Ciphertext(CiphertextSection<'a>),
}

impl<'a> IntermediateSection<'a> {
    fn parser_unencrypted_section(
        np_adv_body: &'a [u8],
    ) -> nom::IResult<&'a [u8], IntermediateSection<'a>> {
        combinator::map_opt(SectionContents::parse, |sc| match sc.header {
            SectionHeader::Unencrypted => {
                Some(IntermediateSection::Plaintext(PlaintextSection::new(sc.contents)))
            }
            SectionHeader::Encrypted(_) => None,
        })(np_adv_body)
    }

    pub(crate) fn parser_encrypted_with_header(
        adv_header: V1AdvHeader,
    ) -> impl Fn(&'a [u8]) -> nom::IResult<&[u8], IntermediateSection> {
        move |adv_body| {
            fn split_at_mic(contents: &[u8]) -> Option<(&[u8], SectionMic)> {
                contents.len().checked_sub(SectionMic::CONTENTS_LEN).map(|len_before_mic| {
                    let (before_mic, mic) = contents.split_at(len_before_mic);
                    let mic = SectionMic::try_from(mic).expect("MIC length checked above");

                    (before_mic, mic)
                })
            }

            fn build_mic_section<'a>(
                adv_header: V1AdvHeader,
                format_bytes: &'a [u8],
                salt: MultiSalt,
                token: CiphertextExtendedIdentityToken,
                contents_len: u8,
                contents: &'a [u8],
            ) -> Option<IntermediateSection<'a>> {
                split_at_mic(contents).map(|(before_mic, mic)| {
                    IntermediateSection::Ciphertext(CiphertextSection::MicEncrypted(
                        MicEncryptedSection {
                            contents: EncryptedSectionContents::new(
                                adv_header,
                                format_bytes,
                                salt,
                                token,
                                contents_len,
                                before_mic,
                            ),
                            mic,
                        },
                    ))
                })
            }

            combinator::map_opt(
                combinator::map_opt(SectionContents::parse, |sc| match sc.header {
                    SectionHeader::Unencrypted => None,
                    SectionHeader::Encrypted(e) => {
                        Some((sc.format_bytes, sc.contents, sc.contents_len, e))
                    }
                }),
                move |(format_bytes, contents, contents_len, header)| match header {
                    EncryptedSectionHeader::MicShortSalt { salt, token } => build_mic_section(
                        adv_header,
                        format_bytes,
                        salt.into(),
                        token,
                        contents_len,
                        contents,
                    ),
                    EncryptedSectionHeader::MicExtendedSalt { salt, token } => build_mic_section(
                        adv_header,
                        format_bytes,
                        salt.into(),
                        token,
                        contents_len,
                        contents,
                    ),
                    EncryptedSectionHeader::SigExtendedSalt { salt, token } => {
                        Some(IntermediateSection::Ciphertext(
                            CiphertextSection::SignatureEncrypted(SignatureEncryptedSection {
                                contents: EncryptedSectionContents::new(
                                    adv_header,
                                    format_bytes,
                                    salt,
                                    token,
                                    contents_len,
                                    contents,
                                ),
                            }),
                        ))
                    }
                },
            )(adv_body)
        }
    }
}

/// Components of a section after header decode, but before decryption or DE parsing.
///
/// This is just the first stage of parsing sections, followed by [IntermediateSection].
#[derive(PartialEq, Eq, Debug)]
pub(crate) struct SectionContents<'adv> {
    /// 1-2 bytes of the format saved for later use in verification
    pub(crate) format_bytes: &'adv [u8],
    /// Section header contents which includes salt + identity token
    pub(crate) header: SectionHeader,
    /// Contents of the section after the header.
    /// No validation is performed on the contents.
    pub(crate) contents: &'adv [u8],
    /// The length of the contents stored as an u8.
    pub(crate) contents_len: u8,
}

impl<'adv> SectionContents<'adv> {
    pub(crate) fn parse(input: &'adv [u8]) -> nom::IResult<&'adv [u8], Self> {
        let (input, (format_bytes, header, contents_len)) = SectionHeader::parse(input)?;
        let (input, contents) = bytes::complete::take(contents_len)(input)?;

        Ok((input, Self { format_bytes, header, contents, contents_len }))
    }
}

/// A plaintext section deserialized from a V1 advertisement.
#[derive(PartialEq, Eq, Debug)]
pub struct PlaintextSection<'adv> {
    contents: &'adv [u8],
}

impl<'adv> PlaintextSection<'adv> {
    pub(crate) fn new(contents: &'adv [u8]) -> Self {
        Self { contents }
    }
}

impl<'adv> Section<'adv> for PlaintextSection<'adv> {
    type Iterator = DataElementParsingIterator<'adv>;

    fn iter_data_elements(&self) -> Self::Iterator {
        DataElementParsingIterator::new(self.contents)
    }
}

#[derive(PartialEq, Eq, Debug)]
pub(crate) enum CiphertextSection<'a> {
    SignatureEncrypted(SignatureEncryptedSection<'a>),
    MicEncrypted(MicEncryptedSection<'a>),
}

impl<'a> CiphertextSection<'a> {
    /// Try decrypting into some raw bytes given some raw unsigned crypto-material.
    #[cfg(feature = "devtools")]
    pub(crate) fn try_resolve_identity_and_decrypt<
        C: V1DiscoveryCryptoMaterial,
        P: CryptoProvider,
    >(
        &self,
        allocator: &mut DeserializationArenaAllocator<'a>,
        crypto_material: &C,
    ) -> Option<Result<ArrayView<u8, { NP_ADV_MAX_SECTION_LEN }>, ArenaOutOfSpace>> {
        match self {
            CiphertextSection::SignatureEncrypted(x) => {
                let identity_resolution_material =
                    crypto_material.signed_identity_resolution_material::<P>();
                x.try_resolve_identity_and_decrypt::<P>(allocator, &identity_resolution_material)
            }
            CiphertextSection::MicEncrypted(x) => match x.contents.salt {
                MultiSalt::Short(_) => x.try_resolve_short_salt_identity_and_decrypt::<P>(
                    allocator,
                    &crypto_material.mic_short_salt_identity_resolution_material::<P>(),
                ),
                MultiSalt::Extended(_) => x.try_resolve_extended_salt_identity_and_decrypt::<P>(
                    allocator,
                    &crypto_material.mic_extended_salt_identity_resolution_material::<P>(),
                ),
            },
        }
    }

    /// Return the data needed to resolve identities.
    ///
    /// In the typical case of trying many identities across a few sections,
    /// these should be calculated once for all relevant sections, then re-used
    /// for all identity match attempts.
    pub(crate) fn identity_resolution_contents<C: CryptoProvider>(
        &self,
    ) -> SectionIdentityResolutionContents {
        match self {
            CiphertextSection::SignatureEncrypted(x) => {
                x.contents.compute_identity_resolution_contents::<C>()
            }
            CiphertextSection::MicEncrypted(x) => {
                x.contents.compute_identity_resolution_contents::<C>()
            }
        }
    }
}
