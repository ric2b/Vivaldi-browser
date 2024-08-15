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

//! Credential types used in deserialization.
//!
//! While simple implementations are provided to get started with, there is likely opportunity for
//! efficiency gains with implementations tailored to suit (e.g. caching a few hot credentials
//! rather than reading from disk every time, etc).

use crate::MetadataKey;

use crate::credential::v0::{V0DiscoveryCredential, V0ProtocolVersion};

use core::convert::Infallible;
use core::fmt::Debug;
use crypto_provider::{CryptoProvider, CryptoRng};

pub mod book;
pub mod source;
#[cfg(test)]
pub mod tests;
pub mod v0;
pub mod v1;

/// Information about a credential as supplied by the caller.
#[derive(Clone)]
pub struct MatchableCredential<V: ProtocolVersion, M: MatchedCredential> {
    /// The discovery credential/cryptographic information associated
    /// with this particular credential which is used for discovering
    /// advertisements/advertisement sections generated via the
    /// paired sender credential.
    pub discovery_credential: V::DiscoveryCredential,
    /// The data which will be yielded back to the caller upon a successful
    /// identity-match against this credential.
    pub match_data: M,
}

impl<V: ProtocolVersion, M: MatchedCredential> MatchableCredential<V, M> {
    /// De-structures this credential into the pairing of a discovery
    /// credential and some matched credential data.
    pub fn into_pair(self) -> (V::DiscoveryCredential, M) {
        (self.discovery_credential, self.match_data)
    }
    /// Views this credential as a (borrowed) discovery-credential
    /// combined with some matched credential data
    /// (which is copied - see documentation on [`MatchedCredential`])
    pub fn as_pair(&self) -> (&V::DiscoveryCredential, ReferencedMatchedCredential<M>) {
        (&self.discovery_credential, ReferencedMatchedCredential::from(&self.match_data))
    }
}

/// The portion of a credential's data to be bundled with the advertisement content it was used to
/// decrypt. At a minimum, this includes any encrypted identity-specific metadata.
///
/// As it is `Debug` and `Eq`, implementors should not hold any cryptographic secrets to avoid
/// accidental logging, timing side channels on comparison, etc, or should use custom impls of
/// those traits rather than deriving them.
///
/// Instances of `MatchedCredential` may be cloned whenever advertisement content is
/// successfully associated with a credential (see [`crate::WithMatchedCredential`]). As a
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
impl MetadataMatchedCredential<alloc::vec::Vec<u8>> {
    /// Builds a [`MetadataMatchedCredential`] whose contents are given
    /// as plaintext to be encrypted using AES-GCM against the given
    /// broadcast crypto-material.
    pub fn encrypt_from_plaintext<V, B, C>(broadcast_cm: &B, plaintext_metadata: &[u8]) -> Self
    where
        V: ProtocolVersion,
        B: BroadcastCryptoMaterial<V>,
        C: CryptoProvider,
    {
        let encrypted_metadata = broadcast_cm.encrypt_metadata::<C>(plaintext_metadata);
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

/// Error returned when metadata decryption fails.
#[derive(Debug, Eq, PartialEq)]
pub struct MetadataDecryptionError;

/// Seal for protocol versions to enforce totality.
pub(crate) mod protocol_version_seal {
    /// Internal-only supertrait of protocol versions
    /// for the purpose of sealing the trait.
    pub trait ProtocolVersionSeal: Clone {}
}

/// Marker trait for protocol versions (V0/V1)
/// and associated data about them.
pub trait ProtocolVersion: protocol_version_seal::ProtocolVersionSeal {
    /// The discovery credential type for this protocol version, which
    /// is the minimal amount of cryptographic materials that we need
    /// in order to discover advertisements/sections which make
    /// use of the sender-paired version of the credential.
    type DiscoveryCredential: DiscoveryCryptoMaterial<Self> + Clone;

    /// The native-length metadata key for this protocol version
    /// [i.e: if V0, a 14-byte metadata key, or if V1, a 16-byte
    /// metadata key.]
    type MetadataKey: Clone + AsRef<[u8]>;

    /// Computes the metadata nonce for this version from the given key-seed.
    fn metadata_nonce_from_key_seed<C: CryptoProvider>(key_seed: &[u8; 32]) -> [u8; 12];

    /// Expands the passed metadata key (if needed) to a 16-byte metadata key
    /// which may be used for metadata encryption/decryption
    fn expand_metadata_key<C: CryptoProvider>(metadata_key: Self::MetadataKey) -> MetadataKey;

    /// Generates a random metadata key using the given cryptographically-secure Rng
    fn gen_random_metadata_key<R: CryptoRng>(rng: &mut R) -> Self::MetadataKey;

    #[cfg(any(test, feature = "alloc"))]
    /// Decrypt the given metadata using the given metadata nonce and version-specific
    /// metadata key. Returns [`MetadataDecryptionError`] in the case that
    /// the decryption operation failed.
    fn decrypt_metadata<C: CryptoProvider>(
        metadata_nonce: [u8; 12],
        metadata_key: Self::MetadataKey,
        encrypted_metadata: &[u8],
    ) -> Result<alloc::vec::Vec<u8>, MetadataDecryptionError> {
        use crypto_provider::{
            aead::{Aead, AeadInit},
            aes::Aes128Key,
        };

        let metadata_key = Self::expand_metadata_key::<C>(metadata_key);
        let metadata_key = Aes128Key::from(metadata_key.0);
        let aead = <<C as CryptoProvider>::Aes128Gcm as AeadInit<Aes128Key>>::new(&metadata_key);
        // No additional authenticated data for encrypted metadata.
        aead.decrypt(encrypted_metadata, &[], &metadata_nonce).map_err(|_| MetadataDecryptionError)
    }
}

/// Trait for structures which provide cryptographic
/// materials for discovery in a particular protocol version.
/// See [`crate::credential::v0::V0DiscoveryCryptoMaterial`]
/// and [`crate::credential::v1::V1DiscoveryCryptoMaterial`]
/// for V0 and V1 specializations.
pub trait DiscoveryCryptoMaterial<V: ProtocolVersion> {
    /// Constructs or copies the metadata nonce used for decryption of associated credential
    /// metadata for the identity represented via this crypto material.
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12];
}

/// Cryptographic materials necessary for broadcasting encrypted
/// advertisement contents with the given protocol version.
pub trait BroadcastCryptoMaterial<V: ProtocolVersion> {
    /// Yields a copy of the key seed to be used to derive other key materials used
    /// in the encryption of broadcasted advertisement contents.
    fn key_seed(&self) -> [u8; 32];

    /// Yields a copy of the metadata-key (size dependent on protocol version)
    /// to tag advertisement contents sent with this broadcast crypto-material.
    fn metadata_key(&self) -> V::MetadataKey;

    /// Yields the 16-byte expanded metadata key, suitable for metadata encryption.
    fn expanded_metadata_key<C: CryptoProvider>(&self) -> MetadataKey {
        V::expand_metadata_key::<C>(self.metadata_key())
    }

    /// Constructs the metadata nonce used for encryption of associated credential
    /// metadata for the identity represented via this crypto material.
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        V::metadata_nonce_from_key_seed::<C>(&self.key_seed())
    }

    /// Derives a V0 discovery credential from this V0 broadcast crypto-material
    /// which may be used to discover v0 advertisements broadcasted with this credential.`
    fn derive_v0_discovery_credential<C: CryptoProvider>(&self) -> V0DiscoveryCredential
    where
        V: V0ProtocolVersion,
    {
        let key_seed = self.key_seed();
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&key_seed);
        let metadata_key_hmac =
            hkdf.legacy_metadata_key_hmac_key().calculate_hmac(self.metadata_key().as_ref());
        V0DiscoveryCredential::new(key_seed, metadata_key_hmac)
    }

    #[cfg(any(test, feature = "alloc"))]
    /// Encrypts the given plaintext metadata bytes to allow that metadata
    /// to be shared with receiving devices.
    fn encrypt_metadata<C: CryptoProvider>(
        &self,
        plaintext_metadata: &[u8],
    ) -> alloc::vec::Vec<u8> {
        use crypto_provider::{
            aead::{Aead, AeadInit},
            aes::Aes128Key,
        };
        let plaintext_metadata_key = self.expanded_metadata_key::<C>();
        let plaintext_metadata_key = Aes128Key::from(plaintext_metadata_key.0);

        let aead =
            <<C as CryptoProvider>::Aes128Gcm as AeadInit<Aes128Key>>::new(&plaintext_metadata_key);
        // No additional authenticated data for encrypted metadata.
        aead.encrypt(plaintext_metadata, &[], &self.metadata_nonce::<C>())
            .expect("Metadata encryption should be infallible")
    }
}

/// Concrete implementation of [`BroadcastCryptoMaterial<V>`] for
/// a particular protocol version which keeps the key seed
/// and the metadata key contiguous in memory.
///
/// Broadcast crypto-material specified in this way will only
/// be usable for (unsigned) advertisement content broadcasts
/// in the given protocol version.
///
/// For more flexible expression of broadcast credentials,
/// feel free to directly implement one or more of the
/// [`BroadcastCryptoMaterial`] and/or
/// [`crate::credential::v1::SignedBroadcastCryptoMaterial`]
/// traits on your own struct, dependent on the details
/// of your own broadcast credentials.
pub struct SimpleBroadcastCryptoMaterial<V: ProtocolVersion> {
    key_seed: [u8; 32],
    metadata_key: V::MetadataKey,
}

impl<V: ProtocolVersion> SimpleBroadcastCryptoMaterial<V> {
    /// Builds some simple broadcast crypto-materials out of
    /// the provided key-seed and version-specific metadata-key.
    pub fn new(key_seed: [u8; 32], metadata_key: V::MetadataKey) -> Self {
        Self { key_seed, metadata_key }
    }
}

impl<V: ProtocolVersion> BroadcastCryptoMaterial<V> for SimpleBroadcastCryptoMaterial<V> {
    fn key_seed(&self) -> [u8; 32] {
        self.key_seed
    }
    fn metadata_key(&self) -> V::MetadataKey {
        self.metadata_key.clone()
    }
}
