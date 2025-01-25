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

//! Nearby Presence-specific usage of LDT.
#![no_std]

#[cfg(feature = "std")]
extern crate std;

#[cfg(test)]
mod np_adv_test_vectors;
#[cfg(test)]
mod tests;

use array_view::ArrayView;
use core::fmt;
use core::ops;
use crypto_provider::{aes::BLOCK_SIZE, CryptoProvider, CryptoRng, FromCryptoRng};
use ldt::{LdtCipher, LdtDecryptCipher, LdtEncryptCipher, LdtError, Swap, XorPadder};
use np_hkdf::{v0_ldt_expanded_salt, NpHmacSha256Key, NpKeySeedHkdf};
use xts_aes::XtsAes128;

/// Max LDT-XTS-AES data size: `(2 * AES block size) - 1`
pub const LDT_XTS_AES_MAX_LEN: usize = 31;

/// V0 format uses a 14-byte identity token
pub const V0_IDENTITY_TOKEN_LEN: usize = 14;

/// Max payload size once identity token prefix has been removed
pub const NP_LDT_MAX_EFFECTIVE_PAYLOAD_LEN: usize = LDT_XTS_AES_MAX_LEN - V0_IDENTITY_TOKEN_LEN;

/// Length of a V0 advertisement salt
pub const V0_SALT_LEN: usize = 2;

/// The salt included in a V0 NP advertisement.
/// LDT does not use an IV but can instead incorporate the 2 byte, regularly rotated,
/// salt from the advertisement payload and XOR it with the padded tweak data.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct V0Salt {
    /// Salt bytes extracted from the incoming NP advertisement
    bytes: [u8; V0_SALT_LEN],
}

impl V0Salt {
    /// Returns the salt as a byte array.
    pub fn bytes(&self) -> [u8; V0_SALT_LEN] {
        self.bytes
    }
}

impl From<[u8; V0_SALT_LEN]> for V0Salt {
    fn from(arr: [u8; V0_SALT_LEN]) -> Self {
        Self { bytes: arr }
    }
}

/// "Short" 14-byte identity token type employed for V0
#[derive(Debug, Clone, Copy, Hash, PartialEq, Eq)]
pub struct V0IdentityToken([u8; V0_IDENTITY_TOKEN_LEN]);

impl V0IdentityToken {
    /// Constructs a V0 identity token from raw bytes.
    pub const fn new(value: [u8; V0_IDENTITY_TOKEN_LEN]) -> Self {
        Self(value)
    }
    /// Returns the underlying bytes
    pub fn bytes(&self) -> [u8; V0_IDENTITY_TOKEN_LEN] {
        self.0
    }

    /// Returns the token bytes as a slice
    pub fn as_slice(&self) -> &[u8] {
        &self.0
    }
}

impl From<[u8; V0_IDENTITY_TOKEN_LEN]> for V0IdentityToken {
    fn from(value: [u8; V0_IDENTITY_TOKEN_LEN]) -> Self {
        Self(value)
    }
}

impl AsRef<[u8]> for V0IdentityToken {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl FromCryptoRng for V0IdentityToken {
    fn new_random<R: CryptoRng>(rng: &mut R) -> Self {
        Self(rng.gen())
    }
}

/// [LdtEncryptCipher] parameterized for XTS-AES-128 with the [Swap] mix function.
pub type NpLdtEncryptCipher<C> = LdtEncryptCipher<{ BLOCK_SIZE }, XtsAes128<C>, Swap>;

/// [LdtDecryptCipher] parameterized for XTS-AES-128 with the [Swap] mix function.
type NpLdtDecryptCipher<C> = LdtDecryptCipher<{ BLOCK_SIZE }, XtsAes128<C>, Swap>;

/// Range of valid NP LDT message lengths for encryption/decryption, in a convenient form that
/// doesn't need a CryptoProvider parameter.
pub const VALID_INPUT_LEN: ops::Range<usize> = BLOCK_SIZE..BLOCK_SIZE * 2;

/// Build a Nearby Presence specific LDT XTS-AES-128 decrypter from a provided [NpKeySeedHkdf] and
/// metadata_key_hmac, with the [Swap] mix function
pub fn build_np_adv_decrypter_from_key_seed<C: CryptoProvider>(
    key_seed: &NpKeySeedHkdf<C>,
    identity_token_hmac: [u8; 32],
) -> AuthenticatedNpLdtDecryptCipher<C> {
    build_np_adv_decrypter(
        &key_seed.v0_ldt_key(),
        identity_token_hmac,
        key_seed.v0_identity_token_hmac_key(),
    )
}

/// Build a Nearby Presence specific LDT XTS-AES-128 decrypter from precalculated cipher components,
/// with the [Swap] mix function
pub fn build_np_adv_decrypter<C: CryptoProvider>(
    ldt_key: &ldt::LdtKey<xts_aes::XtsAes128Key>,
    identity_token_hmac: [u8; 32],
    identity_token_hmac_key: NpHmacSha256Key,
) -> AuthenticatedNpLdtDecryptCipher<C> {
    AuthenticatedNpLdtDecryptCipher {
        ldt_decrypter: NpLdtDecryptCipher::<C>::new(ldt_key),
        metadata_key_tag: identity_token_hmac,
        metadata_key_hmac_key: identity_token_hmac_key,
    }
}

/// Decrypts and validates a NP legacy format advertisement encrypted with LDT.
///
/// A NP legacy advertisement will always be in the format of:
///
/// Header (1 byte) | Salt (2 bytes) | Identity token (14 bytes) | repeated
/// { DE header | DE payload }
///
/// Example:
/// Header (1 byte) | Salt (2 bytes) | Identity token (14 bytes) |
/// Tx power DE header (1 byte) | Tx power (1 byte) | Action DE header(1 byte) | action (1-3 bytes)
///
/// The ciphertext bytes will always start with the Identity through the end of the
/// advertisement, for example in the above [ Identity (14 bytes) | Tx power DE header (1 byte) |
/// Tx power (1 byte) | Action DE header(1 byte) | action (1-3 bytes) ] will be the ciphertext section
/// passed as the input to `decrypt_and_verify`
///
/// `B` is the underlying block cipher block size.
/// `O` is the max output size (must be 2 * B - 1).
/// `T` is the tweakable block cipher used by LDT.
/// `M` is the mix function used by LDT.
pub struct AuthenticatedNpLdtDecryptCipher<C: CryptoProvider> {
    ldt_decrypter: LdtDecryptCipher<BLOCK_SIZE, XtsAes128<C>, Swap>,
    metadata_key_tag: [u8; 32],
    metadata_key_hmac_key: NpHmacSha256Key,
}

impl<C: CryptoProvider> AuthenticatedNpLdtDecryptCipher<C> {
    /// Decrypt an advertisement payload using the provided padder.
    ///
    /// If the plaintext's identity token matches this decrypter's MAC, returns the verified identity
    /// token and the remaining plaintext (the bytes after the identity token).
    ///
    /// NOTE: because LDT acts as a PRP over the entire message, tampering with any bit scrambles
    /// the whole message, so we can leverage the MAC on just the metadata key to ensure integrity
    /// for the whole message.
    ///
    /// # Errors
    /// - If `payload` has a length outside `[BLOCK_SIZE, BLOCK_SIZE * 2)`.
    /// - If the decrypted plaintext fails its HMAC validation
    #[allow(clippy::expect_used, clippy::indexing_slicing)]
    pub fn decrypt_and_verify(
        &self,
        payload: &[u8],
        padder: &XorPadder<BLOCK_SIZE>,
    ) -> Result<
        (V0IdentityToken, ArrayView<u8, NP_LDT_MAX_EFFECTIVE_PAYLOAD_LEN>),
        LdtAdvDecryptError,
    > {
        // we copy to avoid exposing plaintext that hasn't been validated w/ hmac
        let mut buffer = [0_u8; LDT_XTS_AES_MAX_LEN];
        let populated_buffer = buffer
            .get_mut(..payload.len())
            .ok_or(LdtAdvDecryptError::InvalidLength(payload.len()))?;
        populated_buffer.copy_from_slice(payload);

        self.ldt_decrypter.decrypt(populated_buffer, padder).map_err(|e| match e {
            LdtError::InvalidLength(l) => LdtAdvDecryptError::InvalidLength(l),
        })?;
        // slice is safe since input is a valid LDT-XTS-AES len
        let identity_token = &populated_buffer[..V0_IDENTITY_TOKEN_LEN];
        self.metadata_key_hmac_key
            .verify_hmac::<C>(identity_token, self.metadata_key_tag)
            .map_err(|_| LdtAdvDecryptError::MacMismatch)?;

        let token_arr: [u8; V0_IDENTITY_TOKEN_LEN] =
            identity_token.try_into().expect("Length verified above");
        Ok((
            token_arr.into(),
            ArrayView::try_from_slice(&buffer[V0_IDENTITY_TOKEN_LEN..payload.len()])
                .expect("Buffer len less token len is the max output len"),
        ))
    }
}

/// Errors that can occur during [AuthenticatedNpLdtDecryptCipher::decrypt_and_verify].
#[derive(Debug, PartialEq, Eq)]
pub enum LdtAdvDecryptError {
    /// The ciphertext data was an invalid length.
    InvalidLength(usize),
    /// The MAC calculated from the plaintext did not match the expected value
    MacMismatch,
}

impl fmt::Display for LdtAdvDecryptError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            LdtAdvDecryptError::InvalidLength(len) => {
                write!(f, "Adv decrypt error: invalid length ({len})")
            }
            LdtAdvDecryptError::MacMismatch => write!(f, "Adv decrypt error: MAC mismatch"),
        }
    }
}

/// Build a XorPadder by HKDFing the NP advertisement salt
pub fn salt_padder<C: CryptoProvider>(salt: V0Salt) -> XorPadder<BLOCK_SIZE> {
    XorPadder::from(v0_ldt_expanded_salt::<C>(&salt.bytes))
}
