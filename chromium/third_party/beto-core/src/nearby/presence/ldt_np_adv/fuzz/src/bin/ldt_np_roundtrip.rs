#![cfg_attr(fuzzing, no_main)]
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

use crypto_provider_rustcrypto::RustCrypto;
use derive_fuzztest::fuzztest;
use ldt::*;
use ldt_np_adv::*;
use xts_aes::XtsAes128;

#[fuzztest]
fn test(data: LdtNpRoundtripFuzzInput) {
    let salt = data.salt.into();
    let padder = salt_padder::<RustCrypto>(salt);

    let hkdf = np_hkdf::NpKeySeedHkdf::<RustCrypto>::new(&data.key_seed);
    let ldt_enc = LdtEncryptCipher::<16, XtsAes128<RustCrypto>, Swap>::new(&hkdf.v0_ldt_key());
    let identity_token_hmac: [u8; 32] =
        hkdf.v0_identity_token_hmac_key().calculate_hmac::<RustCrypto>(&data.plaintext[..14]);

    let cipher = build_np_adv_decrypter_from_key_seed::<RustCrypto>(&hkdf, identity_token_hmac);

    let len = 16 + (data.len as usize % 16);
    let mut ciphertext = data.plaintext;

    ldt_enc.encrypt(&mut ciphertext[..len], &padder).unwrap();
    let (token, plaintext) = cipher.decrypt_and_verify(&ciphertext[..len], &padder).unwrap();

    assert_eq!(&data.plaintext[..14], token.as_slice());
    assert_eq!(&data.plaintext[14..len], plaintext.as_slice());
}

#[derive(Clone, Debug, arbitrary::Arbitrary)]
struct LdtNpRoundtripFuzzInput {
    key_seed: [u8; 32],
    salt: [u8; 2],
    // max length = 2 * AES block size - 1
    plaintext: [u8; 31],
    // will % 16 to get a [0-15] value for how much of the plaintext to use past the first block
    len: u8,
}
