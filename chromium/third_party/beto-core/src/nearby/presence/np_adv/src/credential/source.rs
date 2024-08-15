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

use crate::credential::{
    DiscoveryCryptoMaterial, MatchableCredential, MatchedCredential, ProtocolVersion,
    ReferencedMatchedCredential,
};
use core::borrow::Borrow;

/// Specialized version of the [`CredentialSource`] trait for
/// credential-sources which provide discovery credentials
/// for a specific protocol version.
///
/// If you want ready-made structures which can provide
/// credentials for both V0 and V1 protocol versions,
/// see [`crate::credential::book::CredentialBook`]
/// and [`crate::credential::book::CredentialBookBuilder`] instead.
///
/// It's preferred to use this kind of credential-source
/// in client code, if possible, and then lift to a
/// [`CredentialSource`] using [`AsCredentialSource`]
/// instead of implementing [`CredentialSource`] directly,
/// since it's better to trust this crate to handle
/// the details of what's in [`DiscoveryCryptoMaterial`]s
/// for specific protocol versions.
pub trait DiscoveryCredentialSource<'a, V: ProtocolVersion>
where
    Self: 'a,
{
    /// The kind of data yielded to the caller upon a successful
    /// identity-match.
    type Matched: MatchedCredential;

    /// The kind of crypto-material yielded from the wrapped
    /// iterator, which allows borrowing a discovery credential.
    type Crypto: DiscoveryCryptoMaterial<V> + Borrow<V::DiscoveryCredential>;

    /// The iterator type produced which emits credentials.
    /// This is a lending iterator which may borrow things from `self`.
    type Iterator: Iterator<Item = (Self::Crypto, Self::Matched)>;

    /// Iterate over the available credentials
    fn iter(&'a self) -> Self::Iterator;
}

/// A source of credentials for a particular protocol version,
/// utilizing any [`DiscoveryCryptoMaterial`] which is usable
/// for discovering advertisements in that protocol version.
///
/// This trait is largely leveraged as a tool for building
/// new kinds of [`crate::credential::book::CredentialBook`]s
/// via the [`crate::credential::book::CredentialBookFromSources`]
/// wrapper. It differs from the [`DiscoveryCredentialSource`]
/// trait in that the crypto-materials do not have to be
/// discovery credentials, and can instead be some pre-calculated
/// crypto-materials.
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
    type Crypto: DiscoveryCryptoMaterial<V>;

    /// The iterator type produced which emits credentials.
    /// This is a lending iterator which may borrow things from `self`.
    type Iterator: Iterator<Item = (Self::Crypto, Self::Matched)>;

    /// Iterate over the available credentials
    fn iter(&'a self) -> Self::Iterator;
}

// Note: This is needed to get around coherence problems
// with the [`CredentialSource`] trait's relationship
// with [`DiscoveryCredentialSource`] if it were declared
// as a sub-trait (i.e: conflicting impls)
/// Wrapper which turns any [`DiscoveryCredentialSource`]
/// into a [`CredentialSource`].
pub struct AsCredentialSource<S>(pub S);

impl<'a, V: ProtocolVersion, S: DiscoveryCredentialSource<'a, V>> CredentialSource<'a, V>
    for AsCredentialSource<S>
{
    type Matched = <S as DiscoveryCredentialSource<'a, V>>::Matched;
    type Crypto = <S as DiscoveryCredentialSource<'a, V>>::Crypto;
    type Iterator = <S as DiscoveryCredentialSource<'a, V>>::Iterator;

    fn iter(&'a self) -> Self::Iterator {
        self.0.iter()
    }
}

/// A simple [`DiscoveryCredentialSource`] which iterates over a provided slice of credentials
pub struct SliceCredentialSource<'c, V: ProtocolVersion, M: MatchedCredential> {
    credentials: &'c [MatchableCredential<V, M>],
}

impl<'c, V: ProtocolVersion, M: MatchedCredential> SliceCredentialSource<'c, V, M> {
    /// Construct the credential supplier from the provided slice of credentials.
    pub fn new(credentials: &'c [MatchableCredential<V, M>]) -> Self {
        Self { credentials }
    }
}

impl<'a, 'b, V: ProtocolVersion, M: MatchedCredential> DiscoveryCredentialSource<'a, V>
    for SliceCredentialSource<'b, V, M>
where
    'b: 'a,
    Self: 'b,
    &'a <V as ProtocolVersion>::DiscoveryCredential: DiscoveryCryptoMaterial<V>,
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
