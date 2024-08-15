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

//! Common externally-acessible V1 constructs for both of the
//! serialization+deserialization flows.

/// Information about the verification scheme used
/// for verifying the integrity of the contents
/// of a decrypted section.
#[derive(Clone, Copy)]
#[repr(u8)]
pub enum V1VerificationMode {
    /// Message integrity code verification.
    Mic = 0,
    /// Signature verification.
    Signature = 1,
}

impl From<np_adv::extended::deserialize::VerificationMode> for V1VerificationMode {
    fn from(verification_mode: np_adv::extended::deserialize::VerificationMode) -> Self {
        use np_adv::extended::deserialize::VerificationMode;
        match verification_mode {
            VerificationMode::Mic => Self::Mic,
            VerificationMode::Signature => Self::Signature,
        }
    }
}
