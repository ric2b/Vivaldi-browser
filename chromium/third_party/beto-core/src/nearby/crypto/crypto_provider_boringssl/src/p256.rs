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

use alloc::vec;
use bssl_crypto::ecdh::{PrivateKey, PublicKey};
use core::fmt::{Debug, Formatter};
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
pub struct P256PublicKey(PublicKey<bssl_crypto::ec::P256>);

impl PartialEq for P256PublicKey {
    fn eq(&self, other: &Self) -> bool {
        self.0.to_x962_uncompressed().as_ref() == other.0.to_x962_uncompressed().as_ref()
    }
}

impl Debug for P256PublicKey {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "X9.62 bytes: {:?}", self.0.to_x962_uncompressed().as_ref())
    }
}

impl crypto_provider::p256::P256PublicKey for P256PublicKey {
    type Error = Error;

    fn from_sec1_bytes(bytes: &[u8]) -> Result<Self, Self::Error> {
        PublicKey::from_x962_uncompressed(bytes).map(P256PublicKey).ok_or(Error::ConversionFailed)
    }

    fn to_sec1_bytes(&self, point_compression: PointCompression) -> ArrayVec<[u8; 65]> {
        match point_compression {
            PointCompression::Compressed => unimplemented!(),
            PointCompression::Uncompressed => {
                let mut bytes = ArrayVec::<[u8; 65]>::new();
                bytes.extend_from_slice(self.0.to_x962_uncompressed().as_ref());
                bytes
            }
        }
    }

    #[allow(clippy::expect_used, clippy::indexing_slicing)]
    fn to_affine_coordinates(&self) -> Result<([u8; 32], [u8; 32]), Self::Error> {
        let raw_key = self.0.to_x962_uncompressed();
        let x = raw_key.as_ref()[1..33].try_into().map_err(|_| Error::ConversionFailed)?;
        let y = raw_key.as_ref()[33..65].try_into().map_err(|_| Error::ConversionFailed)?;
        Ok((x, y))
    }
    fn from_affine_coordinates(x: &[u8; 32], y: &[u8; 32]) -> Result<Self, Self::Error> {
        let mut point = vec![0x04];
        point.extend_from_slice(x);
        point.extend_from_slice(y);
        PublicKey::<bssl_crypto::ec::P256>::from_x962_uncompressed(&point)
            .map(Self)
            .ok_or(Error::ConversionFailed)
    }
}

/// Ephemeral secret for use in a P256 Diffie-Hellman exchange.
pub struct P256EphemeralSecret {
    secret: PrivateKey<bssl_crypto::ec::P256>,
}

impl EphemeralSecret<P256> for P256EphemeralSecret {
    type Impl = P256Ecdh;
    type Error = Error;
    type Rng = ();
    type EncodedPublicKey =
        <P256PublicKey as crypto_provider::elliptic_curve::PublicKey<P256>>::EncodedPublicKey;

    fn generate_random(_rng: &mut Self::Rng) -> Self {
        Self { secret: PrivateKey::generate() }
    }

    fn public_key_bytes(&self) -> Self::EncodedPublicKey {
        P256PublicKey(self.secret.to_public_key()).to_bytes()
    }

    fn diffie_hellman(
        self,
        other_pub: &P256PublicKey,
    ) -> Result<<Self::Impl as EcdhProvider<P256>>::SharedSecret, Self::Error> {
        let shared_secret = self.secret.compute_shared_key(&other_pub.0);
        shared_secret.try_into().map_err(|_| Error::DiffieHellmanFailed)
    }
}

/// Error type for ECDH operations.
#[derive(Debug)]
pub enum Error {
    /// Failed when trying to convert between representations.
    ConversionFailed,
    /// The Diffie-Hellman key exchange failed.
    DiffieHellmanFailed,
}

#[cfg(test)]
impl crypto_provider_test::elliptic_curve::EphemeralSecretForTesting<P256> for P256EphemeralSecret {
    #[allow(clippy::unwrap_used)]
    fn from_private_components(
        private_bytes: &[u8; 32],
        _public_key: &P256PublicKey,
    ) -> Result<Self, Self::Error> {
        Ok(Self { secret: PrivateKey::from_big_endian(private_bytes).unwrap() })
    }
}

#[cfg(test)]
mod tests {
    use super::P256Ecdh;
    use core::marker::PhantomData;
    use crypto_provider_test::p256::*;

    #[apply(p256_test_cases)]
    fn p256_tests(testcase: CryptoProviderTestCase<P256Ecdh>, name: &str) {
        if name.contains("compressed") {
            return; // EC point compression not supported by bssl-crypto yet
        }
        testcase(PhantomData::<P256Ecdh>)
    }
}
