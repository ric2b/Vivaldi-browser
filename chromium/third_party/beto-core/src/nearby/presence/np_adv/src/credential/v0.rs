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
use crate::credential::{protocol_version_seal, DiscoveryCryptoMaterial, ProtocolVersion};
use crate::legacy::ShortMetadataKey;
use crate::MetadataKey;
use crypto_provider::{CryptoProvider, CryptoRng};

/// Cryptographic information about a particular V0 discovery credential
/// necessary to match and decrypt encrypted V0 advertisements.
#[derive(Clone)]
pub struct V0DiscoveryCredential {
    key_seed: [u8; 32],
    legacy_metadata_key_hmac: [u8; 32],
}

impl V0DiscoveryCredential {
    /// Construct an [V0DiscoveryCredential] from the provided identity data.
    pub fn new(key_seed: [u8; 32], legacy_metadata_key_hmac: [u8; 32]) -> Self {
        Self { key_seed, legacy_metadata_key_hmac }
    }
}

impl DiscoveryCryptoMaterial<V0> for V0DiscoveryCredential {
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        V0::metadata_nonce_from_key_seed::<C>(&self.key_seed)
    }
}

impl V0DiscoveryCryptoMaterial for V0DiscoveryCredential {
    fn ldt_adv_cipher<C: CryptoProvider>(&self) -> ldt_np_adv::LdtNpAdvDecrypterXtsAes128<C> {
        let hkdf = np_hkdf::NpKeySeedHkdf::new(&self.key_seed);
        ldt_np_adv::build_np_adv_decrypter(
            &hkdf.legacy_ldt_key(),
            self.legacy_metadata_key_hmac,
            hkdf.legacy_metadata_key_hmac_key(),
        )
    }
}

/// Type-level identifier for the V0 protocol version.
#[derive(Debug, Clone)]
pub enum V0 {}

impl protocol_version_seal::ProtocolVersionSeal for V0 {}

impl ProtocolVersion for V0 {
    type DiscoveryCredential = V0DiscoveryCredential;
    type MetadataKey = ShortMetadataKey;

    fn metadata_nonce_from_key_seed<C: CryptoProvider>(key_seed: &[u8; 32]) -> [u8; 12] {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(key_seed);
        hkdf.legacy_metadata_nonce()
    }
    fn expand_metadata_key<C: CryptoProvider>(metadata_key: ShortMetadataKey) -> MetadataKey {
        metadata_key.expand::<C>()
    }
    fn gen_random_metadata_key<R: CryptoRng>(rng: &mut R) -> ShortMetadataKey {
        ShortMetadataKey(rng.gen())
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
pub trait V0DiscoveryCryptoMaterial: DiscoveryCryptoMaterial<V0> {
    /// Returns an LDT NP advertisement cipher built with the provided `Aes`
    fn ldt_adv_cipher<C: CryptoProvider>(&self) -> ldt_np_adv::LdtNpAdvDecrypterXtsAes128<C>;
}

/// [`V0DiscoveryCryptoMaterial`] that minimizes CPU time when providing key material at
/// the expense of occupied memory.
pub struct PrecalculatedV0DiscoveryCryptoMaterial {
    pub(crate) legacy_ldt_key: ldt::LdtKey<xts_aes::XtsAes128Key>,
    pub(crate) legacy_metadata_key_hmac: [u8; 32],
    pub(crate) legacy_metadata_key_hmac_key: [u8; 32],
    pub(crate) metadata_nonce: [u8; 12],
}

impl PrecalculatedV0DiscoveryCryptoMaterial {
    /// Construct a new instance from the provided credential material.
    pub(crate) fn new<C: CryptoProvider>(discovery_credential: &V0DiscoveryCredential) -> Self {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&discovery_credential.key_seed);
        Self {
            legacy_ldt_key: hkdf.legacy_ldt_key(),
            legacy_metadata_key_hmac: discovery_credential.legacy_metadata_key_hmac,
            legacy_metadata_key_hmac_key: *hkdf.legacy_metadata_key_hmac_key().as_bytes(),
            metadata_nonce: hkdf.legacy_metadata_nonce(),
        }
    }
}

impl DiscoveryCryptoMaterial<V0> for PrecalculatedV0DiscoveryCryptoMaterial {
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        self.metadata_nonce
    }
}

impl V0DiscoveryCryptoMaterial for PrecalculatedV0DiscoveryCryptoMaterial {
    fn ldt_adv_cipher<C: CryptoProvider>(&self) -> ldt_np_adv::LdtNpAdvDecrypterXtsAes128<C> {
        ldt_np_adv::build_np_adv_decrypter(
            &self.legacy_ldt_key,
            self.legacy_metadata_key_hmac,
            self.legacy_metadata_key_hmac_key.into(),
        )
    }
}

// Implementations for reference types -- we don't provide a blanket impl for references
// due to the potential to conflict with downstream crates' implementations.

impl<'a> DiscoveryCryptoMaterial<V0> for &'a V0DiscoveryCredential {
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        (*self).metadata_nonce::<C>()
    }
}

impl<'a> V0DiscoveryCryptoMaterial for &'a V0DiscoveryCredential {
    fn ldt_adv_cipher<C: CryptoProvider>(&self) -> ldt_np_adv::LdtNpAdvDecrypterXtsAes128<C> {
        (*self).ldt_adv_cipher::<C>()
    }
}

impl<'a> DiscoveryCryptoMaterial<V0> for &'a PrecalculatedV0DiscoveryCryptoMaterial {
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        (*self).metadata_nonce::<C>()
    }
}

impl<'a> V0DiscoveryCryptoMaterial for &'a PrecalculatedV0DiscoveryCryptoMaterial {
    fn ldt_adv_cipher<C: CryptoProvider>(&self) -> ldt_np_adv::LdtNpAdvDecrypterXtsAes128<C> {
        (*self).ldt_adv_cipher::<C>()
    }
}
