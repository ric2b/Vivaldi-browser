// Copyright 2023 Google LLC
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

//! Provides filtering APIs to determine whether or not a buffer of bytes represents
//! a valid Nearby Presence advertisement and if so which was its corresponding identity
//! it matched with. Used as a first pass option to quickly check if a buffer should
//! further processed.
use crate::credential::matched::MatchedCredential;
use crate::header::{NpVersionHeader, V0Encoding, V1AdvHeader};
use crate::legacy::data_elements::DataElementDeserializeError;
use crate::legacy::deserialize::intermediate::{IntermediateAdvContents, LdtAdvContents};
use crate::legacy::deserialize::DecryptedAdvContents;
use crate::{
    credential::{book::CredentialBook, v0::V0DiscoveryCryptoMaterial},
    legacy::{
        data_elements::actions::{self, ActionsDataElement},
        deserialize::{DecryptError, DeserializedDataElement},
        PacketFlavor,
    },
};
use array_view::ArrayView;
use core::fmt::Debug;
use crypto_provider::CryptoProvider;

#[cfg(test)]
mod tests;

/// The options to filter for in an advertisement payload.
pub enum FilterOptions {
    /// Filter criteria for matching against only v0 advertisements
    V0FilterOptions(V0Filter),
    /// Filter criteria for matching against only v1 advertisements
    V1FilterOptions(V1Filter),
    /// Criteria to filter for in both v0 and v1 advertisements
    Either(V0Filter, V1Filter),
}

/// Error returned when no advertisement matching the criteria was found.
#[derive(PartialEq, Debug)]
pub struct NoMatch;

/// Error returned if the provided slice is of invalid length
#[derive(PartialEq, Debug)]
pub struct InvalidLength;

/// The criteria of what to filter for in a V0 advertisement.
pub struct V0Filter {
    identity: IdentityFilterType,
    data_elements: V0DataElementsFilter,
}

/// The type of identity to filter for.
pub enum IdentityFilterType {
    /// Filter only on public identity advertisements
    Public,
    /// Filter only on private identity advertisements
    Private,
    /// Don't filter on any specific identity type
    Any,
}

/// Returned by the filter API indicating the status and in the case of private identity the key seed
/// which matched the decrypted advertisement
#[derive(Debug, PartialEq)]
pub enum FilterResult<M: MatchedCredential> {
    /// A public identity advertisement matching the filter criteria was detected.
    Public,
    /// A Private identity advertisement matching the filter criteria was detected and decrypted
    /// with the contained `KeySeed`.
    Private(M),
}

/// The criteria of what data elements to filter for in a V0 advertisement.
pub struct V0DataElementsFilter {
    contains_tx_power: Option<bool>,
    actions_filter: Option<V0ActionsFilter>,
}

/// The total number of unique boolean action types
const NUM_ACTIONS: usize = 6;

/// Specify which specific actions bits to filter on, will filter on if any of the specified
/// actions are matched
//TODO: do we need a more specific filter criteria, eg: only return if `(A AND B) || C` are found?
// Potentially this could be pulled out more generally into a V0Filter trait which has many different
// implementations.
pub struct V0ActionsFilter {
    actions: array_view::ArrayView<Option<actions::ActionType>, NUM_ACTIONS>,
}

impl FilterOptions {
    /// WARNING: This does not perform signature verification on the advertisement, even if it detects
    /// a valid advertisement it does not verify it. Verification must occur before further processing
    /// of the adv.
    ///
    /// Returns `Ok` if a advertisement was detected which matched the filter criteria, and `Err` otherwise.
    pub fn match_advertisement<'a, B, P>(
        &self,
        cred_book: &'a B,
        adv: &[u8],
    ) -> Result<FilterResult<B::Matched>, NoMatch>
    where
        B: CredentialBook<'a>,
        P: CryptoProvider,
    {
        NpVersionHeader::parse(adv)
            .map(|(remaining, header)| match header {
                NpVersionHeader::V0(encoding) => {
                    let filter = match self {
                        FilterOptions::V0FilterOptions(filter) => filter,
                        FilterOptions::Either(filter, _) => filter,
                        _ => return Err(NoMatch),
                    };
                    filter.match_v0_adv::<B, P>(encoding, cred_book, remaining)
                }
                NpVersionHeader::V1(header) => {
                    let filter = match self {
                        FilterOptions::V1FilterOptions(filter) => filter,
                        FilterOptions::Either(_, filter) => filter,
                        _ => return Err(NoMatch),
                    };
                    filter.match_v1_adv::<B, P>(cred_book, remaining, header)
                }
            })
            .map_err(|_| NoMatch)?
    }
}

impl V0Filter {
    /// Filter for the provided criteria returning success if the advertisement bytes successfully
    /// match the filter criteria
    fn match_v0_adv<'a, B, P>(
        &self,
        encoding: V0Encoding,
        cred_book: &'a B,
        remaining: &[u8],
    ) -> Result<FilterResult<B::Matched>, NoMatch>
    where
        B: CredentialBook<'a>,
        P: CryptoProvider,
    {
        let contents =
            IntermediateAdvContents::deserialize::<P>(encoding, remaining).map_err(|_| NoMatch)?;
        match contents {
            IntermediateAdvContents::Unencrypted(p) => match self.identity {
                IdentityFilterType::Public | IdentityFilterType::Any => self
                    .data_elements
                    .match_v0_legible_adv(|| p.data_elements())
                    .map(|()| FilterResult::Public),
                _ => Err(NoMatch),
            },
            IntermediateAdvContents::Ldt(c) => match self.identity {
                IdentityFilterType::Private | IdentityFilterType::Any => {
                    let (legible_adv, m) = try_decrypt_and_match::<B, P>(cred_book.v0_iter(), &c)?;
                    self.data_elements
                        .match_v0_legible_adv(|| legible_adv.data_elements())
                        .map(|()| FilterResult::Private(m))
                }
                _ => Err(NoMatch),
            },
        }
    }
}

fn try_decrypt_and_match<'cred, B, P>(
    v0_creds: B::V0Iterator,
    adv: &LdtAdvContents,
) -> Result<(DecryptedAdvContents, B::Matched), NoMatch>
where
    B: CredentialBook<'cred>,
    P: CryptoProvider,
{
    for (crypto_material, m) in v0_creds {
        let ldt = crypto_material.ldt_adv_cipher::<P>();
        match adv.try_decrypt(&ldt) {
            Ok(c) => return Ok((c, m)),
            Err(e) => match e {
                DecryptError::DecryptOrVerifyError => continue,
            },
        }
    }
    Err(NoMatch)
}

impl V0DataElementsFilter {
    /// A legible adv is either plaintext to begin with, or decrypted contents from an encrypted adv
    fn match_v0_legible_adv<F, I>(&self, data_elements: impl Fn() -> I) -> Result<(), NoMatch>
    where
        F: PacketFlavor,
        I: Iterator<Item = Result<DeserializedDataElement<F>, DataElementDeserializeError>>,
    {
        match &self.contains_tx_power {
            None => Ok(()),
            Some(c) => {
                if c == &data_elements()
                    .any(|de| matches!(de, Ok(DeserializedDataElement::TxPower(_))))
                {
                    Ok(())
                } else {
                    Err(NoMatch)
                }
            }
        }?;

        match &self.actions_filter {
            None => Ok(()),
            Some(filter) => {
                if let Some(_err) = data_elements().find_map(|result| result.err()) {
                    return Err(NoMatch);
                }
                // find if an actions DE exists, if so match on the provided action filter
                let actions = data_elements().find_map(|de| match de {
                    Ok(DeserializedDataElement::Actions(actions)) => Some(actions),
                    _ => None,
                });
                if let Some(actions) = actions {
                    filter.match_v0_actions(&actions)
                } else {
                    return Err(NoMatch);
                }
            }
        }?;

        Ok(())
    }
}

impl V0ActionsFilter {
    /// Creates a new filter from a slice of Actions to look for. Will succeed if any of the provided
    /// actions are found. Maximum length of `actions` is 7, as there are 7 unique actions possible to
    /// filter on for V0.
    pub fn new_from_slice(actions: &[actions::ActionType]) -> Result<Self, InvalidLength> {
        if actions.len() > NUM_ACTIONS {
            return Err(InvalidLength);
        }
        let mut filter_actions = [None; NUM_ACTIONS];
        for (i, action) in actions.iter().map(|action| Some(*action)).enumerate() {
            // i will always be in bounds because the length is checked above
            filter_actions[i] = action;
        }
        Ok(ArrayView::try_from_array(filter_actions, actions.len())
            .map(|actions| Self { actions })
            .expect("Length checked above, so this can't happen"))
    }

    fn match_v0_actions<F: PacketFlavor>(
        &self,
        actions: &ActionsDataElement<F>,
    ) -> Result<(), NoMatch> {
        for action in self.actions.as_slice().iter() {
            if actions.action.has_action(action.expect("This will always contain Some")) {
                return Ok(());
            }
        }
        Err(NoMatch)
    }
}

/// The criteria of what to filter for in a V1 advertisement.
pub struct V1Filter;

impl V1Filter {
    /// Filter for the provided criteria returning success if the advertisement bytes successfully
    /// match the filter criteria
    #[allow(clippy::extra_unused_type_parameters)]
    fn match_v1_adv<'a, B, P>(
        &self,
        _cred_book: &'a B,
        _remaining: &[u8],
        _header: V1AdvHeader,
    ) -> Result<FilterResult<B::Matched>, NoMatch>
    where
        B: CredentialBook<'a>,
        P: CryptoProvider,
    {
        todo!()
    }
}
