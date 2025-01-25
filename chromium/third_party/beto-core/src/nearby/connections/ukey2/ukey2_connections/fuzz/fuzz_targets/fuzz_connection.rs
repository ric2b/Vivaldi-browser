#![cfg_attr(fuzzing, no_main)]
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

use arbitrary::Arbitrary;
use crypto_provider_rustcrypto::RustCrypto;
use derive_fuzztest::fuzztest;
use rand_chacha::rand_core::SeedableRng;
use ukey2_connections::HandshakeImplementation;
use ukey2_connections::{
    D2DHandshakeContext, InitiatorD2DHandshakeContext, ServerD2DHandshakeContext,
};
use ukey2_rs::NextProtocol;

#[derive(Clone, Debug, Arbitrary)]
enum Type {
    SentByInitiator,
    SentByServer,
    ReceivedByInitiator,
    ReceivedByServer,
}

#[derive(Clone, Debug, Arbitrary)]
struct Message {
    sender: Type,
    payload: Vec<u8>,
    associated_data: Option<Vec<u8>>,
}

#[fuzztest]
fn test(client_rng_seed: [u8; 32], server_rng_seed: [u8; 32], messages: Vec<Message>) {
    let mut initiator_ctx = InitiatorD2DHandshakeContext::<RustCrypto, _>::new_impl(
        HandshakeImplementation::Spec,
        rand_chacha::ChaChaRng::from_seed(client_rng_seed),
        vec![NextProtocol::Aes256CbcHmacSha256],
    );
    let mut server_ctx = ServerD2DHandshakeContext::<RustCrypto, _>::new_impl(
        HandshakeImplementation::Spec,
        rand_chacha::ChaChaRng::from_seed(server_rng_seed),
        &[NextProtocol::Aes256CbcHmacSha256],
    );
    let client_init = initiator_ctx
        .get_next_handshake_message()
        .expect("Initial get_next_handshake_message should succeed");
    server_ctx
        .handle_handshake_message(&client_init)
        .expect("Server handling client init message should succeed");
    let server_init = server_ctx
        .get_next_handshake_message()
        .expect("server.get_next_handshake_message should succeed for server init");
    initiator_ctx
        .handle_handshake_message(&server_init)
        .expect("Client handling server init message should succeed");
    let client_finish = initiator_ctx
        .get_next_handshake_message()
        .expect("client.get_next_handshake_message should succeed when for client finish");
    server_ctx
        .handle_handshake_message(&client_finish)
        .expect("Server handling client finish message should succeed");
    assert!(server_ctx.is_handshake_complete());
    assert!(initiator_ctx.is_handshake_complete());

    let mut server_connection = server_ctx
        .to_connection_context()
        .expect("Server handshake context should be converted to connection context successfully");
    let mut initiator_connection = initiator_ctx.to_connection_context().expect(
        "Initator handshake context should be converted to connection context successfully",
    );

    for Message { sender, payload, associated_data } in messages {
        match sender {
            Type::SentByInitiator => {
                let ciphertext = initiator_connection
                    .encode_message_to_peer::<RustCrypto, _>(&payload, associated_data.as_ref());
                let decoded = server_connection
                    .decode_message_from_peer::<RustCrypto, _>(
                        &ciphertext,
                        associated_data.as_ref(),
                    )
                    .unwrap();
                assert_eq!(decoded, payload);
            }
            Type::SentByServer => {
                let ciphertext = server_connection
                    .encode_message_to_peer::<RustCrypto, _>(&payload, associated_data.as_ref());
                let decoded = initiator_connection
                    .decode_message_from_peer::<RustCrypto, _>(
                        &ciphertext,
                        associated_data.as_ref(),
                    )
                    .unwrap();
                assert_eq!(decoded, payload);
            }
            Type::ReceivedByInitiator => {
                // Both Ok and Err results are possible here since the input is Arbitrary payload
                let _unused_result = initiator_connection
                    .decode_message_from_peer::<RustCrypto, _>(&payload, associated_data.as_ref());
            }
            Type::ReceivedByServer => {
                // Both Ok and Err results are possible here since the input is Arbitrary payload
                let _unused_result = server_connection
                    .decode_message_from_peer::<RustCrypto, _>(&payload, associated_data.as_ref());
            }
        }
    }
}
