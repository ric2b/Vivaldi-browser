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

#![allow(clippy::indexing_slicing)]

use crypto_provider::CryptoProvider;
use crypto_provider_default::CryptoProviderImpl;
use rand::SeedableRng;
use rand::{rngs::StdRng, CryptoRng, RngCore};
use ukey2_rs::{HandshakeImplementation, NextProtocol};

use crate::{
    crypto_utils::{decrypt_cbc, encrypt_cbc},
    java_utils, Aes256Key, D2DConnectionContextV1, D2DHandshakeContext, DeserializeError,
    InitiatorD2DHandshakeContext, ServerD2DHandshakeContext,
};

type AesCbcPkcs7Padded = <CryptoProviderImpl as CryptoProvider>::AesCbcPkcs7Padded;

#[test]
fn crypto_test_encrypt_decrypt() {
    let message = b"Hello World!";
    let key = b"42424242424242424242424242424242";
    let (ciphertext, iv) =
        encrypt_cbc::<_, AesCbcPkcs7Padded>(key, message, &mut rand::rngs::StdRng::from_entropy());
    let decrypt_result = decrypt_cbc::<AesCbcPkcs7Padded>(key, ciphertext.as_slice(), &iv);
    let ptext = decrypt_result.expect("Decrypt should be successful");
    assert_eq!(ptext, message.to_vec());
}

#[test]
fn crypto_test_encrypt_seeded() {
    let message = b"Hello World!";
    let key = b"42424242424242424242424242424242";
    let mut rng = MockRng;
    let (ciphertext, iv) = encrypt_cbc::<_, AesCbcPkcs7Padded>(key, message, &mut rng);
    // Expected values extracted from the results of the current implementation.
    // This test makes sure that we don't accidentally change the encryption logic that
    // causes incompatibility between versions.
    assert_eq!(&iv, &[1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]);
    assert_eq!(
        ciphertext,
        &[20, 59, 195, 101, 11, 208, 245, 128, 247, 196, 81, 80, 158, 77, 174, 61]
    );
}

#[test]
fn crypto_test_decrypt_seeded() {
    let iv = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1];
    let ciphertext = [20, 59, 195, 101, 11, 208, 245, 128, 247, 196, 81, 80, 158, 77, 174, 61];
    let key = b"42424242424242424242424242424242";
    let plaintext = decrypt_cbc::<AesCbcPkcs7Padded>(key, &ciphertext, &iv).unwrap();
    assert_eq!(plaintext, b"Hello World!");
}

#[test]
fn decrypt_test_wrong_key() {
    let message = b"Hello World!";
    let good_key = b"42424242424242424242424242424242";
    let (ciphertext, iv) = encrypt_cbc::<_, AesCbcPkcs7Padded>(
        good_key,
        message,
        &mut rand::rngs::StdRng::from_entropy(),
    );
    let bad_key = b"43434343434343434343434343434343";
    let decrypt_result = decrypt_cbc::<AesCbcPkcs7Padded>(bad_key, ciphertext.as_slice(), &iv);
    match decrypt_result {
        // The padding is valid, but the decrypted value should be bad since the keys don't match
        Ok(decrypted_bad) => assert_ne!(decrypted_bad, message),
        // The padding is bad, so it returns an error and is unable to decrypt
        Err(crypto_provider::aes::cbc::DecryptionError::BadPadding) => (),
    }
    let decrypt_result = decrypt_cbc::<AesCbcPkcs7Padded>(good_key, ciphertext.as_slice(), &iv);
    let ptext = decrypt_result.unwrap();
    assert_eq!(ptext, message.to_vec());
}

fn run_cbc_handshake() -> (D2DConnectionContextV1, D2DConnectionContextV1) {
    run_handshake_with_rng::<CryptoProviderImpl, _>(
        rand::rngs::StdRng::from_entropy(),
        vec![NextProtocol::Aes256CbcHmacSha256],
    )
}

fn run_gcm_handshake() -> (D2DConnectionContextV1, D2DConnectionContextV1) {
    run_handshake_with_rng::<CryptoProviderImpl, _>(
        rand::rngs::StdRng::from_entropy(),
        vec![NextProtocol::Aes256GcmSiv],
    )
}

fn run_handshake_with_rng<C, R>(
    mut rng: R,
    next_protocols: Vec<NextProtocol>,
) -> (D2DConnectionContextV1<R>, D2DConnectionContextV1<R>)
where
    C: CryptoProvider,
    R: rand::RngCore + rand::CryptoRng + rand::SeedableRng + Send,
{
    let mut initiator_ctx = InitiatorD2DHandshakeContext::<C, R>::new_impl(
        HandshakeImplementation::Spec,
        R::from_rng(&mut rng).unwrap(),
        next_protocols.clone(),
    );
    let mut server_ctx = ServerD2DHandshakeContext::<C, R>::new_impl(
        HandshakeImplementation::Spec,
        R::from_rng(&mut rng).unwrap(),
        &next_protocols,
    );
    server_ctx
        .handle_handshake_message(
            initiator_ctx.get_next_handshake_message().expect("No message").as_slice(),
        )
        .expect("Failed to handle message");
    initiator_ctx
        .handle_handshake_message(
            server_ctx.get_next_handshake_message().expect("No message").as_slice(),
        )
        .expect("Failed to handle message");
    server_ctx
        .handle_handshake_message(
            initiator_ctx.get_next_handshake_message().expect("No message").as_slice(),
        )
        .expect("Failed to handle message");
    assert!(initiator_ctx.is_handshake_complete());
    assert!(server_ctx.is_handshake_complete());
    (initiator_ctx.to_connection_context().unwrap(), server_ctx.to_connection_context().unwrap())
}

// TODO: Find a way to inject RNG / generated ephemeral secrets in openSSL and test them here
#[cfg(feature = "test_rustcrypto")]
#[test]
fn send_receive_message_seeded() {
    use crypto_provider_rustcrypto::RustCryptoImpl;
    let rng = MockRng;
    let message = b"Hello World!";
    let (mut init_conn_ctx, mut server_conn_ctx) =
        run_handshake_with_rng::<RustCryptoImpl<MockRng>, _>(
            rng,
            vec![NextProtocol::Aes256CbcHmacSha256],
        );
    let encoded =
        init_conn_ctx.encode_message_to_peer::<RustCryptoImpl<MockRng>, &[u8]>(message, None);
    // Expected values extracted from the results of the current implementation.
    // This test makes sure that we don't accidentally change the encryption logic that
    // causes incompatibility between versions.
    assert_eq!(
        encoded,
        &[
            10, 64, 10, 28, 8, 1, 16, 2, 42, 16, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            50, 4, 8, 13, 16, 1, 18, 32, 23, 58, 102, 24, 40, 222, 59, 212, 182, 181, 96, 44, 57,
            21, 93, 253, 71, 54, 67, 37, 226, 43, 104, 224, 178, 221, 219, 189, 106, 135, 175, 150,
            18, 32, 134, 9, 237, 41, 112, 183, 129, 198, 240, 13, 139, 66, 21, 56, 28, 100, 142,
            240, 155, 52, 242, 11, 211, 132, 175, 230, 15, 241, 208, 185, 15, 105
        ]
    );
    let decoded = server_conn_ctx
        .decode_message_from_peer::<CryptoProviderImpl, &[u8]>(&encoded, None)
        .unwrap();
    assert_eq!(message, &decoded[..]);
}

#[test]
fn send_receive_message() {
    let message = b"Hello World!";
    let (mut init_conn_ctx, mut server_conn_ctx) = run_cbc_handshake();
    let encoded = init_conn_ctx.encode_message_to_peer::<CryptoProviderImpl, &[u8]>(message, None);
    let decoded = server_conn_ctx
        .decode_message_from_peer::<CryptoProviderImpl, &[u8]>(encoded.as_slice(), None);
    assert_eq!(message.to_vec(), decoded.expect("Decode should be successful"));
}

#[test]
fn send_receive_message_gcm() {
    let message = b"Hello World!";
    let (mut init_conn_ctx, mut server_conn_ctx) = run_gcm_handshake();
    let encoded = init_conn_ctx.encode_message_to_peer::<CryptoProviderImpl, &[u8]>(message, None);
    let decoded = server_conn_ctx
        .decode_message_from_peer::<CryptoProviderImpl, &[u8]>(encoded.as_slice(), None);
    assert_eq!(message.to_vec(), decoded.expect("Decode should be successful"));
}

#[test]
fn send_receive_message_associated_data() {
    let message = b"Hello World!";
    let (mut init_conn_ctx, mut server_conn_ctx) = run_cbc_handshake();
    let encoded = init_conn_ctx
        .encode_message_to_peer::<CryptoProviderImpl, _>(message, Some(b"associated data"));
    let decoded = server_conn_ctx.decode_message_from_peer::<CryptoProviderImpl, _>(
        encoded.as_slice(),
        Some(b"associated data"),
    );
    assert_eq!(message.to_vec(), decoded.expect("Decode should be successful"));
    // Make sure decode fails with missing associated data.
    let decoded = server_conn_ctx
        .decode_message_from_peer::<CryptoProviderImpl, &[u8]>(encoded.as_slice(), None);
    assert!(decoded.is_err());
    // Make sure decode fails with different associated data.
    let decoded = server_conn_ctx.decode_message_from_peer::<CryptoProviderImpl, _>(
        encoded.as_slice(),
        Some(b"assoc1ated data"),
    );
    assert!(decoded.is_err());
}

#[test]
fn test_save_restore_session() {
    let (init_conn_ctx, server_conn_ctx) = run_cbc_handshake();
    let init_session = init_conn_ctx.save_session();
    let server_session = server_conn_ctx.save_session();
    let mut init_restored_ctx =
        D2DConnectionContextV1::from_saved_session::<CryptoProviderImpl>(init_session.as_slice())
            .expect("failed to restore client session");
    let mut server_restored_ctx =
        D2DConnectionContextV1::from_saved_session::<CryptoProviderImpl>(server_session.as_slice())
            .expect("failed to restore server session");
    let message = b"Hello World!";
    let encoded =
        init_restored_ctx.encode_message_to_peer::<CryptoProviderImpl, &[u8]>(message, None);
    let decoded = server_restored_ctx
        .decode_message_from_peer::<CryptoProviderImpl, &[u8]>(encoded.as_slice(), None);
    assert_eq!(message.to_vec(), decoded.expect("Decode should be successful"));
}

#[test]
fn test_save_restore_bad_session() {
    let (init_conn_ctx, server_conn_ctx) = run_cbc_handshake();
    let init_session = init_conn_ctx.save_session();
    let server_session = server_conn_ctx.save_session();
    let _ =
        D2DConnectionContextV1::from_saved_session::<CryptoProviderImpl>(init_session.as_slice())
            .expect("failed to restore client session");
    let server_restored_ctx =
        D2DConnectionContextV1::from_saved_session::<CryptoProviderImpl>(&server_session[0..60]);
    assert_eq!(server_restored_ctx.unwrap_err(), DeserializeError::BadDataLength);
}

#[test]
fn test_save_restore_bad_protocol_version() {
    let (init_conn_ctx, server_conn_ctx) = run_cbc_handshake();
    let init_session = init_conn_ctx.save_session();
    let mut server_session = server_conn_ctx.save_session();
    let _ =
        D2DConnectionContextV1::from_saved_session::<CryptoProviderImpl>(init_session.as_slice())
            .expect("failed to restore client session");
    server_session[0] = 0; // Change the protocol version to an invalid one (0)
    let server_restored_ctx =
        D2DConnectionContextV1::from_saved_session::<CryptoProviderImpl>(&server_session);
    assert_eq!(server_restored_ctx.unwrap_err(), DeserializeError::BadProtocolVersion);
}

#[test]
fn test_unique_session() {
    let (mut init_conn_ctx, mut server_conn_ctx) = run_cbc_handshake();
    let init_session = init_conn_ctx.get_session_unique::<CryptoProviderImpl>();
    let server_session = server_conn_ctx.get_session_unique::<CryptoProviderImpl>();
    let message = b"Hello World!";
    let encoded = init_conn_ctx.encode_message_to_peer::<CryptoProviderImpl, &[u8]>(message, None);
    let decoded = server_conn_ctx
        .decode_message_from_peer::<CryptoProviderImpl, &[u8]>(encoded.as_slice(), None);
    assert_eq!(message.to_vec(), decoded.expect("Decode should be successful"));
    let init_session_after = init_conn_ctx.get_session_unique::<CryptoProviderImpl>();
    let server_session_after = server_conn_ctx.get_session_unique::<CryptoProviderImpl>();
    let bad_server_ctx = D2DConnectionContextV1::new::<CryptoProviderImpl>(
        server_conn_ctx.get_sequence_number_for_decoding(),
        server_conn_ctx.get_sequence_number_for_encoding(),
        Aes256Key::default(),
        Aes256Key::default(),
        StdRng::from_entropy(),
        NextProtocol::Aes256CbcHmacSha256,
    );
    assert_eq!(init_session, init_session_after);
    assert_eq!(server_session, server_session_after);
    assert_eq!(init_session, server_session);
    assert_ne!(server_session, bad_server_ctx.get_session_unique::<CryptoProviderImpl>());
}

#[test]
fn test_java_hashcode() {
    assert_eq!(java_utils::hash_code("4".as_bytes()), 83i32);
    assert_eq!(java_utils::hash_code(&[0x65, 0x47]), 4163i32);
    assert_eq!(java_utils::hash_code(&[0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78]), 1590192324i32);
    assert_eq!(
        java_utils::hash_code(&[0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0xFF]),
        2051321787
    );
}

/// A mock RNG that always returns 1 at each byte. The output from this RNG is
/// not changed from call to call to avoid ordering changes in code from
/// changing the expected output. The downside is that code that keeps looping
/// and generating a new random number until it fits certain criteria will hang
/// indefinitely.
#[derive(Eq, PartialEq, Clone, Debug)]
struct MockRng;

impl SeedableRng for MockRng {
    type Seed = [u8; 0];

    fn from_seed(_seed: Self::Seed) -> Self {
        Self
    }
}

impl CryptoRng for MockRng {}

impl RngCore for MockRng {
    fn next_u32(&mut self) -> u32 {
        let mut buf = [0_u8; 4];
        self.fill_bytes(&mut buf);
        u32::from_le_bytes(buf)
    }

    fn next_u64(&mut self) -> u64 {
        let mut buf = [0_u8; 8];
        self.fill_bytes(&mut buf);
        u64::from_le_bytes(buf)
    }

    fn fill_bytes(&mut self, dest: &mut [u8]) {
        dest.fill(1);
    }

    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), rand::Error> {
        self.fill_bytes(dest);
        Ok(())
    }
}
