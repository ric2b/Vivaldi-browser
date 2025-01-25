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

//! An adapter that converts between generated protobuf types and native Rust types.

use crypto_provider::elliptic_curve::EcdhProvider;
use crypto_provider::p256::{P256EcdhProvider, P256PublicKey, P256};
use crypto_provider::CryptoProvider;
use std::collections::HashSet;
use std::fmt::{Display, Formatter};
use ukey2_proto::ukey2_all_proto::{securemessage, ukey};

/// For generated proto types for UKEY2 messages
trait WithMessageType: ukey2_proto::protobuf::Message {
    fn msg_type() -> ukey::ukey2message::Type;
}

pub(crate) trait ToWrappedMessage {
    /// Wrap `self` in a `Ukey2Message`. Creates a new `Ukey2Message` with `message_type` set to
    /// [`msg_type`][WithMessageType::msg_type] and `message_data` set to the serialized bytes for
    /// the `self` proto.
    fn to_wrapped_msg(self) -> ukey::Ukey2Message;
}

impl<M: WithMessageType> ToWrappedMessage for M {
    fn to_wrapped_msg(self) -> ukey::Ukey2Message {
        ukey::Ukey2Message {
            message_type: Some(Self::msg_type().into()),
            message_data: self.write_to_bytes().ok(),
            ..Default::default()
        }
    }
}

impl WithMessageType for ukey::Ukey2Alert {
    fn msg_type() -> ukey::ukey2message::Type {
        ukey::ukey2message::Type::ALERT
    }
}

impl WithMessageType for ukey::Ukey2ServerInit {
    fn msg_type() -> ukey::ukey2message::Type {
        ukey::ukey2message::Type::SERVER_INIT
    }
}

impl WithMessageType for ukey::Ukey2ClientFinished {
    fn msg_type() -> ukey::ukey2message::Type {
        ukey::ukey2message::Type::CLIENT_FINISH
    }
}

impl WithMessageType for ukey::Ukey2ClientInit {
    fn msg_type() -> ukey::ukey2message::Type {
        ukey::ukey2message::Type::CLIENT_INIT
    }
}

/// Convert a generated proto type into our custom adapter type.
pub(crate) trait IntoAdapter<A> {
    /// Convert `self` into the adapter type.
    fn into_adapter(self) -> Result<A, ukey::ukey2alert::AlertType>;
}

/// Enum representing the different supported next_protocol strings, ordered by desirability.
#[derive(Copy, Clone, Debug, Eq, Hash, Ord, PartialOrd, PartialEq)]
#[repr(i32)]
pub enum NextProtocol {
    /// AES-256-GCM-SIV, for use with newer clients.
    Aes256GcmSiv,
    /// AES_256_CBC-HMAC_SHA256, already in use and supported by all clients.
    Aes256CbcHmacSha256,
}

impl TryFrom<&String> for NextProtocol {
    type Error = ukey::ukey2alert::AlertType;

    fn try_from(value: &String) -> Result<Self, Self::Error> {
        match value.as_str() {
            "AES_256_GCM_SIV" => Ok(NextProtocol::Aes256GcmSiv),
            "AES_256_CBC-HMAC_SHA256" => Ok(NextProtocol::Aes256CbcHmacSha256),
            _ => Err(ukey::ukey2alert::AlertType::BAD_NEXT_PROTOCOL),
        }
    }
}

impl Display for NextProtocol {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}",
            match self {
                NextProtocol::Aes256CbcHmacSha256 => "AES_256_CBC-HMAC_SHA256",
                NextProtocol::Aes256GcmSiv => "AES_256_GCM_SIV",
            },
        )
    }
}

#[derive(Debug, PartialEq, Eq)]
pub(crate) enum MessageType {
    ClientInit,
    ServerInit,
    ClientFinish,
}

pub(crate) struct ClientInit {
    version: i32,
    commitments: Vec<CipherCommitment>,
    next_protocols: HashSet<NextProtocol>,
}

impl ClientInit {
    pub fn version(&self) -> i32 {
        self.version
    }

    pub fn commitments(&self) -> &[CipherCommitment] {
        &self.commitments
    }

    pub fn next_protocols(&self) -> &HashSet<NextProtocol> {
        &self.next_protocols
    }
}

#[allow(dead_code)]
pub(crate) struct ServerInit {
    version: i32,
    random: [u8; 32],
    handshake_cipher: HandshakeCipher,
    pub(crate) public_key: Vec<u8>,
    selected_next_protocol: NextProtocol,
}

impl ServerInit {
    pub fn version(&self) -> i32 {
        self.version
    }

    pub fn handshake_cipher(&self) -> HandshakeCipher {
        self.handshake_cipher
    }

    pub fn selected_next_protocol(&self) -> NextProtocol {
        self.selected_next_protocol
    }
}

pub(crate) struct ClientFinished {
    pub(crate) public_key: Vec<u8>,
}

/// The handshake cipher used for UKEY2 handshake. Corresponds to the proto message
/// `ukey::Ukey2HandshakeCipher`.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum HandshakeCipher {
    /// NIST P-256 used for ECDH, SHA512 used for commitment
    P256Sha512,
    /// Curve 25519 used for ECDH, SHA512 used for commitment
    Curve25519Sha512,
}

impl HandshakeCipher {
    pub(crate) fn as_proto(&self) -> ukey::Ukey2HandshakeCipher {
        match self {
            HandshakeCipher::P256Sha512 => ukey::Ukey2HandshakeCipher::P256_SHA512,
            HandshakeCipher::Curve25519Sha512 => ukey::Ukey2HandshakeCipher::CURVE25519_SHA512,
        }
    }
}

#[derive(Clone)]
pub(crate) struct CipherCommitment {
    cipher: HandshakeCipher,
    commitment: Vec<u8>,
}

impl CipherCommitment {
    pub fn cipher(&self) -> HandshakeCipher {
        self.cipher
    }

    pub fn commitment(&self) -> &[u8] {
        &self.commitment
    }
}

pub(crate) enum GenericPublicKey<C: CryptoProvider> {
    Ec256(<C::P256 as EcdhProvider<P256>>::PublicKey),
    // Other public key types are not supported
}

impl IntoAdapter<MessageType> for ukey::ukey2message::Type {
    fn into_adapter(self) -> Result<MessageType, ukey::ukey2alert::AlertType> {
        match self {
            ukey::ukey2message::Type::CLIENT_INIT => Ok(MessageType::ClientInit),
            ukey::ukey2message::Type::SERVER_INIT => Ok(MessageType::ServerInit),
            ukey::ukey2message::Type::CLIENT_FINISH => Ok(MessageType::ClientFinish),
            _ => Err(ukey::ukey2alert::AlertType::BAD_MESSAGE_TYPE),
        }
    }
}

impl IntoAdapter<HandshakeCipher> for i32 {
    fn into_adapter(self) -> Result<HandshakeCipher, ukey::ukey2alert::AlertType> {
        const P256_CODE: i32 = ukey::Ukey2HandshakeCipher::P256_SHA512 as i32;
        const CURVE25519_CODE: i32 = ukey::Ukey2HandshakeCipher::CURVE25519_SHA512 as i32;
        match self {
            P256_CODE => Ok(HandshakeCipher::P256Sha512),
            CURVE25519_CODE => Ok(HandshakeCipher::Curve25519Sha512),
            _ => Err(ukey::ukey2alert::AlertType::BAD_HANDSHAKE_CIPHER),
        }
    }
}

impl IntoAdapter<CipherCommitment> for ukey::ukey2client_init::CipherCommitment {
    fn into_adapter(self) -> Result<CipherCommitment, ukey::ukey2alert::AlertType> {
        let handshake_cipher: HandshakeCipher = self
            .handshake_cipher
            .ok_or(ukey::ukey2alert::AlertType::BAD_HANDSHAKE_CIPHER)
            .and_then(|code| code.value().into_adapter())?;
        // no bad commitment so this is best-effort
        let commitment = self
            .commitment
            .filter(|c| !c.is_empty())
            .ok_or(ukey::ukey2alert::AlertType::BAD_HANDSHAKE_CIPHER)?;
        Ok(CipherCommitment { commitment, cipher: handshake_cipher })
    }
}

impl IntoAdapter<ClientInit> for ukey::Ukey2ClientInit {
    fn into_adapter(self) -> Result<ClientInit, ukey::ukey2alert::AlertType> {
        if self.random().len() != 32 {
            return Err(ukey::ukey2alert::AlertType::BAD_RANDOM);
        }
        let version: i32 = self.version.ok_or(ukey::ukey2alert::AlertType::BAD_VERSION)?;
        let next_protocol = self
            .next_protocol
            .filter(|n| !n.is_empty())
            .ok_or(ukey::ukey2alert::AlertType::BAD_NEXT_PROTOCOL)?;
        let mut next_protocols: HashSet<NextProtocol> =
            HashSet::from([(&next_protocol).try_into()?]);
        let other_next_protocols: Vec<NextProtocol> =
            self.next_protocols.iter().filter_map(|p| p.try_into().ok()).collect();
        next_protocols.extend(&other_next_protocols);
        Ok(ClientInit {
            next_protocols,
            version,
            commitments: self
                .cipher_commitments
                .into_iter()
                .map(|c| c.into_adapter())
                .collect::<Result<Vec<_>, _>>()?,
        })
    }
}

impl IntoAdapter<ServerInit> for ukey::Ukey2ServerInit {
    fn into_adapter(self) -> Result<ServerInit, ukey::ukey2alert::AlertType> {
        let version: i32 = self.version.ok_or(ukey::ukey2alert::AlertType::BAD_VERSION)?;
        let random: [u8; 32] = self
            .random
            .and_then(|r| r.try_into().ok())
            .ok_or(ukey::ukey2alert::AlertType::BAD_RANDOM)?;
        let handshake_cipher = self
            .handshake_cipher
            .ok_or(ukey::ukey2alert::AlertType::BAD_HANDSHAKE_CIPHER)
            .and_then(|code| code.value().into_adapter())?;
        // We will be handling bad pubkeys in the layers above
        let public_key: Vec<u8> =
            self.public_key.ok_or(ukey::ukey2alert::AlertType::BAD_PUBLIC_KEY)?;
        let selected_next_protocol = self
            .selected_next_protocol
            .and_then(|p| (&p).try_into().ok())
            .unwrap_or(NextProtocol::Aes256CbcHmacSha256);
        Ok(ServerInit { handshake_cipher, version, public_key, random, selected_next_protocol })
    }
}

impl IntoAdapter<ClientFinished> for ukey::Ukey2ClientFinished {
    fn into_adapter(self) -> Result<ClientFinished, ukey::ukey2alert::AlertType> {
        let public_key: Vec<u8> =
            self.public_key.ok_or(ukey::ukey2alert::AlertType::BAD_PUBLIC_KEY)?;
        Ok(ClientFinished { public_key })
    }
}

impl<C: CryptoProvider> IntoAdapter<GenericPublicKey<C>> for securemessage::GenericPublicKey {
    fn into_adapter(self) -> Result<GenericPublicKey<C>, ukey::ukey2alert::AlertType> {
        let key_type = self
            .type_
            .and_then(|t| t.enum_value().ok())
            .ok_or(ukey::ukey2alert::AlertType::BAD_PUBLIC_KEY)?;
        match key_type {
            securemessage::PublicKeyType::EC_P256 => {
                let (key_x, key_y) = self
                    .ec_p256_public_key
                    .into_option()
                    .and_then(|pk| pk.x.zip(pk.y))
                    .ok_or(ukey::ukey2alert::AlertType::BAD_PUBLIC_KEY)?;
                let key_x_bytes: [u8; 32] = positive_twos_complement_to_32_byte_unsigned(&key_x)
                    .ok_or(ukey::ukey2alert::AlertType::BAD_PUBLIC_KEY)?;
                let key_y_bytes: [u8; 32] = positive_twos_complement_to_32_byte_unsigned(&key_y)
                    .ok_or(ukey::ukey2alert::AlertType::BAD_PUBLIC_KEY)?;
                <C::P256 as P256EcdhProvider>::PublicKey::from_affine_coordinates(
                    &key_x_bytes,
                    &key_y_bytes,
                )
                .map(GenericPublicKey::Ec256)
                .map_err(|_| ukey::ukey2alert::AlertType::BAD_PUBLIC_KEY)
            }
            securemessage::PublicKeyType::RSA2048 => {
                // We don't support RSA keys
                Err(ukey::ukey2alert::AlertType::BAD_PUBLIC_KEY)
            }
            securemessage::PublicKeyType::DH2048_MODP => {
                // We don't support DH2048 keys, only ECDH.
                Err(ukey::ukey2alert::AlertType::BAD_PUBLIC_KEY)
            }
        }
    }
}

/// Turns a big endian two's complement integer representation into big endian unsigned
/// representation. If the input byte array is not positive or cannot be fit into 32 byte unsigned
/// int range, then `None` is returned.
fn positive_twos_complement_to_32_byte_unsigned(twos_complement: &[u8]) -> Option<[u8; 32]> {
    #[allow(clippy::indexing_slicing)]
    if !twos_complement.is_empty() && (twos_complement[0] & 0x80) == 0 {
        let mut twos_complement_iter = twos_complement.iter().rev();
        let mut result = [0_u8; 32];
        for (dst, src) in result.iter_mut().rev().zip(&mut twos_complement_iter) {
            *dst = *src;
        }
        if twos_complement_iter.any(|x| *x != 0) {
            // If any remaining elements are non-zero, the input cannot be fit into the 32 byte
            // unsigned range
            return None;
        }
        // No conversion needed since positive two's complement is the same as unsigned
        Some(result)
    } else {
        None
    }
}

#[cfg(test)]
mod test {
    #[test]
    fn test_positive_twos_complement_to_32_byte_unsigned() {
        assert_eq!(
            super::positive_twos_complement_to_32_byte_unsigned(&[]), // Empty input
            None
        );
        assert_eq!(
            super::positive_twos_complement_to_32_byte_unsigned(&[0xff, 0x05, 0x05]), // Negative
            None
        );
        assert_eq!(
            super::positive_twos_complement_to_32_byte_unsigned(&[0xff; 32]), // Negative
            None
        );
        assert_eq!(
            super::positive_twos_complement_to_32_byte_unsigned(&[0x05; 34]), // Too long
            None
        );
        assert_eq!(
            super::positive_twos_complement_to_32_byte_unsigned(&[0x05, 0xff]),
            Some([
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x05, 0xff
            ])
        );
        assert_eq!(
            super::positive_twos_complement_to_32_byte_unsigned(&[0x05, 0x05, 0x05]),
            Some([
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x05, 0x05, 0x05
            ])
        );
        assert_eq!(
            super::positive_twos_complement_to_32_byte_unsigned(&[0x05; 32]),
            Some([0x05; 32])
        );
        let mut input_33_bytes = [0xff_u8; 33];
        assert_eq!(
            super::positive_twos_complement_to_32_byte_unsigned(&input_33_bytes),
            None // Negative input
        );
        input_33_bytes[0] = 0;
        assert_eq!(
            super::positive_twos_complement_to_32_byte_unsigned(&input_33_bytes),
            Some([0xff; 32])
        );
    }
}
