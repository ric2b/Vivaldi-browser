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

//! Definitions of traits for structures which supply
//! credentials for discovering advertisements/advertisement
//! sections for a _particular_ protocol version.

use crate::credential::matched::{MatchedCredential, ReferencedMatchedCredential};
use crate::credential::{DiscoveryMetadataCryptoMaterial, MatchableCredential, ProtocolVersion};

/// A source of credentials for a particular protocol version,
/// utilizing any [`DiscoveryMetadataCryptoMaterial`] which is usable
/// for discovering advertisements in that protocol version.
///
/// This trait is largely leveraged as a tool for building
/// new kinds of [`crate::credential::book::CredentialBook`]s
/// via the [`crate::credential::book::CredentialBookFromSources`]
/// wrapper.
///
/// See [`crate::credential::book::CachedCredentialSource`]
/// for an example of this pattern.
pub trait CredentialSource<'a, V: ProtocolVersion>
where
    Self: 'a,
{
    /// The kind of data yielded to the caller upon a successful
    /// identity-match.
    type Matched: MatchedCredential;

    /// The kind of crypto-material yielded from the wrapped
    /// iterator.
    type Crypto: DiscoveryMetadataCryptoMaterial<V>;

    /// The iterator type produced which emits credentials.
    /// This is a lending iterator which may borrow things from `self`.
    type Iterator: Iterator<Item = (Self::Crypto, Self::Matched)>;

    /// Iterate over the available credentials
    fn iter(&'a self) -> Self::Iterator;
}

/// A simple [`CredentialSource`] which iterates over a provided slice of credentials
pub struct SliceCredentialSource<'c, V: ProtocolVersion, M: MatchedCredential> {
    credentials: &'c [MatchableCredential<V, M>],
}

impl<'c, V: ProtocolVersion, M: MatchedCredential> SliceCredentialSource<'c, V, M> {
    /// Construct the credential supplier from the provided slice of credentials.
    pub fn new(credentials: &'c [MatchableCredential<V, M>]) -> Self {
        Self { credentials }
    }
}

impl<'a, 'b, V: ProtocolVersion, M: MatchedCredential> CredentialSource<'a, V>
    for SliceCredentialSource<'b, V, M>
where
    'b: 'a,
    Self: 'b,
    &'a <V as ProtocolVersion>::DiscoveryCredential: DiscoveryMetadataCryptoMaterial<V>,
{
    type Matched = ReferencedMatchedCredential<'a, M>;
    type Crypto = &'a V::DiscoveryCredential;
    type Iterator = core::iter::Map<
        core::slice::Iter<'a, MatchableCredential<V, M>>,
        fn(
            &'a MatchableCredential<V, M>,
        ) -> (&'a V::DiscoveryCredential, ReferencedMatchedCredential<M>),
    >;

    fn iter(&'a self) -> Self::Iterator {
        self.credentials.iter().map(MatchableCredential::<V, M>::as_pair)
    }
}
