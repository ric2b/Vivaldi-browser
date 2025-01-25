// Copyright 2024 Google LLC
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

#![allow(clippy::unwrap_used)]

use super::super::*;
use alloc::format;
use crypto_provider_default::CryptoProviderImpl;

#[test]
fn section_identity_resolution_content_derives() {
    let salt: ExtendedV1Salt = [0; 16].into();
    let nonce = salt.compute_nonce::<CryptoProviderImpl>();
    let token = CiphertextExtendedIdentityToken([0; 16]);
    let section = SectionIdentityResolutionContents { identity_token: token, nonce };
    assert_eq!(section, section);
    let _ = format!("{:?}", section);
}

#[test]
fn sig_encrypted_section_debug() {
    let ss = SignatureEncryptedSection {
        contents: EncryptedSectionContents::new(
            V1AdvHeader::new(0),
            &[0],
            [0x00; 16].into(),
            [0x00; V1_IDENTITY_TOKEN_LEN].into(),
            1,
            &[0x00; 1],
        ),
    };
    let _ = format!("{:?}", ss);
}

#[test]
fn error_enum_debug_derives() {
    let mic_err = MicVerificationError::MicMismatch;
    let _ = format!("{:?}", mic_err);

    let sig_err = SignatureVerificationError::SignatureMissing;
    let _ = format!("{:?}", sig_err);

    let deser_err = DeserializationError::ArenaOutOfSpace::<MicVerificationError>;
    let _ = format!("{:?}", deser_err);

    let err =
        IdentityResolutionOrDeserializationError::IdentityMatchingError::<MicVerificationError>;
    let _ = format!("{:?}", err);
}
