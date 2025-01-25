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

//! Wrappers around NP's usage of ed25519 signatures.
//!
//! All of NP's usages of ed25519 signatures are performed
//! with "context" bytes prepended to the payload to be signed
//! or verified. These "context" bytes allow for usage of the
//! same base key-pair for different purposes in the protocol.
#![no_std]

use array_view::ArrayView;
use crypto_provider::ed25519::{Ed25519Provider, PrivateKey, PublicKey, Signature, SignatureError};
use sink::{Sink, SinkWriter};
use tinyvec::ArrayVec;

/// Maximum length of the combined (context len byte) + (context bytes) + (signing payload)
/// byte-array which an ed25519 signature will be computed over. This is deliberately
/// chosen to be large enough to incorporate an entire v1 adv as the signing payload.
pub const MAX_SIGNATURE_BUFFER_LEN: usize = 512;

/// Sign the given message with the given context and
/// return a digital signature. The message is represented
/// using a [`SinkWriter`] to allow the caller to construct
/// the payload to sign without requiring a fully-assembled
/// payload available as a slice.
///
/// If the message writer writes too much data (greater than 256 bytes),
/// this will return `None` instead of a valid signature,
/// and so uses in `np_adv` will use `.expect` on the returned value
/// to indicate that this length constraint has been considered.
pub fn sign_with_context<E: Ed25519Provider, W: SinkWriter<DataType = u8>>(
    private_key: &PrivateKey,
    context: &SignatureContext,
    msg_writer: W,
) -> Option<Signature> {
    let mut buffer = context.create_signature_buffer();
    buffer.try_extend_from_writer(msg_writer).map(|_| private_key.sign::<E>(buffer.as_ref()))
}

/// Errors yielded when attempting to verify an ed25519 signature.
#[derive(Debug, PartialEq, Eq)]
pub enum SignatureVerificationError {
    /// The payload that we attempted to verify the signature of was too big
    PayloadTooBig,
    /// The signature we were checking was invalid for the given payload
    SignatureInvalid,
}

impl From<SignatureError> for SignatureVerificationError {
    fn from(_: SignatureError) -> Self {
        Self::SignatureInvalid
    }
}

/// Succeeds if the signature was a valid signature created via the corresponding
/// keypair to this public key using the given [`SignatureContext`] on the given
/// message payload. The message payload is represented
/// using a [`SinkWriter`] to allow the caller to construct
/// the payload to sign without requiring a fully-assembled
/// payload available as a slice.
///
/// If the message writer writes too much data (greater than 256 bytes),
/// this will return `None` instead of a valid signature,
/// and so uses in `np_adv` will use `.expect` on the returned value
/// to indicate that this length constraint has been considered.
pub fn verify_signature_with_context<E: Ed25519Provider, W: SinkWriter<DataType = u8>>(
    public_key: &PublicKey,
    context: &SignatureContext,
    msg_writer: W,
    signature: Signature,
) -> Result<(), SignatureVerificationError> {
    let mut buffer = context.create_signature_buffer();
    let maybe_write_success = buffer.try_extend_from_writer(msg_writer);
    match maybe_write_success {
        Some(_) => {
            public_key.verify_strict::<E>(buffer.as_ref(), signature)?;
            Ok(())
        }
        None => Err(SignatureVerificationError::PayloadTooBig),
    }
}

/// Minimum length (in bytes) for a [`SignatureContext`] (which cannot be empty).
pub const MIN_SIGNATURE_CONTEXT_LEN: usize = 1;

/// Maximum length (in bytes) for a [`SignatureContext`] (which uses an 8-bit length field).
pub const MAX_SIGNATURE_CONTEXT_LEN: usize = 255;

/// (Non-empty) context bytes to use in the construction of NP's
/// Ed25519 signatures. The context bytes should uniquely
/// identify the component of the protocol performing the
/// signature/verification (e.g: advertisement signing,
/// connection signing), and should be between 1 and
/// 255 bytes in length.
pub struct SignatureContext {
    data: ArrayView<u8, MAX_SIGNATURE_CONTEXT_LEN>,
}

impl SignatureContext {
    /// Creates a signature buffer with size bounded by MAX_SIGNATURE_BUFFER_LEN
    /// which is pre-populated with the contents yielded by
    /// [`SignatureContext#write_length_prefixed`].
    fn create_signature_buffer(&self) -> impl Sink<u8> + AsRef<[u8]> {
        let mut buffer = ArrayVec::<[u8; MAX_SIGNATURE_BUFFER_LEN]>::new();
        #[allow(clippy::expect_used)]
        self.write_length_prefixed(&mut buffer).expect("Context should always fit into sig buffer");
        buffer
    }

    /// Writes the contents of this signature context, prefixed
    /// by the length of the context payload to the given byte-sink.
    /// If writing to the sink failed at some point during this operation,
    /// `None` will be returned, and the data written to the sink should
    /// be considered to be invalid.
    fn write_length_prefixed<S: Sink<u8>>(&self, sink: &mut S) -> Option<()> {
        let length_byte = self.data.len() as u8;
        sink.try_push(length_byte)?;
        sink.try_extend_from_slice(self.data.as_slice())
    }

    /// Attempts to construct a signature context from the utf-8 bytes
    /// of the given string. Returns `None` if the passed string
    /// is invalid to use for signature context bytes.
    pub const fn from_string_bytes(data_str: &str) -> Result<Self, SignatureContextInvalidLength> {
        let data_bytes = data_str.as_bytes();
        Self::from_bytes(data_bytes)
    }

    #[allow(clippy::indexing_slicing)]
    const fn from_bytes(bytes: &[u8]) -> Result<Self, SignatureContextInvalidLength> {
        let num_bytes = bytes.len();
        if num_bytes < MIN_SIGNATURE_CONTEXT_LEN || num_bytes > MAX_SIGNATURE_CONTEXT_LEN {
            Err(SignatureContextInvalidLength)
        } else {
            let mut array = [0u8; MAX_SIGNATURE_CONTEXT_LEN];
            let mut i = 0;
            while i < num_bytes {
                array[i] = bytes[i];
                i += 1;
            }
            let data = ArrayView::const_from_array(array, bytes.len());
            Ok(Self { data })
        }
    }
}

/// Error raised when attempting to construct a
/// [`SignatureContext`] out of data with an
/// invalid length in bytes.
#[derive(Debug)]
pub struct SignatureContextInvalidLength;

impl TryFrom<&[u8]> for SignatureContext {
    type Error = SignatureContextInvalidLength;

    fn try_from(value: &[u8]) -> Result<Self, Self::Error> {
        Self::from_bytes(value)
    }
}
