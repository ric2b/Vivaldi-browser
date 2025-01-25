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

use ed25519_dalek::Signer;

use crypto_provider::ed25519::{
    InvalidPublicKeyBytes, RawPrivateKey, RawPrivateKeyPermit, RawPublicKey, RawSignature,
    SignatureError, SignatureImpl, PRIVATE_KEY_LENGTH, PUBLIC_KEY_LENGTH, SIGNATURE_LENGTH,
};

pub struct Ed25519;

impl crypto_provider::ed25519::Ed25519Provider for Ed25519 {
    type KeyPair = KeyPair;
    type PublicKey = PublicKey;
    type Signature = Signature;
}

pub struct KeyPair(ed25519_dalek::SigningKey);

impl crypto_provider::ed25519::KeyPairImpl for KeyPair {
    type PublicKey = PublicKey;
    type Signature = Signature;

    fn raw_private_key(&self, _permit: &RawPrivateKeyPermit) -> [u8; PRIVATE_KEY_LENGTH] {
        self.0.to_bytes()
    }

    fn from_raw_private_key(bytes: &RawPrivateKey, _permit: &RawPrivateKeyPermit) -> Self {
        Self(ed25519_dalek::SigningKey::from_bytes(bytes))
    }

    #[allow(clippy::expect_used)]
    fn sign(&self, msg: &[u8]) -> Self::Signature {
        Self::Signature::from_bytes(&self.0.sign(msg).to_bytes())
    }

    //TODO: allow providing a crypto rng and make it a no-op for openssl if the need arises to
    // improve perf of keypair generation
    #[cfg(feature = "std")]
    fn generate() -> Self {
        let mut csprng = rand::rngs::ThreadRng::default();
        Self(ed25519_dalek::SigningKey::generate(&mut csprng))
    }

    fn public_key(&self) -> Self::PublicKey {
        PublicKey(self.0.verifying_key())
    }
}

pub struct Signature(ed25519_dalek::Signature);

impl crypto_provider::ed25519::SignatureImpl for Signature {
    fn from_bytes(bytes: &RawSignature) -> Self {
        Self(ed25519_dalek::Signature::from_bytes(bytes))
    }

    fn to_bytes(&self) -> [u8; SIGNATURE_LENGTH] {
        self.0.to_bytes()
    }
}

pub struct PublicKey(ed25519_dalek::VerifyingKey);

impl crypto_provider::ed25519::PublicKeyImpl for PublicKey {
    type Signature = Signature;

    fn from_bytes(bytes: &RawPublicKey) -> Result<Self, InvalidPublicKeyBytes>
    where
        Self: Sized,
    {
        ed25519_dalek::VerifyingKey::from_bytes(bytes)
            .map(PublicKey)
            .map_err(|_| InvalidPublicKeyBytes)
    }

    fn to_bytes(&self) -> [u8; PUBLIC_KEY_LENGTH] {
        self.0.to_bytes()
    }

    fn verify_strict(
        &self,
        message: &[u8],
        signature: &Self::Signature,
    ) -> Result<(), SignatureError> {
        self.0.verify_strict(message, &signature.0).map_err(|_| SignatureError)
    }
}

#[cfg(test)]
mod tests {
    use crypto_provider_test::ed25519::{run_rfc_test_vectors, run_wycheproof_test_vectors};

    use crate::ed25519::Ed25519;

    #[test]
    fn wycheproof_test_ed25519_rustcrypto() {
        run_wycheproof_test_vectors::<Ed25519>()
    }

    #[test]
    fn rfc_test_ed25519_rustcrypto() {
        run_rfc_test_vectors::<Ed25519>()
    }
}
