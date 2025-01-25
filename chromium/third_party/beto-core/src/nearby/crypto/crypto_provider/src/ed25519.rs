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

use core::fmt::Debug;

// User-facing, crypto-provider independent structs

/// The length of a ed25519 `Signature`, in bytes.
pub const SIGNATURE_LENGTH: usize = 64;

/// The length of an ed25519 `PrivateKey`, in bytes.
pub const PRIVATE_KEY_LENGTH: usize = 32;

/// The length of an ed25519 `PrivateKey`, in bytes.
pub const PUBLIC_KEY_LENGTH: usize = 32;

/// A byte buffer the size of a ed25519 `Signature`.
pub type RawSignature = [u8; SIGNATURE_LENGTH];

/// A byte buffer the size of a ed25519 `PublicKey`.
pub type RawPublicKey = [u8; PUBLIC_KEY_LENGTH];

/// A byte buffer the size of a ed25519 `PrivateKey`.
pub type RawPrivateKey = [u8; PRIVATE_KEY_LENGTH];

/// A permission token which may be supplied to methods which allow
/// converting private keys to/from raw bytes.
///
/// In general, operations of this kind should only be done in
/// development-tools, tests, or in credential storage layers
/// to prevent accidental exposure of the private key.
pub struct RawPrivateKeyPermit {
    _marker: (),
}

impl RawPrivateKeyPermit {
    pub(crate) fn new() -> Self {
        Self { _marker: () }
    }
}

#[cfg(feature = "raw_private_key_permit")]
impl core::default::Default for RawPrivateKeyPermit {
    fn default() -> Self {
        Self::new()
    }
}

/// A crypto-provider-independent representation of the private
/// key of an ed25519 key-pair, kept in such a way that
/// it does not permit de-structuring it into raw bytes,
/// nor constructing one from raw bytes.
///
/// Useful for when you want a data-structure to be
/// crypto-provider independent and contain a private key.
#[derive(Clone)]
pub struct PrivateKey(RawPrivateKey);

impl PrivateKey {
    /// Derives the public key corresponding to this private key.
    pub fn derive_public_key<E: Ed25519Provider>(&self) -> PublicKey {
        let key_pair = E::KeyPair::from_private_key(self);
        key_pair.public_key().to_external()
    }
    /// Sign the given message and return a digital signature
    pub fn sign<E: Ed25519Provider>(&self, msg: &[u8]) -> Signature {
        let key_pair = E::KeyPair::from_private_key(self);
        key_pair.sign(msg).to_external()
    }
    /// Generate an ed25519 private key from a CSPRNG
    /// generate is not available in `no-std`.
    #[cfg(feature = "std")]
    pub fn generate<E: Ed25519Provider>() -> Self {
        let key_pair = E::KeyPair::generate();
        key_pair.private_key()
    }

    /// Returns the raw bytes of this private key.
    /// This operation is only possible while holding a [`RawPrivateKeyPermit`].
    pub fn raw_private_key(&self, _permit: &RawPrivateKeyPermit) -> RawPrivateKey {
        self.0
    }
    /// Constructs a private key from the raw bytes of the key.
    /// This operation is only possible while holding a [`RawPrivateKeyPermit`].
    pub fn from_raw_private_key(wrapped: RawPrivateKey, _permit: &RawPrivateKeyPermit) -> Self {
        PrivateKey(wrapped)
    }
}

/// error returned when bad bytes are provided to generate keypair
#[derive(Debug)]
pub struct InvalidPublicKeyBytes;

/// Error returned if the verification on the signature + message fails
#[derive(Debug)]
pub struct SignatureError;

/// A crypto-provider-independent representation of a valid
/// public key for an ed25519 key-pair in the Edwards Y-format.
///
/// Useful for when you want a data-structure to be crypto-provider
/// independent and contain a public key.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PublicKey(RawPublicKey);

impl PublicKey {
    /// Attempts to parse a public key from an array of bytes.
    /// If the input is not in the Edwards Y-format, this method
    /// will yield an `InvalidPublicKeyBytes` error.
    pub fn from_bytes<E: Ed25519Provider>(
        wrapped: RawPublicKey,
    ) -> Result<Self, InvalidPublicKeyBytes> {
        // Simply verify that we can construct the crypto-provider-dependent variant.
        let _ = <E::PublicKey as PublicKeyImpl>::from_bytes(&wrapped)?;
        Ok(PublicKey(wrapped))
    }
    /// Converts this public-key into the raw bytes of this public-key.
    pub fn into_bytes(self) -> RawPublicKey {
        self.0
    }

    /// Converts this crypto-provider-independent public key to a crypto-provider's internal rep.
    #[allow(clippy::expect_used)]
    fn as_internal<E: Ed25519Provider>(&self) -> E::PublicKey {
        <E::PublicKey as PublicKeyImpl>::from_bytes(&self.0)
            .expect("Public key bytes validated upon construction.")
    }

    /// Succeeds if the signature on the given message is verified
    /// by this public key.
    pub fn verify_strict<E: Ed25519Provider>(
        &self,
        message: &[u8],
        signature: Signature,
    ) -> Result<(), SignatureError> {
        let public_key = self.as_internal::<E>();
        let signature = signature.as_internal::<E>();
        public_key.verify_strict(message, &signature)
    }
}

/// A crypto-provider-independent representation of an Ed25519 signature.
/// The underlying representation here can be any arbitrary bytes - verifying
/// code handles determining whether/not the signature actually corresponds
/// to a real Ed25519 signature and matches the given public key.
#[derive(Clone)]
pub struct Signature(RawSignature);

impl From<RawSignature> for Signature {
    fn from(wrapped: RawSignature) -> Self {
        Signature(wrapped)
    }
}

impl Signature {
    /// Constructs a signature from raw bytes.
    pub fn new(wrapped: RawSignature) -> Self {
        Signature(wrapped)
    }
    /// Transforms this signature back into raw bytes.
    pub fn to_bytes(self) -> RawSignature {
        self.0
    }
    /// Converts this crypto-provider-independent signature to a crypto-provider's internal rep.
    fn as_internal<E: Ed25519Provider>(&self) -> E::Signature {
        <E::Signature as SignatureImpl>::from_bytes(&self.0)
    }
}

// Implementor-facing crypto-provider-internal traits.

/// Collection of types used to provide an implementation of ed25519, the Edwards-curve Digital
/// Signature Algorithm scheme using sha-512 (sha2) and Curve25519
pub trait Ed25519Provider {
    /// The internal representation of a keypair which includes both public and secret halves of an asymmetric key.
    type KeyPair: KeyPairImpl<PublicKey = Self::PublicKey, Signature = Self::Signature>;
    /// The internal representation of an ed25519 public key, used when verifying a message
    type PublicKey: PublicKeyImpl<Signature = Self::Signature>;
    /// The internal representation of an ed25519 signature which is the result of signing a message
    type Signature: SignatureImpl;
}

/// The keypair which includes both public and secret halves of an asymmetric key.
pub trait KeyPairImpl: Sized {
    /// The ed25519 public key, used when verifying a message
    type PublicKey: PublicKeyImpl;

    /// The ed25519 signature returned when signing a message
    type Signature: SignatureImpl;

    /// Returns the private key bytes of the `KeyPair`.
    /// This operation is only possible while holding a [`RawPrivateKeyPermit`].
    fn raw_private_key(&self, _permit: &RawPrivateKeyPermit) -> RawPrivateKey;

    /// Builds a key-pair from a `RawPrivateKey` array of bytes.
    /// This operation is only possible while holding a [`RawPrivateKeyPermit`].
    fn from_raw_private_key(bytes: &RawPrivateKey, _permit: &RawPrivateKeyPermit) -> Self
    where
        Self: Sized;

    /// Returns the private key of the `KeyPair` in an opaque form.
    fn private_key(&self) -> PrivateKey {
        // We're okay to reach in and grab the bytes of the private key,
        // since the way that we're exposing it would require a valid
        // [`RawPrivateKeyPermit`] to extract them again.
        let wrapped = self.raw_private_key(&RawPrivateKeyPermit::new());
        PrivateKey(wrapped)
    }

    /// Builds a key-pair from a [`PrivateKey`], given in an opaque form.
    fn from_private_key(private_key: &PrivateKey) -> Self
    where
        Self: Sized,
    {
        // We're okay to reach in and construct an instance from
        // the bytes of the private key, since the way that they
        // were originally expressed would still require a valid
        // [`RawPrivateKeyPermit`] to access them.
        let raw_private_key = &private_key.0;
        Self::from_raw_private_key(raw_private_key, &RawPrivateKeyPermit::new())
    }

    /// Sign the given message and return a digital signature
    fn sign(&self, msg: &[u8]) -> Self::Signature;

    /// Generate an ed25519 keypair from a CSPRNG
    /// generate is not available in `no-std`
    #[cfg(feature = "std")]
    fn generate() -> Self;

    /// getter function for the Public Key of the key pair
    fn public_key(&self) -> Self::PublicKey;
}

/// An ed25519 signature
pub trait SignatureImpl: Sized {
    /// Create a new signature from a fixed size byte array. This represents a container for the
    /// byte serialization of an Ed25519 signature, and does not necessarily represent well-formed
    /// field or curve elements.
    ///
    /// Signature verification libraries are expected to reject invalid field
    /// elements at the time a signature is verified (not constructed).
    fn from_bytes(bytes: &RawSignature) -> Self;

    /// Returns a slice of the signature bytes
    fn to_bytes(&self) -> RawSignature;

    /// Returns a crypto-provider-independent `Signature` from this implementation-specific struct.
    fn to_external(&self) -> Signature {
        let wrapped = self.to_bytes();
        Signature(wrapped)
    }
}

/// An ed25519 public key
pub trait PublicKeyImpl {
    /// the signature type being used by verify
    type Signature: SignatureImpl;

    /// Builds this public key from an array of bytes in
    /// the format yielded by `to_bytes`.
    fn from_bytes(bytes: &RawPublicKey) -> Result<Self, InvalidPublicKeyBytes>
    where
        Self: Sized;

    /// Yields the bytes of the public key
    fn to_bytes(&self) -> RawPublicKey;

    /// Returns a crypto-provider-independent `PublicKey` from this implementation-specific struct.
    fn to_external(&self) -> PublicKey {
        let wrapped = self.to_bytes();
        PublicKey(wrapped)
    }

    /// Succeeds if the signature was a valid signature created by this Keypair on the prehashed_message.
    fn verify_strict(
        &self,
        message: &[u8],
        signature: &Self::Signature,
    ) -> Result<(), SignatureError>;
}
