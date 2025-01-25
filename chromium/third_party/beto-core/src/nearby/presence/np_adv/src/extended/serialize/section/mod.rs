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

#[cfg(feature = "std")]
extern crate std;

use array_view::ArrayView;
use core::{convert, fmt};
use crypto_provider::CryptoProvider;
use np_hkdf::v1_salt::DataElementOffset;
use sink::Sink as _;

use crate::extended::{
    serialize::{
        section::encoder::SectionEncoder, AdvBuilder, CapacityLimitedVec, WriteDataElement,
    },
    to_array_view, NP_ADV_MAX_SECTION_LEN,
};

pub(crate) mod encoder;
pub(crate) mod header;

/// Accumulates data elements and encodes them into a section.
#[derive(Debug)]
pub struct SectionBuilder<R: AsMut<AdvBuilder>, SE: SectionEncoder> {
    /// The length of the header produced by `section_encoder`
    pub(crate) header_len: usize,
    /// Contains the section header, the identity-specified overhead, and any DEs added
    pub(crate) section: CapacityLimitedVec<u8, { NP_ADV_MAX_SECTION_LEN }>,
    pub(crate) section_encoder: SE,
    /// mut ref-able to enforce only one active section builder at a time
    pub(crate) adv_builder: R,
    next_de_offset: DataElementOffset,
}

impl<'a, SE: SectionEncoder> SectionBuilder<&'a mut AdvBuilder, SE> {
    /// Add this builder to the advertisement that created it.
    pub fn add_to_advertisement<C: CryptoProvider>(self) {
        let _ = self.add_to_advertisement_internal::<C>();
    }
}

impl<SE: SectionEncoder> SectionBuilder<AdvBuilder, SE> {
    /// Gets the 0-based index of the section currently under construction
    /// in the context of the containing advertisement.
    pub fn section_index(&self) -> usize {
        self.adv_builder.section_count()
    }
    /// Add this builder to the advertisement that created it,
    /// and returns the containing advertisement back to the caller.
    pub fn add_to_advertisement<C: CryptoProvider>(self) -> AdvBuilder {
        self.add_to_advertisement_internal::<C>()
    }
}

impl<R: AsMut<AdvBuilder>, SE: SectionEncoder> SectionBuilder<R, SE> {
    pub(crate) fn new(
        header_len: usize,
        section: CapacityLimitedVec<u8, NP_ADV_MAX_SECTION_LEN>,
        section_encoder: SE,
        adv_builder: R,
    ) -> Self {
        Self {
            header_len,
            section,
            section_encoder,
            adv_builder,
            next_de_offset: DataElementOffset::ZERO,
        }
    }

    /// Add this builder to the advertisement that created it.
    /// Returns the mut-refable to the advertisement builder
    /// which the contents of this section builder were added to.
    //TODO: make this fallible, if the section being added is invalid, right now it is possible to
    // create invalid adv's that don't parse
    fn add_to_advertisement_internal<C: CryptoProvider>(mut self) -> R {
        let adv_builder = self.adv_builder.as_mut();
        adv_builder.add_section(Self::build_section::<C>(
            self.header_len,
            self.section.into_inner(),
            self.section_encoder,
        ));
        self.adv_builder
    }

    /// Gets the derived salt which will be employed for the next DE offset.
    ///
    /// Suitable for scenarios (like FFI) where a closure would be inappropriate
    /// for DE construction, and interaction with the client is preferred.
    pub fn next_de_salt(&self) -> SE::DerivedSalt {
        self.section_encoder.de_salt(self.next_de_offset)
    }

    /// Add a data element to the section with a closure that returns a `Result`.
    ///
    /// The provided `build_de` closure will be invoked with the derived salt for this DE.
    pub fn add_de_res<W: WriteDataElement, E, F: FnOnce(SE::DerivedSalt) -> Result<W, E>>(
        &mut self,
        build_de: F,
    ) -> Result<(), AddDataElementError<E>> {
        let writer = build_de(self.next_de_salt()).map_err(AddDataElementError::BuildDeError)?;

        let orig_len = self.section.len();
        // since we own the writer, and it's immutable, no race risk writing header w/ len then
        // the contents as long as it's not simply an incorrect impl
        let de_header = writer.de_header();
        let content_len = self
            .section
            .try_extend_from_slice(de_header.serialize().as_slice())
            .ok_or(AddDataElementError::InsufficientSectionSpace)
            .and_then(|_| {
                let after_header_len = self.section.len();
                writer
                    .write_de_contents(&mut self.section)
                    .ok_or(AddDataElementError::InsufficientSectionSpace)
                    .map(|_| self.section.len() - after_header_len)
            })
            .map_err(|e| {
                // if anything went wrong, truncate any partial writes (e.g. just the header)
                self.section.truncate(orig_len);
                e
            })?;

        if content_len != usize::from(de_header.len.as_u8()) {
            // TODO eliminate this possibility by keeping a 127-byte buffer
            // to write DEs into, then calculating the written length, so the
            // DE impl doesn't have to do it
            panic!(
                "Buggy WriteDataElement impl: header len {}, actual written len {}",
                de_header.len.as_u8(),
                content_len
            );
        }

        self.next_de_offset = self.next_de_offset.incremented();

        Ok(())
    }

    /// Add a data element to the section with a closure that returns the data element directly.
    ///
    /// The provided `build_de` closure will be invoked with the derived salt for this DE.
    pub fn add_de<W: WriteDataElement, F: FnOnce(SE::DerivedSalt) -> W>(
        &mut self,
        build_de: F,
    ) -> Result<(), AddDataElementError<convert::Infallible>> {
        self.add_de_res(|derived_salt| Ok::<_, convert::Infallible>(build_de(derived_salt)))
    }

    /// Convert a section builder's contents into an encoded section.
    ///
    /// `section_contents` must have size > 0.
    ///
    /// `header_len` is the length of the prefix of `section_contents` that has been populated
    /// with the data returned from [SectionEncoder::header()] which does NOT include the length byte.
    ///
    /// Implemented without self to avoid partial-move issues.
    pub(crate) fn build_section<C: CryptoProvider>(
        header_len: usize,
        mut section_contents: tinyvec::ArrayVec<[u8; NP_ADV_MAX_SECTION_LEN]>,
        mut section_encoder: SE,
    ) -> EncodedSection {
        // there is space because the capacity for DEs was restricted to allow it
        section_contents.resize(section_contents.len() + SE::SUFFIX_LEN, 0);

        let (format_and_salt_and_identity_token, rest_of_contents) =
            section_contents.split_at_mut(header_len);

        let (section_length_byte, rest_of_contents) = rest_of_contents.split_at_mut(1);

        let section_len = rest_of_contents.len().try_into().expect(
            "section length will always fit into a u8 and has been validated by the section builder",
        );
        // set the section length byte
        section_length_byte[0] = section_len;
        section_encoder.postprocess::<C>(
            format_and_salt_and_identity_token,
            section_len,
            rest_of_contents,
        );

        to_array_view(section_contents)
    }
}

/// Errors for adding a DE to a section
#[derive(Debug, PartialEq, Eq)]
pub enum AddDataElementError<E> {
    /// An error occurred when invoking the DE builder closure.
    BuildDeError(E),
    /// Too much data to fit into the section
    InsufficientSectionSpace,
}

impl<E: fmt::Display> fmt::Display for AddDataElementError<E> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            AddDataElementError::BuildDeError(e) => write!(f, "Build DE error: {}", e),
            AddDataElementError::InsufficientSectionSpace => {
                write!(f, "Insufficient section space")
            }
        }
    }
}

#[cfg(feature = "std")]
impl<E: fmt::Debug + fmt::Display> std::error::Error for AddDataElementError<E> {}

/// The encoded form of an advertisement section
pub(crate) type EncodedSection = ArrayView<u8, NP_ADV_MAX_SECTION_LEN>;
