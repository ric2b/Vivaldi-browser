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

use crypto_provider::elliptic_curve::{EcdhProvider, EphemeralSecret, PublicKey};
use crypto_provider::x25519::X25519;

/// The BoringSSL implementation of X25519 ECDH.
pub struct X25519Ecdh;

impl EcdhProvider<X25519> for X25519Ecdh {
    type PublicKey = X25519PublicKey;
    type EphemeralSecret = X25519EphemeralSecret;
    type SharedSecret = [u8; 32];
}

/// A X25519 ephemeral secret used for Diffie-Hellman.
pub struct X25519EphemeralSecret {
    secret: bssl_crypto::x25519::PrivateKey,
}

impl EphemeralSecret<X25519> for X25519EphemeralSecret {
    type Impl = X25519Ecdh;
    type Error = bssl_crypto::x25519::DiffieHellmanError;
    type Rng = ();
    type EncodedPublicKey = [u8; 32];

    fn generate_random(_rng: &mut Self::Rng) -> Self {
        Self { secret: bssl_crypto::x25519::PrivateKey::generate() }
    }

    fn public_key_bytes(&self) -> Self::EncodedPublicKey {
        let pubkey: bssl_crypto::x25519::PublicKey = (&self.secret).into();
        pubkey.to_bytes()
    }

    fn diffie_hellman(
        self,
        other_pub: &<X25519Ecdh as EcdhProvider<X25519>>::PublicKey,
    ) -> Result<<X25519Ecdh as EcdhProvider<X25519>>::SharedSecret, Self::Error> {
        self.secret.diffie_hellman(&other_pub.0).map(|secret| secret.to_bytes())
    }
}

#[cfg(test)]
impl crypto_provider_test::elliptic_curve::EphemeralSecretForTesting<X25519>
    for X25519EphemeralSecret
{
    fn from_private_components(
        private_bytes: &[u8; 32],
        _public_key: &X25519PublicKey,
    ) -> Result<Self, Self::Error> {
        Ok(Self { secret: bssl_crypto::x25519::PrivateKey::from_private_bytes(private_bytes) })
    }
}

/// A X25519 public key.
#[derive(Debug, PartialEq, Eq)]
pub struct X25519PublicKey(bssl_crypto::x25519::PublicKey);

impl PublicKey<X25519> for X25519PublicKey {
    type Error = Error;
    type EncodedPublicKey = [u8; 32];

    fn from_bytes(bytes: &[u8]) -> Result<Self, Self::Error> {
        let byte_arr: [u8; 32] = bytes.try_into().map_err(|_| Error::WrongSize)?;
        Ok(Self(bssl_crypto::x25519::PublicKey::from(&byte_arr)))
    }

    fn to_bytes(&self) -> Self::EncodedPublicKey {
        self.0.to_bytes()
    }
}

/// Error type for the BoringSSL implementation of x25519.
#[derive(Debug)]
pub enum Error {
    /// Unexpected size for the given input.
    WrongSize,
}

#[cfg(test)]
mod tests {
    use super::X25519Ecdh;
    use core::marker::PhantomData;
    use crypto_provider_test::x25519::*;

    #[apply(x25519_test_cases)]
    fn x25519_tests(testcase: CryptoProviderTestCase<X25519Ecdh>) {
        testcase(PhantomData::<X25519Ecdh>)
    }
}
