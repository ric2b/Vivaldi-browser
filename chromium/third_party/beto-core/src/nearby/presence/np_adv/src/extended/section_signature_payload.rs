// Copyright 2023 Google LLC
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

//! V1 advertisement section np_ed25519 signature payload
//! after the included context bytes, and utilities for
//! performing signatures and signature verification.

use crate::header::VERSION_HEADER_V1;
use crypto_provider::{
    aes::ctr::AesCtrNonce,
    ed25519::{Ed25519Provider, PrivateKey, PublicKey, Signature},
};
use sink::{Sink, SinkWriter};

use crate::NP_SVC_UUID;

/// A struct representing the necessary contents
/// of an v1 advertisement section np_ed25519 signature payload which
/// come after the context prefix (shared among all advs).
pub(crate) struct SectionSignaturePayload<'a> {
    /// first 1-2 bytes of format
    format_bytes: &'a [u8],
    /// Salt bytes
    salt_bytes: &'a [u8],
    /// Nonce for en/decrypting the section
    nonce: &'a AesCtrNonce,
    /// plaintext identity token
    plaintext_identity_token: &'a [u8],
    /// the len of the rest of the section contents stored as an u8
    section_payload_len: u8,
    /// plaintext identity token
    plaintext_data_elements: &'a [u8],
}

const ADV_SIGNATURE_CONTEXT: np_ed25519::SignatureContext = {
    match np_ed25519::SignatureContext::from_string_bytes("Advertisement Signed Section") {
        Ok(x) => x,
        Err(_) => panic!(),
    }
};

impl<'a> SectionSignaturePayload<'a> {
    /// Construct a section signature payload with separate section len and
    /// remaining contents of the section header.
    ///
    /// The section header should be in its on-the-wire form.
    pub(crate) fn new(
        format_bytes: &'a [u8],
        salt_bytes: &'a [u8],
        nonce: &'a AesCtrNonce,
        plaintext_identity_token: &'a [u8],
        section_payload_len: u8,
        plaintext_data_elements: &'a [u8],
    ) -> Self {
        Self {
            format_bytes,
            salt_bytes,
            nonce,
            plaintext_identity_token,
            section_payload_len,
            plaintext_data_elements,
        }
    }

    /// Generates a signature for this section signing payload using
    /// the given Ed25519 key-pair.
    pub(crate) fn sign<E: Ed25519Provider>(self, private_key: &PrivateKey) -> Signature {
        np_ed25519::sign_with_context::<E, _>(private_key, &ADV_SIGNATURE_CONTEXT, self)
            .expect("section signature payloads should fit in signature buffer")
    }

    /// Verifies a signature for this section signing payload using
    /// the given Ed25519 public key.
    pub(crate) fn verify<E: Ed25519Provider>(
        self,
        signature: Signature,
        public_key: &PublicKey,
    ) -> Result<(), np_ed25519::SignatureVerificationError> {
        np_ed25519::verify_signature_with_context::<E, _>(
            public_key,
            &ADV_SIGNATURE_CONTEXT,
            self,
            signature,
        )
    }
}

impl<'a> SinkWriter for SectionSignaturePayload<'a> {
    type DataType = u8;

    fn write_payload<S: Sink<u8> + ?Sized>(self, sink: &mut S) -> Option<()> {
        sink.try_extend_from_slice(&NP_SVC_UUID)?;
        sink.try_push(VERSION_HEADER_V1)?;
        sink.try_extend_from_slice(self.format_bytes)?;
        sink.try_extend_from_slice(self.salt_bytes)?;
        sink.try_extend_from_slice(self.nonce)?;
        sink.try_extend_from_slice(self.plaintext_identity_token)?;
        sink.try_push(self.section_payload_len)?;
        sink.try_extend_from_slice(self.plaintext_data_elements)
    }
}
