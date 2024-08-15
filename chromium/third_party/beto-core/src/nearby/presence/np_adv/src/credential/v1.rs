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

use crate::credential::{
    protocol_version_seal, BroadcastCryptoMaterial, DiscoveryCryptoMaterial, ProtocolVersion,
};
use crate::MetadataKey;
use crypto_provider::{aes::Aes128Key, ed25519, ed25519::PublicKey, CryptoProvider, CryptoRng};
use np_hkdf::UnsignedSectionKeys;

/// Cryptographic information about a particular V1 discovery credential
/// necessary to match and decrypt encrypted V1 sections.
#[derive(Clone)]
pub struct V1DiscoveryCredential {
    key_seed: [u8; 32],
    expected_unsigned_metadata_key_hmac: [u8; 32],
    expected_signed_metadata_key_hmac: [u8; 32],
    pub_key: ed25519::RawPublicKey,
}
impl V1DiscoveryCredential {
    /// Construct a V1 discovery credential from the provided identity data.
    pub fn new(
        key_seed: [u8; 32],
        expected_unsigned_metadata_key_hmac: [u8; 32],
        expected_signed_metadata_key_hmac: [u8; 32],
        pub_key: ed25519::RawPublicKey,
    ) -> Self {
        Self {
            key_seed,
            expected_unsigned_metadata_key_hmac,
            expected_signed_metadata_key_hmac,
            pub_key,
        }
    }

    /// Constructs pre-calculated crypto material from this discovery credential.
    pub(crate) fn to_precalculated<C: CryptoProvider>(
        &self,
    ) -> PrecalculatedV1DiscoveryCryptoMaterial {
        let signed_identity_resolution_material = self.signed_identity_resolution_material::<C>();
        let unsigned_identity_resolution_material =
            self.unsigned_identity_resolution_material::<C>();
        let signed_verification_material = self.signed_verification_material::<C>();
        let unsigned_verification_material = self.unsigned_verification_material::<C>();
        let metadata_nonce = self.metadata_nonce::<C>();
        PrecalculatedV1DiscoveryCryptoMaterial {
            signed_identity_resolution_material,
            unsigned_identity_resolution_material,
            signed_verification_material,
            unsigned_verification_material,
            metadata_nonce,
        }
    }
}

impl DiscoveryCryptoMaterial<V1> for V1DiscoveryCredential {
    fn metadata_nonce<C: CryptoProvider>(&self) -> [u8; 12] {
        V1::metadata_nonce_from_key_seed::<C>(&self.key_seed)
    }
}

impl V1DiscoveryCryptoMaterial for V1DiscoveryCredential {
    fn signed_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> SignedSectionIdentityResolutionMaterial {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&self.key_seed);
        SignedSectionIdentityResolutionMaterial::from_hkdf_and_expected_metadata_key_hmac(
            &hkdf,
            self.expected_signed_metadata_key_hmac,
        )
    }

    fn unsigned_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> UnsignedSectionIdentityResolutionMaterial {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&self.key_seed);
        UnsignedSectionIdentityResolutionMaterial::from_hkdf_and_expected_metadata_key_hmac(
            &hkdf,
            self.expected_unsigned_metadata_key_hmac,
        )
    }

    fn signed_verification_material<C: CryptoProvider>(&self) -> SignedSectionVerificationMaterial {
        SignedSectionVerificationMaterial { pub_key: self.pub_key }
    }

    fn unsigned_verification_material<C: CryptoProvider>(
        &self,
    ) -> UnsignedSectionVerificationMaterial {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&self.key_seed);
        let mic_hmac_key = *UnsignedSectionKeys::hmac_key(&hkdf).as_bytes();
        UnsignedSectionVerificationMaterial { mic_hmac_key }
    }
}

/// Type-level identifier for the V1 protocol version.
#[derive(Debug, Clone)]
pub enum V1 {}

impl protocol_version_seal::ProtocolVersionSeal for V1 {}

impl ProtocolVersion for V1 {
    type DiscoveryCredential = V1DiscoveryCredential;
    type MetadataKey = MetadataKey;

    fn metadata_nonce_from_key_seed<C: CryptoProvider>(key_seed: &[u8; 32]) -> [u8; 12] {
        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(key_seed);
        hkdf.extended_metadata_nonce()
    }
    fn expand_metadata_key<C: CryptoProvider>(metadata_key: MetadataKey) -> MetadataKey {
        metadata_key
    }
    fn gen_random_metadata_key<R: CryptoRng>(rng: &mut R) -> MetadataKey {
        MetadataKey(rng.gen())
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
    pub(crate) aes_key: Aes128Key,
    /// The metadata key HMAC key for deriving and verifying the identity metadata
    /// key HMAC against the expected value.
    pub(crate) metadata_key_hmac_key: [u8; 32],
    /// The expected metadata key HMAC to check against for an identity match.
    pub(crate) expected_metadata_key_hmac: [u8; 32],
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
    pub(crate) fn from_hkdf_and_expected_metadata_key_hmac<C: CryptoProvider>(
        hkdf: &np_hkdf::NpKeySeedHkdf<C>,
        expected_metadata_key_hmac: [u8; 32],
    ) -> Self {
        Self(SectionIdentityResolutionMaterial {
            aes_key: hkdf.extended_signed_section_aes_key(),
            metadata_key_hmac_key: *hkdf.extended_signed_metadata_key_hmac_key().as_bytes(),
            expected_metadata_key_hmac,
        })
    }
}

/// Cryptographic materials necessary for determining whether or not
/// a given V1 MIC advertisement section matches an identity.
#[derive(Clone)]
pub struct UnsignedSectionIdentityResolutionMaterial(SectionIdentityResolutionMaterial);

impl UnsignedSectionIdentityResolutionMaterial {
    #[cfg(test)]
    pub(crate) fn from_raw(raw: SectionIdentityResolutionMaterial) -> Self {
        Self(raw)
    }
    /// Extracts the underlying section-identity resolution material carried around
    /// within this wrapper for resolution of unsigned sections.
    pub(crate) fn into_raw_resolution_material(self) -> SectionIdentityResolutionMaterial {
        self.0
    }
    /// Gets the underlying section-identity resolution material carried around
    /// within this wrapper for resolution of unsigned sections.
    #[cfg(any(test, feature = "devtools"))]
    pub(crate) fn as_raw_resolution_material(&self) -> &SectionIdentityResolutionMaterial {
        &self.0
    }
    /// Constructs identity-resolution material for an unsigned (MIC) section whose
    /// discovery credential leverages the provided HKDF and has the given
    /// expected metadata-key HMAC.
    pub(crate) fn from_hkdf_and_expected_metadata_key_hmac<C: CryptoProvider>(
        hkdf: &np_hkdf::NpKeySeedHkdf<C>,
        expected_metadata_key_hmac: [u8; 32],
    ) -> Self {
        Self(SectionIdentityResolutionMaterial {
            aes_key: UnsignedSectionKeys::aes_key(hkdf),
            metadata_key_hmac_key: *hkdf.extended_unsigned_metadata_key_hmac_key().as_bytes(),
            expected_metadata_key_hmac,
        })
    }
}

/// Crypto materials for V1 signed sections which are not employed in identity resolution,
/// but may be necessary to verify a signed section.
#[derive(Clone)]
pub struct SignedSectionVerificationMaterial {
    /// The np_ed25519 public key to be
    /// used for signature verification of signed sections.
    pub(crate) pub_key: ed25519::RawPublicKey,
}

impl SignedSectionVerificationMaterial {
    /// Gets the np_ed25519 public key for the given identity,
    /// used for signature verification of signed sections.
    pub(crate) fn signature_verification_public_key<C: CryptoProvider>(
        &self,
    ) -> np_ed25519::PublicKey<C> {
        np_ed25519::PublicKey::from_bytes(&self.pub_key).expect("Should only contain valid keys")
    }
}

/// Crypto materials for V1 unsigned sections which are not employed in identity resolution,
/// but may be necessary to fully decrypt an unsigned section.
#[derive(Clone)]
pub struct UnsignedSectionVerificationMaterial {
    /// The MIC HMAC key for verifying the integrity of unsigned sections.
    pub(crate) mic_hmac_key: [u8; 32],
}

impl UnsignedSectionVerificationMaterial {
    /// Returns the MIC HMAC key for unsigned sections
    pub(crate) fn mic_hmac_key<C: CryptoProvider>(&self) -> np_hkdf::NpHmacSha256Key<C> {
        self.mic_hmac_key.into()
    }
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
pub trait V1DiscoveryCryptoMaterial: DiscoveryCryptoMaterial<V1> {
    /// Constructs or copies the identity resolution material for signed sections
    fn signed_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> SignedSectionIdentityResolutionMaterial;

    /// Constructs or copies the identity resolution material for unsigned sections
    fn unsigned_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> UnsignedSectionIdentityResolutionMaterial;

    /// Constructs or copies non-identity-resolution deserialization material for signed
    /// sections.
    fn signed_verification_material<C: CryptoProvider>(&self) -> SignedSectionVerificationMaterial;

    /// Constructs or copies non-identity-resolution deserialization material for unsigned
    /// sections.
    fn unsigned_verification_material<C: CryptoProvider>(
        &self,
    ) -> UnsignedSectionVerificationMaterial;
}

/// V1 [`DiscoveryCryptoMaterial`] that minimizes CPU time when providing key material at
/// the expense of occupied memory
pub struct PrecalculatedV1DiscoveryCryptoMaterial {
    pub(crate) signed_identity_resolution_material: SignedSectionIdentityResolutionMaterial,
    pub(crate) unsigned_identity_resolution_material: UnsignedSectionIdentityResolutionMaterial,
    pub(crate) signed_verification_material: SignedSectionVerificationMaterial,
    pub(crate) unsigned_verification_material: UnsignedSectionVerificationMaterial,
    pub(crate) metadata_nonce: [u8; 12],
}

impl DiscoveryCryptoMaterial<V1> for PrecalculatedV1DiscoveryCryptoMaterial {
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
    fn unsigned_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> UnsignedSectionIdentityResolutionMaterial {
        self.unsigned_identity_resolution_material.clone()
    }
    fn signed_verification_material<C: CryptoProvider>(&self) -> SignedSectionVerificationMaterial {
        self.signed_verification_material.clone()
    }
    fn unsigned_verification_material<C: CryptoProvider>(
        &self,
    ) -> UnsignedSectionVerificationMaterial {
        self.unsigned_verification_material.clone()
    }
}

// Implementations for reference types -- we don't provide a blanket impl for references
// due to the potential to conflict with downstream crates' implementations.

impl<'a> DiscoveryCryptoMaterial<V1> for &'a V1DiscoveryCredential {
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
    fn unsigned_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> UnsignedSectionIdentityResolutionMaterial {
        (*self).unsigned_identity_resolution_material::<C>()
    }
    fn signed_verification_material<C: CryptoProvider>(&self) -> SignedSectionVerificationMaterial {
        (*self).signed_verification_material::<C>()
    }
    fn unsigned_verification_material<C: CryptoProvider>(
        &self,
    ) -> UnsignedSectionVerificationMaterial {
        (*self).unsigned_verification_material::<C>()
    }
}

impl<'a> DiscoveryCryptoMaterial<V1> for &'a PrecalculatedV1DiscoveryCryptoMaterial {
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
    fn unsigned_identity_resolution_material<C: CryptoProvider>(
        &self,
    ) -> UnsignedSectionIdentityResolutionMaterial {
        (*self).unsigned_identity_resolution_material::<C>()
    }
    fn signed_verification_material<C: CryptoProvider>(&self) -> SignedSectionVerificationMaterial {
        (*self).signed_verification_material::<C>()
    }
    fn unsigned_verification_material<C: CryptoProvider>(
        &self,
    ) -> UnsignedSectionVerificationMaterial {
        (*self).unsigned_verification_material::<C>()
    }
}

/// Extension of [`BroadcastCryptoMaterial`] for `V1` to add
/// crypto-materials which are necessary to sign V1 sections.
pub trait SignedBroadcastCryptoMaterial: BroadcastCryptoMaterial<V1> {
    /// Gets the advertisement-signing private key for constructing
    /// signature-verified V1 sections.
    ///
    /// The private key is returned in an opaque, crypto-provider-independent
    /// form to provide a safeguard against leaking the bytes of the key.
    fn signing_key(&self) -> ed25519::PrivateKey;

    /// Constructs the V1 discovery credential which may be used to discover
    /// V1 advertisement sections broadcasted using this broadcast crypto-material
    fn derive_v1_discovery_credential<C: CryptoProvider>(&self) -> V1DiscoveryCredential {
        let key_seed = self.key_seed();
        let metadata_key = self.metadata_key();
        let pub_key = self.signing_key().derive_public_key::<C::Ed25519>();
        let pub_key = pub_key.to_bytes();

        let hkdf = np_hkdf::NpKeySeedHkdf::<C>::new(&key_seed);
        let unsigned = hkdf
            .extended_unsigned_metadata_key_hmac_key()
            .calculate_hmac(metadata_key.0.as_slice());
        let signed =
            hkdf.extended_signed_metadata_key_hmac_key().calculate_hmac(metadata_key.0.as_slice());
        V1DiscoveryCredential::new(key_seed, unsigned, signed, pub_key)
    }
}

/// Concrete implementation of a [`SignedBroadcastCryptoMaterial`] which keeps the key
/// seed, the V1 metadata key, and the signing key contiguous in memory.
///
/// For more flexible expression of broadcast
/// credentials, feel free to directly implement [`SignedBroadcastCryptoMaterial`]
/// for your own broadcast-credential-storing data-type.
pub struct SimpleSignedBroadcastCryptoMaterial {
    key_seed: [u8; 32],
    metadata_key: MetadataKey,
    signing_key: ed25519::PrivateKey,
}

impl SimpleSignedBroadcastCryptoMaterial {
    /// Builds some simple V1 signed broadcast crypto-materials out of
    /// the provided key-seed, metadata-key, and signing key.
    pub fn new(
        key_seed: [u8; 32],
        metadata_key: MetadataKey,
        signing_key: ed25519::PrivateKey,
    ) -> Self {
        Self { key_seed, metadata_key, signing_key }
    }
}

impl BroadcastCryptoMaterial<V1> for SimpleSignedBroadcastCryptoMaterial {
    fn key_seed(&self) -> [u8; 32] {
        self.key_seed
    }
    fn metadata_key(&self) -> MetadataKey {
        self.metadata_key
    }
}

impl SignedBroadcastCryptoMaterial for SimpleSignedBroadcastCryptoMaterial {
    fn signing_key(&self) -> ed25519::PrivateKey {
        self.signing_key.clone()
    }
}
