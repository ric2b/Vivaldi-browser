// Copyright 2024 Google LLC
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

//! Contains matched credential structs and traits which contain information about the credential
//! which is passed back to the caller upon a successful decrypt and credential match.

#[cfg(any(test, feature = "alloc"))]
use alloc::vec::Vec;
#[cfg(any(test, feature = "alloc"))]
use crypto_provider::CryptoProvider;

#[cfg(any(test, feature = "alloc"))]
use crate::credential::metadata::{decrypt_metadata_with_nonce, encrypt_metadata};
use crate::credential::{v0::V0, v1::V1, ProtocolVersion};
use core::{convert::Infallible, fmt::Debug};
use ldt_np_adv::V0IdentityToken;

/// The portion of a credential's data to be bundled with the advertisement content it was used to
/// decrypt. At a minimum, this includes any encrypted identity-specific metadata.
///
/// As it is `Debug` and `Eq`, implementors should not hold any cryptographic secrets to avoid
/// accidental logging, timing side channels on comparison, etc, or should use custom impls of
/// those traits rather than deriving them.
///
/// Instances of `MatchedCredential` may be cloned whenever advertisement content is
/// successfully associated with a credential (see [`WithMatchedCredential`]). As a
/// result, it's recommended to use matched-credentials which reference
/// some underlying match-data, but don't necessarily own it.
/// See [`ReferencedMatchedCredential`] for the most common case of shared references.
pub trait MatchedCredential: Debug + PartialEq + Eq + Clone {
    /// The type returned for successful calls to [`Self::fetch_encrypted_metadata`].
    type EncryptedMetadata: AsRef<[u8]>;

    /// The type of errors for [`Self::fetch_encrypted_metadata`].
    type EncryptedMetadataFetchError: Debug;

    /// Attempts to obtain the (AES-GCM)-encrypted metadata bytes for the credential,
    /// with possible failure based on the availability of the underlying data (i.e:
    /// failing disk reads.)
    ///
    /// If your implementation does not maintain any encrypted metadata for each credential,
    /// you may simply return an empty byte-array from this method.
    ///
    /// If your method for obtaining metadata cannot fail, use
    /// the `core::convert::Infallible` type for the error type
    /// [`Self::EncryptedMetadataFetchError`].
    fn fetch_encrypted_metadata(
        &self,
    ) -> Result<Self::EncryptedMetadata, Self::EncryptedMetadataFetchError>;
}

/// [`MatchedCredential`] wrapper around a shared reference to a [`MatchedCredential`].
/// This is done instead of providing a blanket impl of [`MatchedCredential`] for
/// reference types to allow for downstream crates to impl [`MatchedCredential`] on
/// specific reference types.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ReferencedMatchedCredential<'a, M: MatchedCredential> {
    wrapped: &'a M,
}

impl<'a, M: MatchedCredential> From<&'a M> for ReferencedMatchedCredential<'a, M> {
    fn from(wrapped: &'a M) -> Self {
        Self { wrapped }
    }
}

impl<'a, M: MatchedCredential> AsRef<M> for ReferencedMatchedCredential<'a, M> {
    fn as_ref(&self) -> &M {
        self.wrapped
    }
}

impl<'a, M: MatchedCredential> MatchedCredential for ReferencedMatchedCredential<'a, M> {
    type EncryptedMetadata = <M as MatchedCredential>::EncryptedMetadata;
    type EncryptedMetadataFetchError = <M as MatchedCredential>::EncryptedMetadataFetchError;
    fn fetch_encrypted_metadata(
        &self,
    ) -> Result<Self::EncryptedMetadata, Self::EncryptedMetadataFetchError> {
        self.wrapped.fetch_encrypted_metadata()
    }
}

/// A simple implementation of [`MatchedCredential`] where all match-data
/// is contained in the encrypted metadata byte-field.
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct MetadataMatchedCredential<A: AsRef<[u8]> + Clone + Debug + PartialEq + Eq> {
    encrypted_metadata: A,
}

#[cfg(any(test, feature = "alloc"))]
impl MetadataMatchedCredential<Vec<u8>> {
    /// Builds a [`MetadataMatchedCredential`] whose contents are given
    /// as plaintext to be encrypted using AES-GCM against the given
    /// broadcast crypto-material.
    pub fn encrypt_from_plaintext<V, C>(
        hkdf: &np_hkdf::NpKeySeedHkdf<C>,
        identity_token: V::IdentityToken,
        plaintext_metadata: &[u8],
    ) -> Self
    where
        V: ProtocolVersion,
        C: CryptoProvider,
    {
        // TODO move this to identity provider
        let encrypted_metadata = encrypt_metadata::<C, V>(hkdf, identity_token, plaintext_metadata);
        Self { encrypted_metadata }
    }
}

impl<A: AsRef<[u8]> + Clone + Debug + PartialEq + Eq> MetadataMatchedCredential<A> {
    /// Builds a new [`MetadataMatchedCredential`] with the given
    /// encrypted metadata.
    pub fn new(encrypted_metadata: A) -> Self {
        Self { encrypted_metadata }
    }
}

impl<A: AsRef<[u8]> + Clone + Debug + PartialEq + Eq> MatchedCredential
    for MetadataMatchedCredential<A>
{
    type EncryptedMetadata = A;
    type EncryptedMetadataFetchError = Infallible;
    fn fetch_encrypted_metadata(&self) -> Result<Self::EncryptedMetadata, Infallible> {
        Ok(self.encrypted_metadata.clone())
    }
}

/// Trivial implementation of [`MatchedCredential`] which consists of no match-data.
/// Suitable for usage scenarios where the decoded advertisement contents matter,
/// but not necessarily which devices generated the contents.
///
/// Attempting to obtain the encrypted metadata from this type of credential
/// will always yield an empty byte-array.
#[derive(Default, Debug, PartialEq, Eq, Clone)]
pub struct EmptyMatchedCredential;

impl MatchedCredential for EmptyMatchedCredential {
    type EncryptedMetadata = [u8; 0];
    type EncryptedMetadataFetchError = Infallible;
    fn fetch_encrypted_metadata(
        &self,
    ) -> Result<Self::EncryptedMetadata, Self::EncryptedMetadataFetchError> {
        Ok([0u8; 0])
    }
}

#[cfg(any(test, feature = "devtools"))]
/// A [`MatchedCredential`] which consists only of the `key_seed` in the crypto-material
/// for the credential. Note that this is unique per-credential by construction,
/// and so this provides natural match-data for credentials in settings where
/// there may not be any other information available.
///
/// Since this matched-credential type contains cryptographic information mirroring
/// a credential's crypto-material, this structure is not suitable for production
/// usage outside of unit tests and dev-tools.
///
/// Additionally, note that the metadata on this particular kind of matched credential
/// is deliberately made inaccessible. This is done because a key-seed representation
/// is only suitable in very limited circumstances where no other meaningful
/// identifying information is available, such as that which is contained in metadata.
/// Attempting to obtain the encrypted metadata from this type of matched credential
/// will always yield an empty byte-array.
#[derive(Default, Debug, PartialEq, Eq, Clone)]
pub struct KeySeedMatchedCredential {
    key_seed: [u8; 32],
}

#[cfg(any(test, feature = "devtools"))]
impl From<[u8; 32]> for KeySeedMatchedCredential {
    fn from(key_seed: [u8; 32]) -> Self {
        Self { key_seed }
    }
}
#[cfg(any(test, feature = "devtools"))]
impl From<KeySeedMatchedCredential> for [u8; 32] {
    fn from(matched: KeySeedMatchedCredential) -> Self {
        matched.key_seed
    }
}

#[cfg(any(test, feature = "devtools"))]
impl MatchedCredential for KeySeedMatchedCredential {
    type EncryptedMetadata = [u8; 0];
    type EncryptedMetadataFetchError = Infallible;
    fn fetch_encrypted_metadata(
        &self,
    ) -> Result<Self::EncryptedMetadata, Self::EncryptedMetadataFetchError> {
        Ok([0u8; 0])
    }
}

/// Common trait to deserialized, decrypted V0 advs and V1 sections which
/// exposes relevant data about matched identities.
pub trait HasIdentityMatch {
    /// The protocol version for which this advertisement
    /// content has an identity-match.
    type Version: ProtocolVersion;

    /// Gets the decrypted plaintext version-specific
    /// metadata key for the associated identity.
    fn identity_token(&self) -> <Self::Version as ProtocolVersion>::IdentityToken;
}

impl HasIdentityMatch for V0IdentityToken {
    type Version = V0;
    fn identity_token(&self) -> Self {
        *self
    }
}

impl HasIdentityMatch for crate::extended::V1IdentityToken {
    type Version = V1;
    fn identity_token(&self) -> Self {
        *self
    }
}

#[cfg(any(test, feature = "alloc"))]
/// Type for errors from [`WithMatchedCredential#decrypt_metadata`]
#[derive(Debug)]
pub enum MatchedMetadataDecryptionError<M: MatchedCredential> {
    /// Retrieving the encrypted metadata failed for one reason
    /// or another, so we didn't get a chance to try decryption.
    RetrievalFailed(<M as MatchedCredential>::EncryptedMetadataFetchError),
    /// The encrypted metadata could be retrieved, but it did
    /// not successfully decrypt against the matched identity.
    /// This could be an indication of data corruption or
    /// of malformed crypto on the sender-side.
    DecryptionFailed,
}

/// Decrypted advertisement content with the [MatchedCredential] from the credential that decrypted
/// it, along with any other information which is relevant to the identity-match.
#[derive(Debug, PartialEq, Eq)]
pub struct WithMatchedCredential<M: MatchedCredential, T: HasIdentityMatch> {
    matched: M,
    /// The 12-byte metadata nonce as derived from the key-seed HKDF
    /// to be used for decrypting the encrypted metadata in the attached
    /// matched-credential.
    metadata_nonce: [u8; 12],
    contents: T,
}

impl<'a, M: MatchedCredential + Clone, T: HasIdentityMatch>
    WithMatchedCredential<ReferencedMatchedCredential<'a, M>, T>
{
    /// Clones the referenced match-data to update this container
    /// so that the match-data is owned, rather than borrowed.
    pub fn clone_match_data(self) -> WithMatchedCredential<M, T> {
        let matched = self.matched.as_ref().clone();
        let metadata_nonce = self.metadata_nonce;
        let contents = self.contents;

        WithMatchedCredential { matched, metadata_nonce, contents }
    }
}

impl<M: MatchedCredential, T: HasIdentityMatch> WithMatchedCredential<M, T> {
    pub(crate) fn new(matched: M, metadata_nonce: [u8; 12], contents: T) -> Self {
        Self { matched, metadata_nonce, contents }
    }
    /// Applies the given function to the wrapped contents, yielding
    /// a new instance with the same matched-credential.
    pub fn map<R: HasIdentityMatch>(
        self,
        mapping: impl FnOnce(T) -> R,
    ) -> WithMatchedCredential<M, R> {
        let contents = mapping(self.contents);
        let matched = self.matched;
        let metadata_nonce = self.metadata_nonce;
        WithMatchedCredential { matched, metadata_nonce, contents }
    }
    /// Credential data for the credential that decrypted the content.
    pub fn matched_credential(&self) -> &M {
        &self.matched
    }
    /// The decrypted advertisement content.
    pub fn contents(&self) -> &T {
        &self.contents
    }

    #[cfg(any(test, feature = "alloc"))]
    fn decrypt_metadata_from_fetch<C: CryptoProvider>(
        &self,
        encrypted_metadata: &[u8],
    ) -> Result<Vec<u8>, MatchedMetadataDecryptionError<M>> {
        decrypt_metadata_with_nonce::<C, T::Version>(
            self.metadata_nonce,
            self.contents.identity_token(),
            encrypted_metadata,
        )
        .map_err(|_| MatchedMetadataDecryptionError::DecryptionFailed)
    }

    #[cfg(any(test, feature = "alloc"))]
    /// Attempts to decrypt the encrypted metadata
    /// associated with the matched credential
    /// based on the details of the identity-match.
    pub fn decrypt_metadata<C: CryptoProvider>(
        &self,
    ) -> Result<Vec<u8>, MatchedMetadataDecryptionError<M>> {
        self.matched
            .fetch_encrypted_metadata()
            .map_err(|e| MatchedMetadataDecryptionError::RetrievalFailed(e))
            .and_then(|x| Self::decrypt_metadata_from_fetch::<C>(self, x.as_ref()))
    }
}
