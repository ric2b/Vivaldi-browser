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

//! Cryptographic materials for v1 advertisement-format credentials.

use crate::credential::{protocol_version_seal, DiscoveryMetadataCryptoMaterial, ProtocolVersion};
use crate::extended::V1IdentityToken;
use crypto_provider::{aead::Aead, aes, ed25519, CryptoProvider};
use np_hkdf::{DerivedSectionKeys, NpKeySeedHkdf};

/// Cryptographic information about a particular V1 discovery credential
/// necessary to match and decrypt encrypted V1 sections.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct V1DiscoveryCredential {
    /// The 32-byte key-seed used for generating other key material.
    pub key_seed: [u8; 32],

    /// The MIC-short-salt variant of the HMAC of the identity token.
    pub expected_mic_short_salt_identity_token_hmac: [u8; 32],

    /// The MIC-extended-salt variant of the HMAC of the identity token.
    pub expected_mic_extended_salt_identity_token_hmac: [u8; 32],

    /// The signed-extended-salt variant of the HMAC of the identity token.
    pub expected_signed_extended_salt_identity_token_hmac: [u8; 32],

    /// The ed25519 public key used for verification of signed sections.
    pub public_key: ed25519::PublicKey,
}

impl V1DiscoveryCredential {
    /// Construct a V1 discovery credential from the provided identity data.
    pub fn new(
        key_seed: [u8; 32],
        expected_mic_short_salt_identity_token_hmac: [u8; 32],
        expected_mic_extended_salt_identity_token_hmac: [u8; 32],
        expected_signed_extended_salt_identity_token_hmac: [u8; 32],
        public_key: ed25519::PublicKey,
    ) -> Self {
        Self {
            key_seed,
            expected_mic_short_salt_identity_token_hmac,
            expected_mic_extended_salt_identity_token_hmac,
            expected_signed_extended_salt_identity_token_hmac,
            public_key,
        }
    }

    /// Constructs pre-calculated crypto material from this discovery credential.
    pub(crate) fn to_precalculated<C: CryptoProvider>(
        &self,
    ) -> PrecalculatedV1DiscoveryCryptoMaterial {
        PrecalculatedV1DiscoveryCryptoMaterial {
            signed_identity_resolution_material: self.signed_identity_resolution_material::<C>(),
            mic_short_salt_identity_resolution_material: self
                .mic_short_salt_identity_resolution_material::<C>(),
            mic_extended_salt_identity_resolution_material: self
                .mic_extended_salt_identity_resolution_material::<C>(),
            signed_verification_material: self.signed_verification_material::<C>(),
            mic_short_salt_verification_material: self.mic_short_salt_verification_material::<C>(),
            mic_extended_salt_verification_material: self
                .mic_extended_salt_verification_material::<C>(),
            metadata_nonce: self.metadata_nonce::<C>(),
        }
    }
}

impl DiscoveryMetadataCryptoMaterial<V1> for V1DiscoveryCredential {
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        np_hkdf::NpKeySeedHkdf::<C>::new(&self.key_seed).v1_metadata_nonce()
    }
}

impl V1DiscoveryCryptoMaterial for V1DiscoveryCredential {
    fn signed_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> SignedSectionIdentityResolutionMaterial {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&self.key_seed);
        SignedSectionIdentityResolutionMaterial::from_hkdf_and_expected_identity_token_hmac(
            &hkdf,
            self.expected_signed_extended_salt_identity_token_hmac,
        )
    }

    fn mic_short_salt_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> MicShortSaltSectionIdentityResolutionMaterial {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&self.key_seed);
        MicShortSaltSectionIdentityResolutionMaterial::from_hkdf_and_expected_identity_token_hmac(
            &hkdf,
            self.expected_mic_short_salt_identity_token_hmac,
        )
    }

    fn mic_extended_salt_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> MicExtendedSaltSectionIdentityResolutionMaterial {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&self.key_seed);
        MicExtendedSaltSectionIdentityResolutionMaterial::from_hkdf_and_expected_identity_token_hmac(
            &hkdf,
            self.expected_mic_extended_salt_identity_token_hmac,
        )
    }

    fn signed_verification_material<C: CryptoProvider>(&self) -> SignedSectionVerificationMaterial {
        SignedSectionVerificationMaterial { public_key: self.public_key.clone() }
    }

    fn mic_short_salt_verification_material<C: CryptoProvider>(
        &self,
    ) -> MicShortSaltSectionVerificationMaterial {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&self.key_seed);
        let mic_hmac_key = *hkdf.v1_mic_short_salt_keys().mic_hmac_key().as_bytes();
        MicShortSaltSectionVerificationMaterial { mic_hmac_key }
    }

    fn mic_extended_salt_verification_material<C: CryptoProvider>(
        &self,
    ) -> MicExtendedSaltSectionVerificationMaterial {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&self.key_seed);
        let mic_hmac_key = *hkdf.v1_mic_extended_salt_keys().mic_hmac_key().as_bytes();
        MicExtendedSaltSectionVerificationMaterial { mic_hmac_key }
    }
}

/// Type-level identifier for the V1 protocol version.
#[derive(Debug, Clone)]
pub enum V1 {}

impl protocol_version_seal::ProtocolVersionSeal for V1 {}

impl ProtocolVersion for V1 {
    type DiscoveryCredential = V1DiscoveryCredential;
    type IdentityToken = V1IdentityToken;

    fn metadata_nonce_from_key_seed<C: CryptoProvider>(
        hkdf: &NpKeySeedHkdf<C>,
    ) -> <C::Aes128Gcm as Aead>::Nonce {
        hkdf.v1_metadata_nonce()
    }

    fn extract_metadata_key<C: CryptoProvider>(
        identity_token: Self::IdentityToken,
    ) -> aes::Aes128Key {
        identity_token.into_bytes().into()
    }
}

/// Trait which exists purely to be able to restrict the protocol
/// version of certain type-bounds to V1.
pub trait V1ProtocolVersion: ProtocolVersion {}

impl V1ProtocolVersion for V1 {}

/// Cryptographic materials necessary for determining whether or not
/// a given V1 advertisement section matches an identity.
/// Per the construction of the V1 specification, this is also
/// the information necessary to decrypt the raw byte contents
/// of an encrypted V1 section.
#[derive(Clone)]
pub(crate) struct SectionIdentityResolutionMaterial {
    /// The AES key for decrypting section ciphertext
    pub(crate) aes_key: aes::Aes128Key,
    /// The identity token HMAC key for deriving and verifying the identity metadata
    /// key HMAC against the expected value.
    pub(crate) identity_token_hmac_key: [u8; 32],
    /// The expected identity token HMAC to check against for an identity match.
    pub(crate) expected_identity_token_hmac: [u8; 32],
}

/// Cryptographic materials necessary for determining whether or not
/// a given V1 signed advertisement section matches an identity.
#[derive(Clone)]
pub struct SignedSectionIdentityResolutionMaterial(SectionIdentityResolutionMaterial);

impl SignedSectionIdentityResolutionMaterial {
    #[cfg(test)]
    pub(crate) fn from_raw(raw: SectionIdentityResolutionMaterial) -> Self {
        Self(raw)
    }
    /// Extracts the underlying section-identity resolution material carried around
    /// within this wrapper for resolution of signed sections.
    pub(crate) fn into_raw_resolution_material(self) -> SectionIdentityResolutionMaterial {
        self.0
    }
    #[cfg(any(test, feature = "devtools"))]
    /// Gets the underlying section-identity resolution material carried around
    /// within this wrapper for resolution of signed sections.
    pub(crate) fn as_raw_resolution_material(&self) -> &SectionIdentityResolutionMaterial {
        &self.0
    }

    /// Constructs identity-resolution material for a signed section whose
    /// discovery credential leverages the provided HKDF and has the given
    /// expected metadata-key HMAC.
    pub(crate) fn from_hkdf_and_expected_identity_token_hmac<C: CryptoProvider>(
        hkdf: &np_hkdf::NpKeySeedHkdf<C>,
        expected_identity_token_hmac: [u8; 32],
    ) -> Self {
        Self(SectionIdentityResolutionMaterial {
            aes_key: hkdf.v1_signature_keys().aes_key(),
            identity_token_hmac_key: *hkdf.v1_signature_keys().identity_token_hmac_key().as_bytes(),
            expected_identity_token_hmac,
        })
    }
}

/// Cryptographic materials necessary for determining whether or not
/// a given V1 MIC extended salt advertisement section matches an identity.
#[derive(Clone)]
pub struct MicExtendedSaltSectionIdentityResolutionMaterial(SectionIdentityResolutionMaterial);

impl MicExtendedSaltSectionIdentityResolutionMaterial {
    /// Extracts the underlying section-identity resolution material carried around
    /// within this wrapper for resolution of MIC extended salt sections.
    pub(crate) fn into_raw_resolution_material(self) -> SectionIdentityResolutionMaterial {
        self.0
    }
    /// Gets the underlying section-identity resolution material carried around
    /// within this wrapper for resolution of MIC extended salt sections.
    #[cfg(any(test, feature = "devtools"))]
    pub(crate) fn as_raw_resolution_material(&self) -> &SectionIdentityResolutionMaterial {
        &self.0
    }
    #[cfg(test)]
    pub(crate) fn as_mut_raw_resolution_material(
        &mut self,
    ) -> &mut SectionIdentityResolutionMaterial {
        &mut self.0
    }
    /// Constructs identity-resolution material for an MIC extended salt section whose
    /// discovery credential leverages the provided HKDF and has the given
    /// expected metadata-key HMAC.
    pub(crate) fn from_hkdf_and_expected_identity_token_hmac<C: CryptoProvider>(
        hkdf: &np_hkdf::NpKeySeedHkdf<C>,
        expected_identity_token_hmac: [u8; 32],
    ) -> Self {
        Self(SectionIdentityResolutionMaterial {
            aes_key: hkdf.v1_mic_extended_salt_keys().aes_key(),
            identity_token_hmac_key: *hkdf
                .v1_mic_extended_salt_keys()
                .identity_token_hmac_key()
                .as_bytes(),
            expected_identity_token_hmac,
        })
    }
}

/// Cryptographic materials necessary for determining whether
/// a given V1 MIC short salt advertisement section matches an identity.
#[derive(Clone)]
pub struct MicShortSaltSectionIdentityResolutionMaterial(SectionIdentityResolutionMaterial);

impl MicShortSaltSectionIdentityResolutionMaterial {
    /// Extracts the underlying section-identity resolution material carried around
    /// within this wrapper for resolution of MIC short salt sections.
    pub(crate) fn into_raw_resolution_material(self) -> SectionIdentityResolutionMaterial {
        self.0
    }
    /// Gets the underlying section-identity resolution material carried around
    /// within this wrapper for resolution of MIC short salt sections.
    #[cfg(any(test, feature = "devtools"))]
    pub(crate) fn as_raw_resolution_material(&self) -> &SectionIdentityResolutionMaterial {
        &self.0
    }
    #[cfg(test)]
    pub(crate) fn as_mut_raw_resolution_material(
        &mut self,
    ) -> &mut SectionIdentityResolutionMaterial {
        &mut self.0
    }

    /// Constructs identity-resolution material for a MIC short salt section whose
    /// discovery credential leverages the provided HKDF and has the given
    /// expected metadata-key HMAC.
    pub(crate) fn from_hkdf_and_expected_identity_token_hmac<C: CryptoProvider>(
        hkdf: &np_hkdf::NpKeySeedHkdf<C>,
        expected_identity_token_hmac: [u8; 32],
    ) -> Self {
        Self(SectionIdentityResolutionMaterial {
            aes_key: hkdf.v1_mic_short_salt_keys().aes_key(),
            identity_token_hmac_key: *hkdf
                .v1_mic_short_salt_keys()
                .identity_token_hmac_key()
                .as_bytes(),
            expected_identity_token_hmac,
        })
    }
}

/// Crypto materials for V1 signed sections which are not employed in identity resolution,
/// but may be necessary to verify a signed section.
#[derive(Clone)]
pub struct SignedSectionVerificationMaterial {
    /// The np_ed25519 public key to be
    /// used for signature verification of signed sections.
    pub(crate) public_key: ed25519::PublicKey,
}

impl SignedSectionVerificationMaterial {
    /// Gets the np_ed25519 public key for the given identity,
    /// used for signature verification of signed sections.
    pub(crate) fn signature_verification_public_key(&self) -> ed25519::PublicKey {
        self.public_key.clone()
    }
}

/// Crypto materials for V1 MIC short salt sections which are not employed in identity resolution,
/// but may be necessary to fully decrypt a MIC short salt section.
#[derive(Clone)]
pub struct MicShortSaltSectionVerificationMaterial {
    /// The MIC HMAC key for verifying the integrity of MIC short salt sections.
    pub(crate) mic_hmac_key: [u8; 32],
}

impl MicSectionVerificationMaterial for MicShortSaltSectionVerificationMaterial {
    fn mic_hmac_key(&self) -> np_hkdf::NpHmacSha256Key {
        self.mic_hmac_key.into()
    }
}

/// Crypto materials for V1 MIC extended salt sections which are not employed in identity
/// resolution, but may be necessary to fully decrypt a MIC extended salt section.
#[derive(Clone)]
pub struct MicExtendedSaltSectionVerificationMaterial {
    /// The MIC HMAC key for verifying the integrity of MIC extended salt sections.
    pub(crate) mic_hmac_key: [u8; 32],
}

impl MicSectionVerificationMaterial for MicExtendedSaltSectionVerificationMaterial {
    fn mic_hmac_key(&self) -> np_hkdf::NpHmacSha256Key {
        self.mic_hmac_key.into()
    }
}

pub(crate) trait MicSectionVerificationMaterial {
    /// Returns the HMAC key for calculating the MIC of the sections
    fn mic_hmac_key(&self) -> np_hkdf::NpHmacSha256Key;
}

// Space-time tradeoffs:
// - Calculating an HKDF from the key seed costs about 2us on a gLinux laptop, and occupies 80b.
// - Calculating an AES (16b) or HMAC (32b) key from the HKDF costs about 700ns.
// The right tradeoff may also vary by use case. For frequently used identities we should
// probably pre-calculate everything. For occasionally used ones, or ones that are loaded from
// disk, used once, and discarded, we might want to precalculate on a separate thread or the
// like.
// The AES key and metadata key HMAC key are the most frequently used ones, as the MIC HMAC key
// is only used on the matching identity, not all identities.

/// Cryptographic material for an individual NP credential used to decrypt and verify v1 sections.
pub trait V1DiscoveryCryptoMaterial: DiscoveryMetadataCryptoMaterial<V1> {
    /// Constructs or copies the identity resolution material for signed sections
    fn signed_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> SignedSectionIdentityResolutionMaterial;

    /// Constructs or copies the identity resolution material for MIC short salt sections
    fn mic_short_salt_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> MicShortSaltSectionIdentityResolutionMaterial;

    /// Constructs or copies the identity resolution material for MIC extended salt sections
    fn mic_extended_salt_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> MicExtendedSaltSectionIdentityResolutionMaterial;

    /// Constructs or copies non-identity-resolution deserialization material for signed
    /// sections.
    fn signed_verification_material<C: CryptoProvider>(&self) -> SignedSectionVerificationMaterial;

    /// Constructs or copies non-identity-resolution deserialization material for MIC short salt
    /// sections.
    fn mic_short_salt_verification_material<C: CryptoProvider>(
        &self,
    ) -> MicShortSaltSectionVerificationMaterial;

    /// Constructs or copies non-identity-resolution deserialization material for MIC extended salt
    /// sections.
    fn mic_extended_salt_verification_material<C: CryptoProvider>(
        &self,
    ) -> MicExtendedSaltSectionVerificationMaterial;
}

/// [`V1DiscoveryCryptoMaterial`] that minimizes CPU time when providing key material at
/// the expense of occupied memory
pub struct PrecalculatedV1DiscoveryCryptoMaterial {
    pub(crate) signed_identity_resolution_material: SignedSectionIdentityResolutionMaterial,
    pub(crate) mic_short_salt_identity_resolution_material:
        MicShortSaltSectionIdentityResolutionMaterial,
    pub(crate) mic_extended_salt_identity_resolution_material:
        MicExtendedSaltSectionIdentityResolutionMaterial,
    pub(crate) signed_verification_material: SignedSectionVerificationMaterial,
    pub(crate) mic_short_salt_verification_material: MicShortSaltSectionVerificationMaterial,
    pub(crate) mic_extended_salt_verification_material: MicExtendedSaltSectionVerificationMaterial,
    pub(crate) metadata_nonce: [u8; 12],
}

impl DiscoveryMetadataCryptoMaterial<V1> for PrecalculatedV1DiscoveryCryptoMaterial {
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        self.metadata_nonce
    }
}

impl V1DiscoveryCryptoMaterial for PrecalculatedV1DiscoveryCryptoMaterial {
    fn signed_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> SignedSectionIdentityResolutionMaterial {
        self.signed_identity_resolution_material.clone()
    }

    fn mic_short_salt_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> MicShortSaltSectionIdentityResolutionMaterial {
        self.mic_short_salt_identity_resolution_material.clone()
    }

    fn mic_extended_salt_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> MicExtendedSaltSectionIdentityResolutionMaterial {
        self.mic_extended_salt_identity_resolution_material.clone()
    }
    fn signed_verification_material<C: CryptoProvider>(&self) -> SignedSectionVerificationMaterial {
        self.signed_verification_material.clone()
    }

    fn mic_short_salt_verification_material<C: CryptoProvider>(
        &self,
    ) -> MicShortSaltSectionVerificationMaterial {
        self.mic_short_salt_verification_material.clone()
    }

    fn mic_extended_salt_verification_material<C: CryptoProvider>(
        &self,
    ) -> MicExtendedSaltSectionVerificationMaterial {
        self.mic_extended_salt_verification_material.clone()
    }
}

// Implementations for reference types -- we don't provide a blanket impl for references
// due to the potential to conflict with downstream crates' implementations.

impl<'a> DiscoveryMetadataCryptoMaterial<V1> for &'a V1DiscoveryCredential {
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        (*self).metadata_nonce::<C>()
    }
}

impl<'a> V1DiscoveryCryptoMaterial for &'a V1DiscoveryCredential {
    fn signed_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> SignedSectionIdentityResolutionMaterial {
        (*self).signed_identity_resolution_material::<C>()
    }

    fn mic_short_salt_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> MicShortSaltSectionIdentityResolutionMaterial {
        (*self).mic_short_salt_identity_resolution_material::<C>()
    }

    fn mic_extended_salt_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> MicExtendedSaltSectionIdentityResolutionMaterial {
        (*self).mic_extended_salt_identity_resolution_material::<C>()
    }
    fn signed_verification_material<C: CryptoProvider>(&self) -> SignedSectionVerificationMaterial {
        (*self).signed_verification_material::<C>()
    }

    fn mic_short_salt_verification_material<C: CryptoProvider>(
        &self,
    ) -> MicShortSaltSectionVerificationMaterial {
        (*self).mic_short_salt_verification_material::<C>()
    }

    fn mic_extended_salt_verification_material<C: CryptoProvider>(
        &self,
    ) -> MicExtendedSaltSectionVerificationMaterial {
        (*self).mic_extended_salt_verification_material::<C>()
    }
}

impl<'a> DiscoveryMetadataCryptoMaterial<V1> for &'a PrecalculatedV1DiscoveryCryptoMaterial {
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        (*self).metadata_nonce::<C>()
    }
}

impl<'a> V1DiscoveryCryptoMaterial for &'a PrecalculatedV1DiscoveryCryptoMaterial {
    fn signed_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> SignedSectionIdentityResolutionMaterial {
        (*self).signed_identity_resolution_material::<C>()
    }

    fn mic_short_salt_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> MicShortSaltSectionIdentityResolutionMaterial {
        (*self).mic_short_salt_identity_resolution_material::<C>()
    }

    fn mic_extended_salt_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> MicExtendedSaltSectionIdentityResolutionMaterial {
        (*self).mic_extended_salt_identity_resolution_material::<C>()
    }
    fn signed_verification_material<C: CryptoProvider>(&self) -> SignedSectionVerificationMaterial {
        (*self).signed_verification_material::<C>()
    }

    fn mic_short_salt_verification_material<C: CryptoProvider>(
        &self,
    ) -> MicShortSaltSectionVerificationMaterial {
        (*self).mic_short_salt_verification_material::<C>()
    }

    fn mic_extended_salt_verification_material<C: CryptoProvider>(
        &self,
    ) -> MicExtendedSaltSectionVerificationMaterial {
        (*self).mic_extended_salt_verification_material::<C>()
    }
}

/// Crypto material for creating V1 sections.
#[derive(Clone)]
pub struct V1BroadcastCredential {
    /// The 32-byte key-seed used for generating other key material.
    pub key_seed: [u8; 32],

    /// The 16-byte identity-token which identifies the sender.
    pub identity_token: V1IdentityToken,

    /// The ed25519 private key to be used for signing section contents.
    pub private_key: ed25519::PrivateKey,
}

impl V1BroadcastCredential {
    /// Builds some simple V1 signed broadcast crypto-materials out of
    /// the provided key-seed, metadata-key, and ed25519 private key.
    pub fn new(
        key_seed: [u8; 32],
        identity_token: V1IdentityToken,
        private_key: ed25519::PrivateKey,
    ) -> Self {
        Self { key_seed, identity_token, private_key }
    }

    /// Key seed from which other keys are derived.
    pub(crate) fn key_seed(&self) -> [u8; 32] {
        self.key_seed
    }

    /// Identity token that will be incorporated into encrypted advertisements.
    pub(crate) fn identity_token(&self) -> V1IdentityToken {
        self.identity_token
    }

    /// Derive a discovery credential with the data necessary to discover advertisements produced
    /// by this broadcast credential.
    pub fn derive_discovery_credential<C: CryptoProvider>(&self) -> V1DiscoveryCredential {
        let key_seed = self.key_seed();
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&key_seed);

        V1DiscoveryCredential::new(
            key_seed,
            hkdf.v1_mic_short_salt_keys()
                .identity_token_hmac_key()
                .calculate_hmac::<C>(self.identity_token.as_slice()),
            hkdf.v1_mic_extended_salt_keys()
                .identity_token_hmac_key()
                .calculate_hmac::<C>(self.identity_token.as_slice()),
            hkdf.v1_signature_keys()
                .identity_token_hmac_key()
                .calculate_hmac::<C>(self.identity_token.as_slice()),
            self.signing_key().derive_public_key::<C::Ed25519>(),
        )
    }

    /// Key used for signature-protected sections
    pub(crate) fn signing_key(&self) -> ed25519::PrivateKey {
        self.private_key.clone()
    }
}
