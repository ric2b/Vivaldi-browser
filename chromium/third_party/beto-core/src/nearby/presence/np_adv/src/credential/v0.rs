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

//! Cryptographic materials for v0 advertisement-format credentials.
use crate::credential::{protocol_version_seal, DiscoveryMetadataCryptoMaterial, ProtocolVersion};
use crypto_provider::{aead::Aead, aes, aes::ctr, CryptoProvider};
use ldt_np_adv::V0IdentityToken;
use np_hkdf::NpKeySeedHkdf;

/// Cryptographic information about a particular V0 discovery credential
/// necessary to match and decrypt encrypted V0 advertisements.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct V0DiscoveryCredential {
    /// The 32-byte key-seed used for generating other key material.
    pub key_seed: [u8; 32],

    /// The (LDT-variant) HMAC of the identity token.
    pub expected_identity_token_hmac: [u8; 32],
}

impl V0DiscoveryCredential {
    /// Construct an [V0DiscoveryCredential] from the provided identity data.
    pub fn new(key_seed: [u8; 32], expected_identity_token_hmac: [u8; 32]) -> Self {
        Self { key_seed, expected_identity_token_hmac }
    }
}

impl DiscoveryMetadataCryptoMaterial<V0> for V0DiscoveryCredential {
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        np_hkdf::NpKeySeedHkdf::<C>::new(&self.key_seed).v0_metadata_nonce()
    }
}

impl V0DiscoveryCryptoMaterial for V0DiscoveryCredential {
    fn ldt_adv_cipher<C: CryptoProvider>(&self) -> ldt_np_adv::AuthenticatedNpLdtDecryptCipher<C> {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&self.key_seed);
        ldt_np_adv::build_np_adv_decrypter(
            &hkdf.v0_ldt_key(),
            self.expected_identity_token_hmac,
            hkdf.v0_identity_token_hmac_key(),
        )
    }
}

/// Type-level identifier for the V0 protocol version.
#[derive(Debug, Clone)]
pub enum V0 {}

impl protocol_version_seal::ProtocolVersionSeal for V0 {}

impl ProtocolVersion for V0 {
    type DiscoveryCredential = V0DiscoveryCredential;
    type IdentityToken = V0IdentityToken;

    fn metadata_nonce_from_key_seed<C: CryptoProvider>(
        hkdf: &NpKeySeedHkdf<C>,
    ) -> <C::Aes128Gcm as Aead>::Nonce {
        hkdf.v0_metadata_nonce()
    }

    // TODO should be IdP specific
    fn extract_metadata_key<C: CryptoProvider>(metadata_key: V0IdentityToken) -> aes::Aes128Key {
        np_hkdf::v0_metadata_expanded_key::<C>(&metadata_key.bytes()).into()
    }
}

/// Trait which exists purely to be able to restrict the protocol
/// version of certain type-bounds to V0.
pub trait V0ProtocolVersion: ProtocolVersion {}

impl V0ProtocolVersion for V0 {}

/// Cryptographic material for an individual NP credential used to decrypt v0 advertisements.
/// Unlike [`V0DiscoveryCredential`], derived information about cryptographic materials may
/// be stored in implementors of this trait.
// Space-time tradeoffs:
// - LDT keys (64b) take about 1.4us.
pub trait V0DiscoveryCryptoMaterial: DiscoveryMetadataCryptoMaterial<V0> {
    /// Returns an LDT NP advertisement cipher built with the provided `Aes`
    fn ldt_adv_cipher<C: CryptoProvider>(&self) -> ldt_np_adv::AuthenticatedNpLdtDecryptCipher<C>;
}

/// [`V0DiscoveryCryptoMaterial`] that minimizes CPU time when providing key material at
/// the expense of occupied memory.
pub struct PrecalculatedV0DiscoveryCryptoMaterial {
    pub(crate) legacy_ldt_key: ldt::LdtKey<xts_aes::XtsAes128Key>,
    pub(crate) legacy_identity_token_hmac: [u8; 32],
    pub(crate) legacy_identity_token_hmac_key: [u8; 32],
    pub(crate) metadata_nonce: ctr::AesCtrNonce,
}

impl PrecalculatedV0DiscoveryCryptoMaterial {
    /// Construct a new instance from the provided credential material.
    pub(crate) fn new<C: CryptoProvider>(discovery_credential: &V0DiscoveryCredential) -> Self {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&discovery_credential.key_seed);
        Self {
            legacy_ldt_key: hkdf.v0_ldt_key(),
            legacy_identity_token_hmac: discovery_credential.expected_identity_token_hmac,
            legacy_identity_token_hmac_key: *hkdf.v0_identity_token_hmac_key().as_bytes(),
            metadata_nonce: hkdf.v0_metadata_nonce(),
        }
    }
}

impl DiscoveryMetadataCryptoMaterial<V0> for PrecalculatedV0DiscoveryCryptoMaterial {
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        self.metadata_nonce
    }
}

impl V0DiscoveryCryptoMaterial for PrecalculatedV0DiscoveryCryptoMaterial {
    fn ldt_adv_cipher<C: CryptoProvider>(&self) -> ldt_np_adv::AuthenticatedNpLdtDecryptCipher<C> {
        ldt_np_adv::build_np_adv_decrypter(
            &self.legacy_ldt_key,
            self.legacy_identity_token_hmac,
            self.legacy_identity_token_hmac_key.into(),
        )
    }
}

// Implementations for reference types -- we don't provide a blanket impl for references
// due to the potential to conflict with downstream crates' implementations.

impl<'a> DiscoveryMetadataCryptoMaterial<V0> for &'a V0DiscoveryCredential {
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        (*self).metadata_nonce::<C>()
    }
}

impl<'a> V0DiscoveryCryptoMaterial for &'a V0DiscoveryCredential {
    fn ldt_adv_cipher<C: CryptoProvider>(&self) -> ldt_np_adv::AuthenticatedNpLdtDecryptCipher<C> {
        (*self).ldt_adv_cipher::<C>()
    }
}

impl<'a> DiscoveryMetadataCryptoMaterial<V0> for &'a PrecalculatedV0DiscoveryCryptoMaterial {
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        (*self).metadata_nonce::<C>()
    }
}

impl<'a> V0DiscoveryCryptoMaterial for &'a PrecalculatedV0DiscoveryCryptoMaterial {
    fn ldt_adv_cipher<C: CryptoProvider>(&self) -> ldt_np_adv::AuthenticatedNpLdtDecryptCipher<C> {
        (*self).ldt_adv_cipher::<C>()
    }
}

/// Crypto material for creating V1 advertisements.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct V0BroadcastCredential {
    /// The 32-byte key-seed used for generating other key material.
    pub key_seed: [u8; 32],

    /// The 14-byte identity-token which identifies the sender.
    pub identity_token: V0IdentityToken,
}

impl V0BroadcastCredential {
    /// Builds some simple broadcast crypto-materials out of
    /// the provided key-seed and version-specific metadata-key.
    pub fn new(key_seed: [u8; 32], identity_token: V0IdentityToken) -> Self {
        Self { key_seed, identity_token }
    }

    /// Key seed from which other keys are derived.
    pub(crate) fn key_seed(&self) -> [u8; 32] {
        self.key_seed
    }

    /// Identity token that will be incorporated into encrypted advertisements.
    pub(crate) fn identity_token(&self) -> V0IdentityToken {
        self.identity_token
    }

    /// Derive a discovery credential with the data necessary to discover advertisements produced
    /// by this broadcast credential.
    pub fn derive_discovery_credential<C: CryptoProvider>(&self) -> V0DiscoveryCredential {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&self.key_seed);

        V0DiscoveryCredential::new(
            self.key_seed,
            hkdf.v0_identity_token_hmac_key().calculate_hmac::<C>(&self.identity_token.bytes()),
        )
    }
}
