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

#![allow(missing_docs, clippy::expect_used, clippy::unwrap_used)]

use criterion::{black_box, criterion_group, criterion_main, Criterion, Throughput};
use rand::{Rng, SeedableRng};

use crypto_provider::CryptoProvider;
use crypto_provider_default::CryptoProviderImpl;
use ukey2_connections::{
    D2DConnectionContextV1, D2DHandshakeContext, InitiatorD2DHandshakeContext,
    ServerD2DHandshakeContext,
};
use ukey2_rs::{HandshakeImplementation, NextProtocol};

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

fn cbc_criterion_benchmark(c: &mut Criterion) {
    let kib = 1024;
    let mut group = c.benchmark_group("throughput");
    let mut plaintext = Vec::new();
    let (mut initiator_ctx, mut server_ctx) = run_handshake_with_rng::<CryptoProviderImpl, _>(
        rand::rngs::StdRng::from_entropy(),
        vec![NextProtocol::Aes256CbcHmacSha256],
    );
    for len in [10 * kib, 1024 * kib] {
        let _ = group.throughput(Throughput::Bytes(len as u64));
        plaintext.resize(len, 0);
        rand::thread_rng().fill(&mut plaintext[..]);
        let _ = group.bench_function(
            format!("AES-CBC-256_HMAC-SHA256 UKEY2 encrypt/decrypt {}KiB", len / kib),
            |b| {
                b.iter(|| {
                    let msg = initiator_ctx
                        .encode_message_to_peer::<CryptoProviderImpl, &[u8]>(&plaintext, None);
                    black_box(
                        server_ctx
                            .decode_message_from_peer::<CryptoProviderImpl, &[u8]>(&msg, None),
                    )
                })
            },
        );
    }
}

fn gcm_criterion_benchmark(c: &mut Criterion) {
    let kib = 1024;
    let mut group = c.benchmark_group("throughput");
    let mut plaintext = Vec::new();
    let (mut initiator_ctx, mut server_ctx) = run_handshake_with_rng::<CryptoProviderImpl, _>(
        rand::rngs::StdRng::from_entropy(),
        vec![NextProtocol::Aes256GcmSiv],
    );
    for len in [10 * kib, 1024 * kib] {
        let _ = group.throughput(Throughput::Bytes(len as u64));
        plaintext.resize(len, 0);
        rand::thread_rng().fill(&mut plaintext[..]);
        let _ = group.bench_function(
            format!("AES-GCM-SIV UKEY2 encrypt/decrypt {}KiB", len / kib),
            |b| {
                b.iter(|| {
                    let msg = initiator_ctx
                        .encode_message_to_peer::<CryptoProviderImpl, &[u8]>(&plaintext, None);
                    black_box(
                        server_ctx
                            .decode_message_from_peer::<CryptoProviderImpl, &[u8]>(&msg, None),
                    )
                })
            },
        );
    }
}

criterion_group!(benches, cbc_criterion_benchmark, gcm_criterion_benchmark);
criterion_main!(benches);
