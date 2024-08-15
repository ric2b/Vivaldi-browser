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

extern crate alloc;

use bssl_crypto::ecdh::{PrivateKey, PublicKey};
use crypto_provider::{
    elliptic_curve::{EcdhProvider, EphemeralSecret, PublicKey as _},
    p256::{PointCompression, P256},
    tinyvec::ArrayVec,
};

/// Implementation of NIST-P256 using bssl-crypto crate.
pub struct P256Ecdh;

impl EcdhProvider<P256> for P256Ecdh {
    type PublicKey = P256PublicKey;
    type EphemeralSecret = P256EphemeralSecret;
    type SharedSecret = [u8; 32];
}

/// A NIST-P256 public key.
#[derive(Debug, PartialEq, Eq)]
pub struct P256PublicKey(PublicKey<bssl_crypto::ecdh::P256>);
impl crypto_provider::p256::P256PublicKey for P256PublicKey {
    type Error = bssl_crypto::ecdh::Error;

    fn from_sec1_bytes(bytes: &[u8]) -> Result<Self, Self::Error> {
        PublicKey::try_from(bytes).map(Self)
    }

    fn to_sec1_bytes(&self, point_compression: PointCompression) -> ArrayVec<[u8; 65]> {
        match point_compression {
            PointCompression::Compressed => unimplemented!(),
            PointCompression::Uncompressed => {
                let mut bytes = ArrayVec::<[u8; 65]>::new();
                bytes.extend_from_slice(&self.0.to_vec());
                bytes
            }
        }
    }

    #[allow(clippy::expect_used)]
    fn to_affine_coordinates(&self) -> Result<([u8; 32], [u8; 32]), Self::Error> {
        Ok(self.0.to_affine_coordinates())
    }
    fn from_affine_coordinates(x: &[u8; 32], y: &[u8; 32]) -> Result<Self, Self::Error> {
        PublicKey::<bssl_crypto::ecdh::P256>::from_affine_coordinates(x, y).map(Self)
    }
}

/// Ephemeral secrect for use in a P256 Diffie-Hellman
pub struct P256EphemeralSecret {
    secret: PrivateKey<bssl_crypto::ecdh::P256>,
}

impl EphemeralSecret<P256> for P256EphemeralSecret {
    type Impl = P256Ecdh;
    type Error = bssl_crypto::ecdh::Error;
    type Rng = ();
    type EncodedPublicKey =
        <P256PublicKey as crypto_provider::elliptic_curve::PublicKey<P256>>::EncodedPublicKey;

    fn generate_random(_rng: &mut Self::Rng) -> Self {
        Self { secret: PrivateKey::generate() }
    }

    fn public_key_bytes(&self) -> Self::EncodedPublicKey {
        P256PublicKey((&self.secret).into()).to_bytes()
    }

    fn diffie_hellman(
        self,
        other_pub: &P256PublicKey,
    ) -> Result<<Self::Impl as EcdhProvider<P256>>::SharedSecret, Self::Error> {
        let shared_secret = self.secret.diffie_hellman(&other_pub.0)?;
        let bytes: <Self::Impl as EcdhProvider<P256>>::SharedSecret = shared_secret.to_bytes();
        Ok(bytes)
    }
}

#[cfg(test)]
impl crypto_provider_test::elliptic_curve::EphemeralSecretForTesting<P256> for P256EphemeralSecret {
    #[allow(clippy::unwrap_used)]
    fn from_private_components(
        private_bytes: &[u8; 32],
        _public_key: &P256PublicKey,
    ) -> Result<Self, Self::Error> {
        Ok(Self { secret: PrivateKey::from_private_bytes(private_bytes).unwrap() })
    }
}

#[cfg(test)]
mod tests {
    use super::P256Ecdh;
    use core::marker::PhantomData;
    use crypto_provider_test::p256::*;

    #[apply(p256_test_cases)]
    fn p256_tests(testcase: CryptoProviderTestCase<P256Ecdh>, name: &str) {
        if name == "to_bytes_compressed" {
            return; // EC point compression not supported by bssl-crypto yet
        }
        testcase(PhantomData::<P256Ecdh>)
    }
}
