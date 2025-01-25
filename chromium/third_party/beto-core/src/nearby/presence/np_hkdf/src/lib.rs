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

//! Wrappers around NP's usage of HKDF.
//!
//! All HKDF calls should happen in this module and expose the correct result type for
//! each derived key use case.

#![no_std]

#[cfg(feature = "std")]
extern crate std;

use crypto_provider::aead::Aead;
use crypto_provider::{aes, aes::Aes128Key, hkdf::Hkdf, hmac::Hmac, CryptoProvider};

pub mod v1_salt;

/// A wrapper around the common NP usage of HMAC-SHA256.
///
/// These are generally derived via HKDF, but could be used for any HMAC-SHA256 key.
#[derive(Clone, Debug)]
pub struct NpHmacSha256Key {
    /// Nearby Presence uses 32-byte HMAC keys.
    ///
    /// Inside the HMAC algorithm they will be padded to 64 bytes.
    key: [u8; 32],
}

impl NpHmacSha256Key {
    /// Build a fresh HMAC instance.
    ///
    /// Since each HMAC is modified as data is fed to it, HMACs should not be reused.
    ///
    /// See also [Self::calculate_hmac] for simple use cases.
    pub fn build_hmac<C: CryptoProvider>(&self) -> C::HmacSha256 {
        C::HmacSha256::new_from_key(self.key)
    }

    /// Returns a reference to the underlying key bytes.
    pub fn as_bytes(&self) -> &[u8; 32] {
        &self.key
    }

    /// Build an HMAC, update it with the provided `data`, and finalize it, returning the resulting
    /// MAC. This is convenient for one-and-done HMAC usage rather than incrementally accumulating
    /// the final MAC.
    pub fn calculate_hmac<C: CryptoProvider>(&self, data: &[u8]) -> [u8; 32] {
        let mut hmac = self.build_hmac::<C>();
        hmac.update(data);
        hmac.finalize()
    }

    /// Build an HMAC, update it with the provided `data`, and verify it.
    ///
    /// This is convenient for one-and-done HMAC usage rather than incrementally accumulating
    /// the final MAC.
    pub fn verify_hmac<C: CryptoProvider>(
        &self,
        data: &[u8],
        expected_mac: [u8; 32],
    ) -> Result<(), crypto_provider::hmac::MacError> {
        let mut hmac = self.build_hmac::<C>();
        hmac.update(data);
        hmac.verify(expected_mac)
    }
}

impl From<[u8; 32]> for NpHmacSha256Key {
    fn from(key: [u8; 32]) -> Self {
        Self { key }
    }
}

/// Salt use for all NP HKDFs
const NP_HKDF_SALT: &[u8] = b"Google Nearby";

/// A wrapper around an NP key seed for deriving HKDF-SHA256 sub keys.
pub struct NpKeySeedHkdf<C: CryptoProvider> {
    hkdf: NpHkdf<C>,
}

impl<C: CryptoProvider> NpKeySeedHkdf<C> {
    /// Build an HKDF from a NP credential key seed
    pub fn new(key_seed: &[u8; 32]) -> Self {
        Self { hkdf: NpHkdf::new(key_seed) }
    }

    /// LDT key used to decrypt a legacy advertisement
    #[allow(clippy::expect_used)]
    pub fn v0_ldt_key(&self) -> ldt::LdtKey<xts_aes::XtsAes128Key> {
        ldt::LdtKey::from_concatenated(
            &self.hkdf.derive_array(b"V0 LDT key").expect("LDT key is a valid length"),
        )
    }

    /// HMAC key used when verifying the raw identity token extracted from an advertisement
    pub fn v0_identity_token_hmac_key(&self) -> NpHmacSha256Key {
        self.hkdf.derive_hmac_sha256_key(b"V0 Identity token verification HMAC key")
    }

    /// AES-GCM nonce used when decrypting metadata
    #[allow(clippy::expect_used)]
    pub fn v0_metadata_nonce(&self) -> <C::Aes128Gcm as Aead>::Nonce {
        self.hkdf.derive_array(b"V0 Metadata nonce").expect("Nonce is a valid length")
    }

    /// AES-GCM nonce used when decrypting metadata.
    ///
    /// Shared between signed and unsigned since they use the same credential.
    #[allow(clippy::expect_used)]
    pub fn v1_metadata_nonce(&self) -> <C::Aes128Gcm as Aead>::Nonce {
        self.hkdf.derive_array(b"V1 Metadata nonce").expect("Nonce is a valid length")
    }

    /// Derived keys for MIC short salt sections
    pub fn v1_mic_short_salt_keys(&self) -> MicShortSaltSectionKeys<'_, C> {
        MicShortSaltSectionKeys { hkdf: &self.hkdf }
    }

    /// Derived keys for MIC extended salt sections
    pub fn v1_mic_extended_salt_keys(&self) -> MicExtendedSaltSectionKeys<'_, C> {
        MicExtendedSaltSectionKeys { hkdf: &self.hkdf }
    }

    /// Derived keys for MIC signature sections
    pub fn v1_signature_keys(&self) -> SignatureSectionKeys<'_, C> {
        SignatureSectionKeys { hkdf: &self.hkdf }
    }
}

/// Derived keys for MIC short salt sections
pub struct MicShortSaltSectionKeys<'a, C: CryptoProvider> {
    hkdf: &'a NpHkdf<C>,
}

impl<'a, C: CryptoProvider> MicShortSaltSectionKeys<'a, C> {
    /// HMAC-SHA256 key used when verifying a section's ciphertext
    pub fn mic_hmac_key(&self) -> NpHmacSha256Key {
        self.hkdf.derive_hmac_sha256_key(b"MIC Section short salt HMAC key")
    }
}

impl<'a, C: CryptoProvider> DerivedSectionKeys<C> for MicShortSaltSectionKeys<'a, C> {
    fn aes_key(&self) -> Aes128Key {
        self.hkdf.derive_aes128_key(b"MIC Section short salt AES key")
    }

    fn identity_token_hmac_key(&self) -> NpHmacSha256Key {
        self.hkdf.derive_hmac_sha256_key(b"MIC Section short salt identity token HMAC key")
    }
}

/// Derived keys for MIC extended salt sections
pub struct MicExtendedSaltSectionKeys<'a, C: CryptoProvider> {
    hkdf: &'a NpHkdf<C>,
}

impl<'a, C: CryptoProvider> MicExtendedSaltSectionKeys<'a, C> {
    /// HMAC-SHA256 key used when verifying a section's ciphertext
    pub fn mic_hmac_key(&self) -> NpHmacSha256Key {
        self.hkdf.derive_hmac_sha256_key(b"MIC Section extended salt HMAC key")
    }
}

impl<'a, C: CryptoProvider> DerivedSectionKeys<C> for MicExtendedSaltSectionKeys<'a, C> {
    fn aes_key(&self) -> Aes128Key {
        self.hkdf.derive_aes128_key(b"MIC Section extended salt AES key")
    }

    fn identity_token_hmac_key(&self) -> NpHmacSha256Key {
        self.hkdf.derive_hmac_sha256_key(b"MIC Section extended salt identity token HMAC key")
    }
}

/// Derived keys for Signature sections
pub struct SignatureSectionKeys<'a, C: CryptoProvider> {
    hkdf: &'a NpHkdf<C>,
}

impl<'a, C: CryptoProvider> DerivedSectionKeys<C> for SignatureSectionKeys<'a, C> {
    fn aes_key(&self) -> Aes128Key {
        self.hkdf.derive_aes128_key(b"Signature Section AES key")
    }

    fn identity_token_hmac_key(&self) -> NpHmacSha256Key {
        self.hkdf.derive_hmac_sha256_key(b"Signature Section identity token HMAC key")
    }
}

/// Derived keys for encrypted V1 sections
pub trait DerivedSectionKeys<C: CryptoProvider> {
    /// AES128 key used when encrypting a section's ciphertext
    fn aes_key(&self) -> Aes128Key;

    /// HMAC-SHA256 key used when verifying a section's plaintext identity token
    fn identity_token_hmac_key(&self) -> NpHmacSha256Key;
}

/// Expand a legacy salt into the expanded salt used with XOR padding in LDT.
#[allow(clippy::expect_used)]
pub fn v0_ldt_expanded_salt<C: CryptoProvider>(salt: &[u8; 2]) -> [u8; aes::BLOCK_SIZE] {
    simple_np_hkdf_expand::<{ aes::BLOCK_SIZE }, C>(salt, b"V0 LDT salt pad")
        // XTS tweak size is the block cipher's block size
        .expect("Tweak size is a valid HKDF size")
}

/// Expand a v0 identity token into an AES128 metadata key.
#[allow(clippy::expect_used)]
pub fn v0_metadata_expanded_key<C: CryptoProvider>(identity_token: &[u8; 14]) -> [u8; 16] {
    simple_np_hkdf_expand::<16, C>(identity_token, b"V0 Metadata key expansion")
        .expect("AES128 key is a valid HKDF size")
}

/// Expand an extended MIC section's short salt into an AES-CTR nonce.
#[allow(clippy::expect_used)]
pub fn extended_mic_section_short_salt_nonce<C: CryptoProvider>(
    salt: [u8; 2],
) -> aes::ctr::AesCtrNonce {
    simple_np_hkdf_expand::<{ aes::ctr::AES_CTR_NONCE_LEN }, C>(
        salt.as_slice(),
        b"MIC Section short salt nonce",
    )
    .expect("AES-CTR nonce is a valid HKDF size")
}

/// Build an HKDF using the NP HKDF salt, calculate output, and discard the HKDF.
/// If using the NP key seed as IKM, see [NpKeySeedHkdf] instead.
///
/// Returns None if the requested size is > 255 * 32 bytes.
fn simple_np_hkdf_expand<const N: usize, C: CryptoProvider>(
    ikm: &[u8],
    info: &[u8],
) -> Option<[u8; N]> {
    let mut buf = [0; N];
    let hkdf = np_salt_hkdf::<C>(ikm);
    hkdf.expand(info, &mut buf[..]).map(|_| buf).ok()
}

/// Construct an HKDF with the Nearby Presence salt and provided `ikm`
fn np_salt_hkdf<C: CryptoProvider>(ikm: &[u8]) -> C::HkdfSha256 {
    C::HkdfSha256::new(Some(NP_HKDF_SALT), ikm)
}

/// NP-flavored HKDF operations for common derived output types
struct NpHkdf<C: CryptoProvider> {
    hkdf: C::HkdfSha256,
}

impl<C: CryptoProvider> NpHkdf<C> {
    /// Build an HKDF using the NP HKDF salt and supplied `ikm`
    pub fn new(ikm: &[u8]) -> Self {
        Self { hkdf: np_salt_hkdf::<C>(ikm) }
    }

    /// Derive a length `N` array using the provided `info`
    /// Returns `None` if N > 255 * 32.
    pub fn derive_array<const N: usize>(&self, info: &[u8]) -> Option<[u8; N]> {
        let mut arr = [0_u8; N];
        self.hkdf.expand(info, &mut arr).map(|_| arr).ok()
    }

    /// Derive an HMAC-SHA256 key using the provided `info`
    #[allow(clippy::expect_used)]
    pub fn derive_hmac_sha256_key(&self, info: &[u8]) -> NpHmacSha256Key {
        self.derive_array(info).expect("HMAC-SHA256 keys are a valid length").into()
    }
    /// Derive an AES-128 key using the provided `info`
    #[allow(clippy::expect_used)]
    pub fn derive_aes128_key(&self, info: &[u8]) -> Aes128Key {
        self.derive_array(info).expect("AES128 keys are a valid length").into()
    }
}
