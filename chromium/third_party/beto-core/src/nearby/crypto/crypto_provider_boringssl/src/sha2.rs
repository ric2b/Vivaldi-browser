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

/// BoringSSL implementation for SHA256
pub struct Sha256;

impl crypto_provider::sha2::Sha256 for Sha256 {
    fn sha256(input: &[u8]) -> [u8; 32] {
        let mut digest = bssl_crypto::digest::Sha256::new();
        digest.update(input);
        digest.digest()
    }
}

/// BoringSSL implementation for SHA512
pub struct Sha512;

impl crypto_provider::sha2::Sha512 for Sha512 {
    fn sha512(input: &[u8]) -> [u8; 64] {
        let mut digest = bssl_crypto::digest::Sha512::new();
        digest.update(input);
        digest.digest()
    }
}
