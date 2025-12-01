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

// This file contains functions for handling CBOR Object Signing and Encryption
// (COSE; RFC 9052), including CBOR Web Tokens (CWTs; RFC 8392).

#ifndef FCP_CONFIDENTIALCOMPUTE_COSE_H_
#define FCP_CONFIDENTIALCOMPUTE_COSE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"

namespace fcp::confidential_compute {

// COSE Key Operation values; see RFC 9052 Table 5.
inline constexpr int64_t kCoseKeyOpEncrypt = 3;
inline constexpr int64_t kCoseKeyOpDecrypt = 4;

// A Cose_Key struct for an Octet Key Pair (OKP) public or private key.
struct OkpKey {
  std::string key_id;
  std::optional<int64_t> algorithm;
  std::vector<int64_t> key_ops;
  std::optional<int64_t> curve;
  std::string x;
  std::string d;

  // CBOR-decodes an OkpKey.
  static absl::StatusOr<OkpKey> Decode(absl::string_view encoded);

  // CBOR-encodes an OkpKey.
  absl::StatusOr<std::string> Encode() const;
};

// A Cose_Key struct for a symmetric key.
struct SymmetricKey {
  std::optional<int64_t> algorithm;
  std::vector<int64_t> key_ops;
  std::string k;

  // CBOR-decodes a SymmetricKey.
  static absl::StatusOr<SymmetricKey> Decode(absl::string_view encoded);

  // CBOR-encodes a SymmetricKey.
  absl::StatusOr<std::string> Encode() const;
};

// A Cose_Sign1 struct for a CBOR Web Token (CWT) containing a OKP key.
struct OkpCwt {
  std::optional<int64_t> algorithm;
  std::optional<absl::Time> issued_at;
  std::optional<absl::Time> expiration_time;
  std::optional<OkpKey> public_key;
  std::string config_properties;  // serialized google.protobuf.Struct
  std::string access_policy_sha256;
  std::string signature;

  // Returns the canonical Sig_structure object containing the protected
  // portions of the CWT. This is the portion of the CWT that should be signed.
  // Note that this SHOULD NOT be used when verifying the signature of a CWT as
  // it only includes the parameters and claims supported by OkpCwt; use
  // GetSigStructureForVerifying instead.
  absl::StatusOr<std::string> BuildSigStructureForSigning(
      absl::string_view aad) const;

  // Returns the canonical Sig_structure object containing the protected
  // portions of a CWT. This is the portion of the CWT that is signed. This
  // function does not perform validation of the CWT beyond what is needed to
  // generate the Sig_structure.
  static absl::StatusOr<std::string> GetSigStructureForVerifying(
      absl::string_view encoded, absl::string_view aad);

  // CBOR-decodes a Cwt. Both COSE_Sign and COSE_Sign1 structures are supported;
  // if a COSE_Sign structure is provided, the first signature is used.
  static absl::StatusOr<OkpCwt> Decode(absl::string_view encoded);

  // CBOR-encodes a Cwt.
  absl::StatusOr<std::string> Encode() const;
};

// A ReleaseToken structure used by the KeyManagementService API. A ReleaseToken
// contains a signed and encrypted symmetric key, along with the logical
// pipeline state change that the KMS must perform to release the key.
struct ReleaseToken {
  std::optional<int64_t> signing_algorithm;
  std::optional<int64_t> encryption_algorithm;
  std::optional<std::string> encryption_key_id;
  // In addition to being present or absent in the ReleaseToken, `src_state` can
  // be set to a string value or to null (indicating that no state exists for
  // the pipeline). Since null and "" are distinct encoded values, they're
  // exposed as distinct values here as well by using a nested std::optional.
  std::optional<std::optional<std::string>> src_state;
  std::optional<std::string> dst_state;
  std::string encrypted_payload;
  std::optional<std::string> encapped_key;
  std::string signature;

  // Returns the canonical Enc_structure used as AAD for the encrypted payload.
  // This includes the `encryption_algorithm`, `encryption_key_id`,`src_state`,
  // and `dst_state` fields. Note that this SHOULD NOT be used when decrypting
  // a ReleaseToken payload as it only includes the known/supported parameters;
  // use `GetEncStructureForDecrypting` instead.
  absl::StatusOr<std::string> BuildEncStructureForEncrypting(
      absl::string_view aad) const;

  // Returns the canonical Enc_structure used  as ADD for the encrypted payload.
  // This function does not perform validation of the ReleaseToken beyond what
  // is needed to generate the Enc_structure.
  //
  // Note: Since decoding ReleaseTokens is normally handled by the KMS, this
  // function is primarily for use in tests.
  static absl::StatusOr<std::string> GetEncStructureForDecrypting(
      absl::string_view encoded, absl::string_view aad);

  // Returns the canonical Sig_structure object containing the protected
  // portions of the ReleaseToken. This is the portion of the ReleaseToken that
  // should be signed. Note that this SHOULD NOT be used when verifying the
  // signature of a ReleaseToken as it only includes the known/supported
  // parameters; use `GetSigStructureForVerifying` instead.
  absl::StatusOr<std::string> BuildSigStructureForSigning(
      absl::string_view aad) const;

  // Returns the canonical Sig_structure object containing the protected
  // portions of a ReleaseToken. This is the portion of the ReleaseToken that is
  // signed. This function does not perform validation of the ReleaseToken
  // beyond what is needed to generate the Sig_structure.
  //
  // Note: Since decoding ReleaseTokens is normally handled by the KMS, this
  // function is primarily for use in tests.
  static absl::StatusOr<std::string> GetSigStructureForVerifying(
      absl::string_view encoded, absl::string_view aad);

  // CBOR-decodes a ReleaseToken.
  static absl::StatusOr<ReleaseToken> Decode(absl::string_view encoded);

  // CBOR-encodes a ReleaseToken.
  absl::StatusOr<std::string> Encode() const;
};

}  // namespace fcp::confidential_compute

#endif  // FCP_CONFIDENTIALCOMPUTE_COSE_H_
