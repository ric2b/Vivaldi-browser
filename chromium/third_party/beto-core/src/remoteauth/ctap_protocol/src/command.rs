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

//! # CTAP Protocol
//!
//! This crate represents CTAP messages and turns them into a binary representation to be sent to a
//! remote device.

use anyhow::anyhow;

/// The Rust representation of CTAP values (or identifiers).
#[derive(Debug, PartialEq)]
pub enum Value {
    AuthenticatorMakeCredential = 0x01,
    AuthenticatorGetAssertion = 0x02,
    AuthenticatorGetInfo = 0x04,
    AuthenticatorClientPIN = 0x06,
    AuthenticatorReset = 0x07,
    AuthenticatorGetNextAssertion = 0x08,
}

/// The Rust representation of CTAP parameters.
#[derive(Debug, PartialEq)]
pub enum Parameters {
    AuthenticatorMakeCredential,
    AuthenticatorGetAssertion,
    AuthenticatorClientPIN,
}

impl From<Parameters> for Vec<u8> {
    fn from(message: Parameters) -> Vec<u8> {
        // TODO: serialize parameters correctly
        match message {
            Parameters::AuthenticatorMakeCredential => vec![],
            Parameters::AuthenticatorGetAssertion => vec![],
            Parameters::AuthenticatorClientPIN => vec![],
        }
    }
}

pub struct Command {
    pub value: Value,
    pub parameters: Option<Parameters>,
}

impl From<Command> for Vec<u8> {
    /// Converts the given CTAP command into its binary representation.
    fn from(message: Command) -> Vec<u8> {
        let mut result = vec![message.value as u8];
        if let Some(p) = message.parameters {
            result.append(&mut p.into());
        }
        result
    }
}

impl TryFrom<Vec<u8>> for Command {
    type Error = anyhow::Error;
    /// Convert a binary message to its Rust representation.
    fn try_from(bytes: Vec<u8>) -> anyhow::Result<Self> {
        if bytes.is_empty() {
            Err(anyhow!("Binary message was empty."))
        } else {
            match bytes[0] {
                _x if _x == Value::AuthenticatorMakeCredential as u8 => Ok(Self {
                    value: Value::AuthenticatorMakeCredential,
                    parameters: Some(Parameters::AuthenticatorMakeCredential),
                }),
                _x if _x == Value::AuthenticatorGetAssertion as u8 => Ok(Self {
                    value: Value::AuthenticatorGetAssertion,
                    parameters: Some(Parameters::AuthenticatorGetAssertion),
                }),
                _x if _x == Value::AuthenticatorGetInfo as u8 => Ok(Self {
                    value: Value::AuthenticatorGetInfo,
                    parameters: None,
                }),
                _x if _x == Value::AuthenticatorClientPIN as u8 => Ok(Self {
                    value: Value::AuthenticatorClientPIN,
                    parameters: Some(Parameters::AuthenticatorClientPIN),
                }),
                _x if _x == Value::AuthenticatorReset as u8 => Ok(Self {
                    value: Value::AuthenticatorReset,
                    parameters: None,
                }),
                _x if _x == Value::AuthenticatorGetNextAssertion as u8 => Ok(Self {
                    value: Value::AuthenticatorGetNextAssertion,
                    parameters: None,
                }),
                _ => Err(anyhow!("Unknown message type.")),
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use anyhow::bail;

    #[test]
    fn translate_message_to_bytes() {
        let mut message = Command {
            value: Value::AuthenticatorReset,
            parameters: None,
        };
        let mut bytes: Vec<u8> = message.into();

        assert_eq!(bytes, vec![Value::AuthenticatorReset as u8]);

        message = Command {
            value: Value::AuthenticatorMakeCredential,
            parameters: None,
        };
        bytes = message.into();

        assert_eq!(bytes, vec![Value::AuthenticatorMakeCredential as u8]);

        message = Command {
            value: Value::AuthenticatorGetAssertion,
            parameters: None,
        };
        bytes = message.into();

        assert_eq!(bytes, vec![Value::AuthenticatorGetAssertion as u8]);

        message = Command {
            value: Value::AuthenticatorGetInfo,
            parameters: None,
        };
        bytes = message.into();

        assert_eq!(bytes, vec![Value::AuthenticatorGetInfo as u8]);

        message = Command {
            value: Value::AuthenticatorClientPIN,
            parameters: None,
        };
        bytes = message.into();

        assert_eq!(bytes, vec![Value::AuthenticatorClientPIN as u8]);

        message = Command {
            value: Value::AuthenticatorGetNextAssertion,
            parameters: None,
        };
        bytes = message.into();

        assert_eq!(bytes, vec![Value::AuthenticatorGetNextAssertion as u8]);
    }

    #[test]
    fn translate_bytes_to_message() -> anyhow::Result<()> {
        let mut bytes = vec![Value::AuthenticatorReset as u8];
        let mut message: Command = bytes.try_into()?;
        assert_eq!(message.value, Value::AuthenticatorReset);
        assert_eq!(message.parameters, None);

        bytes = vec![Value::AuthenticatorMakeCredential as u8];
        message = bytes.try_into()?;
        assert_eq!(message.value, Value::AuthenticatorMakeCredential);
        assert_eq!(
            message.parameters,
            Some(Parameters::AuthenticatorMakeCredential)
        );

        bytes = vec![Value::AuthenticatorGetAssertion as u8];
        message = bytes.try_into()?;
        assert_eq!(message.value, Value::AuthenticatorGetAssertion);
        assert_eq!(
            message.parameters,
            Some(Parameters::AuthenticatorGetAssertion)
        );

        bytes = vec![Value::AuthenticatorGetInfo as u8];
        message = bytes.try_into()?;
        assert_eq!(message.value, Value::AuthenticatorGetInfo);
        assert_eq!(message.parameters, None);

        bytes = vec![Value::AuthenticatorClientPIN as u8];
        message = bytes.try_into()?;
        assert_eq!(message.value, Value::AuthenticatorClientPIN);
        assert_eq!(message.parameters, Some(Parameters::AuthenticatorClientPIN));

        bytes = vec![Value::AuthenticatorGetNextAssertion as u8];
        message = bytes.try_into()?;
        assert_eq!(message.value, Value::AuthenticatorGetNextAssertion);
        assert_eq!(message.parameters, None);
        Ok(())
    }

    #[test]
    fn translate_empty_bytes() -> anyhow::Result<()> {
        let bytes = vec![];
        let res: anyhow::Result<Command> = bytes.try_into();
        match res {
            Err(e) => {
                assert_eq!(e.to_string(), "Binary message was empty.");
                Ok(())
            }
            _ => bail!("Should not parse empty bytes"),
        }
    }

    #[test]
    fn translate_unknown_message() -> anyhow::Result<()> {
        let bytes = vec![0x10];
        let res: anyhow::Result<Command> = bytes.try_into();
        match res {
            Err(e) => {
                assert_eq!(e.to_string(), "Unknown message type.");
                Ok(())
            }
            _ => bail!("Should not parse unknown message."),
        }
    }
}
