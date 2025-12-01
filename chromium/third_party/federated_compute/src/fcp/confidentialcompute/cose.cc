// Copyright 2024 Google LLC
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

#include "fcp/confidentialcompute/cose.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/casts.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#ifdef FCP_CLIENT_SUPPORT_CONFIDENTIAL_AGG
#include "libcppbor/include/cppbor/cppbor.h"
#include "libcppbor/include/cppbor/cppbor_parse.h"
#endif

namespace fcp::confidential_compute {
// TODO: b/361182982 - Clean up the ifdef once the iOS toolchain supports C++20,
// or a better solution towards C++20 compatibility is found.
#ifdef FCP_CLIENT_SUPPORT_CONFIDENTIAL_AGG
namespace {

using ::cppbor::Array;
using ::cppbor::Bstr;
using ::cppbor::Map;

// CWT Claims; see https://www.iana.org/assignments/cwt/cwt.xhtml.
enum CwtClaim {
  kExp = 4,  // Expiration time.
  kIat = 6,  // Issued at.

  // Claims in the private space (-65537 and below).
  // See ../protos/confidentialcompute/cbor_ids.md for claims originating from
  // this project and
  // https://github.com/project-oak/oak/blob/main/oak_dice/src/cert.rs for Oak
  // claims.
  kPublicKey = -65537,           // Claim containing serialized public key.
  kConfigProperties = -65538,    // Claim containing configuration properties.
  kAccessPolicySha256 = -65543,  // Claim containing access policy hash.
  kOakPublicKey = -4670552,      // Oak claim containing serialized public key.
};

// COSE Header parameters; see https://www.iana.org/assignments/cose/cose.xhtml.
enum CoseHeaderParameter {
  kHdrAlg = 1,
  kHdrKid = 4,

  // Parameters in the private space (-65537 and below).
  // See ../protos/confidentialcompute/cbor_ids.md.
  kEncapsulatedKey = -65537,
  kSrcState = -65538,
  kDstState = -65539,
};

// COSE Key parameters; see https://www.iana.org/assignments/cose/cose.xhtml.
enum CoseKeyParameter {
  // Common parameters.
  kKty = 1,
  kKid = 2,
  kAlg = 3,
  kKeyOps = 4,

  // OKP parameters.
  kOkpCrv = -1,
  kOkpX = -2,
  kOkpD = -4,

  // Symmetric parameters.
  kSymmetricK = -1,
};

// COSE Key types; see https://www.iana.org/assignments/cose/cose.xhtml.
enum CoseKeyType {
  kOkp = 1,
  kSymmetric = 4,
};

// Builds the protected header for a COSE structure, which is a map encoded as a
// bstr.
std::vector<uint8_t> BuildProtectedHeader(
    std::optional<int64_t> algorithm,
    const std::optional<std::optional<std::string>>& src_state,
    const std::optional<std::string>& dst_state) {
  Map map;
  if (algorithm) {
    map.add(CoseHeaderParameter::kHdrAlg, *algorithm);
  }
  if (src_state) {
    map.add(CoseHeaderParameter::kSrcState,
            *src_state ? absl::implicit_cast<std::unique_ptr<cppbor::Item>>(
                             std::make_unique<Bstr>(**src_state))
                       : std::make_unique<cppbor::Null>());
  }
  if (dst_state) {
    map.add(CoseHeaderParameter::kDstState, Bstr(*dst_state));
  }
  return map.encode();
}

// Builds the payload for a CWT, which is a map of CWT claims encoded as a bstr.
// See RFC 8392 section 7.1.
absl::StatusOr<std::vector<uint8_t>> BuildCwtPayload(const OkpCwt& cwt) {
  Map map;
  if (cwt.expiration_time) {
    map.add(CwtClaim::kExp, absl::ToUnixSeconds(*cwt.expiration_time));
  }
  if (cwt.issued_at) {
    map.add(CwtClaim::kIat, absl::ToUnixSeconds(*cwt.issued_at));
  }
  if (cwt.public_key) {
    FCP_ASSIGN_OR_RETURN(std::string encoded_public_key,
                         cwt.public_key->Encode());
    map.add(CwtClaim::kPublicKey, Bstr(encoded_public_key));
  }
  if (!cwt.config_properties.empty()) {
    map.add(CwtClaim::kConfigProperties, Bstr(cwt.config_properties));
  }
  if (!cwt.access_policy_sha256.empty()) {
    map.add(CwtClaim::kAccessPolicySha256, Bstr(cwt.access_policy_sha256));
  }
  return map.encode();
}

// Parses a serialized protected header from a COSE structure and sets the
// corresponding output variables (if non-null).
absl::Status ParseProtectedHeader(
    const std::vector<uint8_t>& serialized_header,
    std::optional<int64_t>* algorithm,
    std::optional<std::optional<std::string>>* src_state,
    std::optional<std::string>* dst_state) {
  auto [payload, end_pos, error] = cppbor::parse(serialized_header);
  if (!error.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to decode protected header: ", error));
  } else if (end_pos != serialized_header.data() + serialized_header.size()) {
    return absl::InvalidArgumentError(
        "failed to decode protected header: input contained extra data");
  } else if (payload->type() != cppbor::MAP) {
    return absl::InvalidArgumentError("protected header is invalid");
  }

  // Process the parameters map.
  for (const auto& [key, value] : *payload->asMap()) {
    if (key->asInt() == nullptr) continue;  // Ignore other key types.
    switch (key->asInt()->value()) {
      case CoseHeaderParameter::kHdrAlg:
        if (algorithm) {
          if (value->asInt() == nullptr) {
            return absl::InvalidArgumentError(absl::StrCat(
                "unsupported algorithm parameter type ", value->type()));
          }
          *algorithm = value->asInt()->value();
        }
        break;

      case CoseHeaderParameter::kSrcState:
        if (src_state) {
          if (value->type() == cppbor::SIMPLE &&
              value->asSimple()->simpleType() == cppbor::NULL_T) {
            *src_state = std::optional<std::string>(std::nullopt);
          } else if (value->type() == cppbor::BSTR) {
            *src_state = std::string(value->asBstr()->value().begin(),
                                     value->asBstr()->value().end());
          } else {
            return absl::InvalidArgumentError(
                absl::StrCat("unsupported src_state type ", value->type()));
          }
        }
        break;

      case CoseHeaderParameter::kDstState:
        if (dst_state) {
          if (value->type() != cppbor::BSTR) {
            return absl::InvalidArgumentError(
                absl::StrCat("unsupported dst_state type ", value->type()));
          }
          *dst_state = std::string(value->asBstr()->value().begin(),
                                   value->asBstr()->value().end());
        }
        break;

      default:
        break;
    }
  }
  return absl::OkStatus();
}

// Parses a serialized CWT payload and updates the OkpCwt.
absl::Status ParseCwtPayload(const std::vector<uint8_t>& serialized_payload,
                             OkpCwt& cwt) {
  auto [payload, end_pos, error] =
      cppbor::parse(serialized_payload.data(), serialized_payload.size());
  if (!error.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to decode CWT payload: ", error));
  } else if (end_pos != serialized_payload.data() + serialized_payload.size()) {
    return absl::InvalidArgumentError(
        "failed to decode CWT payload: input contained extra data");
  } else if (payload->type() != cppbor::MAP) {
    return absl::InvalidArgumentError("CWT payload is invalid");
  }

  // Process the claims map.
  for (const auto& [key, value] : *payload->asMap()) {
    if (key->asInt() == nullptr) continue;  // Ignore other key types.
    switch (key->asInt()->value()) {
      case CwtClaim::kExp:
        if (value->asInt() == nullptr) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported exp type ", value->type()));
        }
        cwt.expiration_time = absl::FromUnixSeconds(value->asInt()->value());
        break;

      case CwtClaim::kIat:
        if (value->asInt() == nullptr) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported iat type ", value->type()));
        }
        cwt.issued_at = absl::FromUnixSeconds(value->asInt()->value());
        break;

      case CwtClaim::kOakPublicKey:
      case CwtClaim::kPublicKey: {
        if (value->type() != cppbor::BSTR) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported public_key type ", value->type()));
        }
        FCP_ASSIGN_OR_RETURN(
            cwt.public_key,
            OkpKey::Decode(absl::string_view(
                reinterpret_cast<const char*>(value->asBstr()->value().data()),
                value->asBstr()->value().size())));
        break;
      }

      case CwtClaim::kConfigProperties:
        if (value->type() != cppbor::BSTR) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported configuration type ", value->type()));
        }
        cwt.config_properties.assign(
            reinterpret_cast<const char*>(value->asBstr()->value().data()),
            value->asBstr()->value().size());
        break;

      case CwtClaim::kAccessPolicySha256:
        if (value->type() != cppbor::BSTR) {
          return absl::InvalidArgumentError(absl::StrCat(
              "unsupported access_policy_sha256 type ", value->type()));
        }
        cwt.access_policy_sha256.assign(value->asBstr()->value().begin(),
                                        value->asBstr()->value().end());
        break;

      default:
        break;
    }
  }
  return absl::OkStatus();
}

// Builds a Sig_structure object for a COSE_Sign or COSE_Sign1 structure.
// See RFC 9052 section 4.4 for the contents of the Sig_structure.
std::string BuildSigStructure(
    std::vector<uint8_t> body_protected,
    std::optional<std::vector<uint8_t>> sign_protected, absl::string_view aad,
    std::vector<uint8_t> payload) {
  Array sig_structure;
  sig_structure.add(sign_protected ? "Signature" : "Signature1");
  sig_structure.add(std::move(body_protected));
  if (sign_protected) {
    sig_structure.add(std::move(*sign_protected));
  }
  sig_structure.add(Bstr(aad.begin(), aad.end()));
  sig_structure.add(std::move(payload));
  return sig_structure.toString();
}

// Parses a serialized COSE_Sign or COSE_Sign1 structure and returns the
// protected header, signer protected header (COSE_Sign only), payload, and
// signature.
absl::StatusOr<
    std::tuple<std::vector<uint8_t>, std::optional<std::vector<uint8_t>>,
               std::vector<uint8_t>, std::vector<uint8_t>>>
ParseCoseSign(absl::string_view encoded) {
  auto [item, end_pos, error] = cppbor::parse(
      reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size());
  if (!error.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to decode COSE_Sign: ", error));
  } else if (end_pos != reinterpret_cast<const uint8_t*>(encoded.data()) +
                            encoded.size()) {
    return absl::InvalidArgumentError(
        "failed to decode COSE_Sign: input contained extra data");
  } else if (auto array = item->asArray();
             array == nullptr || array->size() != 4 ||
             array->get(0)->type() != cppbor::BSTR ||
             array->get(1)->type() != cppbor::MAP ||
             array->get(2)->type() != cppbor::BSTR) {
    return absl::InvalidArgumentError("COSE_Sign is invalid");
  }

  // Extract the signature and signer protected header (COSE_Sign only).
  std::optional<std::vector<uint8_t>> sign_protected;
  std::optional<std::vector<uint8_t>> signature;
  switch (auto& component = item->asArray()->get(3); component->type()) {
    case cppbor::BSTR:
      // If the 4th element is a bstr, we're decoding a COSE_Sign1 structure.
      signature = component->asBstr()->moveValue();
      break;

    case cppbor::ARRAY:
      // If the 4th element is an array, we're decoding a COSE_Sign structure.
      // Use the signature and protected header from the first COSE_Signature,
      // which is a (protected header, unprotected header, signature) tuple.
      if (cppbor::Array* sigs = component->asArray(); sigs->size() > 0) {
        if (cppbor::Array* sig = sigs->get(0)->asArray();
            sig->size() == 3 && sig->get(0)->type() == cppbor::BSTR &&
            sig->get(0)->type() == cppbor::BSTR) {
          sign_protected = sig->get(0)->asBstr()->moveValue();
          signature = sig->get(2)->asBstr()->moveValue();
        }
      }
      break;

    default:
      break;
  }
  if (!signature) {
    return absl::InvalidArgumentError("COSE_Sign is invalid");
  }

  return std::make_tuple(
      item->asArray()->get(0)->asBstr()->moveValue(), std::move(sign_protected),
      item->asArray()->get(2)->asBstr()->moveValue(), std::move(*signature));
}

// Builds the payload for a ReleaseToken, which is a COSE_Encrypt0 object
// encoded as a bstr. See also RFC 9052 section 5.2.
std::vector<uint8_t> BuildReleaseTokenPayload(const ReleaseToken& token) {
  Array array;
  array.add(BuildProtectedHeader(token.encryption_algorithm, token.src_state,
                                 token.dst_state));

  Map unprotected_header;
  if (token.encryption_key_id) {
    unprotected_header.add(CoseHeaderParameter::kHdrKid,
                           Bstr(*token.encryption_key_id));
  }
  if (token.encapped_key) {
    unprotected_header.add(CoseHeaderParameter::kEncapsulatedKey,
                           Bstr(*token.encapped_key));
  }
  array.add(std::move(unprotected_header));

  array.add(Bstr(token.encrypted_payload));
  return array.encode();
}

// Builds a Enc_structure object for a COSE_Encrypt0 structure.
// See RFC 9052 section 5.3 for the contents of the Enc_structure.
std::string BuildEncStructure(std::vector<uint8_t> protected_header,
                              absl::string_view aad) {
  Array enc_structure;
  enc_structure.add("Encrypt0");
  enc_structure.add(std::move(protected_header));
  enc_structure.add(Bstr(aad.begin(), aad.end()));
  return enc_structure.toString();
}

// Parses a serialized ReleaseToken payload and updated the ReleaseToken.
absl::StatusOr<std::tuple<std::vector<uint8_t>, Map, std::vector<uint8_t>>>
ParseCoseEncrypt0(const std::vector<uint8_t>& encoded) {
  auto [payload, end_pos, error] = cppbor::parse(encoded);
  if (!error.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to decode COSE_Encrypt0: ", error));
  } else if (end_pos != encoded.data() + encoded.size()) {
    return absl::InvalidArgumentError(
        "failed to decode COSE_Encrypt0: input contained extra data");
  } else if (Array* array = payload->asArray();
             array == nullptr || array->size() != 3 ||
             array->get(0)->type() != cppbor::BSTR ||
             array->get(1)->type() != cppbor::MAP ||
             array->get(2)->type() != cppbor::BSTR) {
    return absl::InvalidArgumentError("COSE_Encrypt0 is invalid");
  }
  return std::make_tuple(payload->asArray()->get(0)->asBstr()->moveValue(),
                         std::move(*payload->asArray()->get(1)->asMap()),
                         payload->asArray()->get(2)->asBstr()->moveValue());
}

}  // namespace

absl::StatusOr<OkpKey> OkpKey::Decode(absl::string_view encoded) {
  auto [item, end_pos, error] = cppbor::parse(
      reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size());
  if (!error.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to decode OkpKey: ", error));
  } else if (end_pos != reinterpret_cast<const uint8_t*>(encoded.data()) +
                            encoded.size()) {
    return absl::InvalidArgumentError(
        "failed to decode OkpKey: input contained extra data");
  } else if (item->type() != cppbor::MAP) {
    return absl::InvalidArgumentError("OkpKey is invalid");
  }

  // Process the parameters map.
  std::optional<uint64_t> kty;
  OkpKey okp_key;
  for (const auto& [key, value] : *item->asMap()) {
    if (key->asInt() == nullptr) continue;  // Ignore other key types.
    switch (key->asInt()->value()) {
      case CoseKeyParameter::kKty:
        if (value->asInt() == nullptr) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported kty type ", value->type()));
        }
        kty = value->asInt()->value();
        break;

      case CoseKeyParameter::kKid:
        if (value->type() != cppbor::BSTR) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported kid type ", value->type()));
        }
        okp_key.key_id.assign(value->asBstr()->value().begin(),
                              value->asBstr()->value().end());
        break;

      case CoseKeyParameter::kAlg:
        if (value->asInt() == nullptr) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported alg type ", value->type()));
        }
        okp_key.algorithm = value->asInt()->value();
        break;

      case CoseKeyParameter::kKeyOps:
        if (value->asArray() == nullptr) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported key_ops type ", value->type()));
        }
        for (const auto& entry : *value->asArray()) {
          if (entry->asInt() == nullptr) {
            return absl::InvalidArgumentError(
                absl::StrCat("unsupported key_ops entry type", entry->type()));
          }
          okp_key.key_ops.push_back(entry->asInt()->value());
        }
        break;

      case CoseKeyParameter::kOkpCrv:
        if (value->asInt() == nullptr) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported curve type ", value->type()));
        }
        okp_key.curve = value->asInt()->value();
        break;

      case CoseKeyParameter::kOkpX:
        if (value->type() != cppbor::BSTR) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported x type ", value->type()));
        }
        okp_key.x.assign(value->asBstr()->value().begin(),
                         value->asBstr()->value().end());
        break;

      case CoseKeyParameter::kOkpD:
        if (value->type() != cppbor::BSTR) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported d type ", value->type()));
        }
        okp_key.d.assign(value->asBstr()->value().begin(),
                         value->asBstr()->value().end());
        break;

      default:
        break;
    }
  }

  if (!kty.has_value() || *kty != CoseKeyType::kOkp) {
    return absl::InvalidArgumentError("missing or wrong Cose_Key type");
  }
  return okp_key;
}

absl::StatusOr<std::string> OkpKey::Encode() const {
  // Generate a map containing the parameters that are set.
  Map map;
  map.add(CoseKeyParameter::kKty, CoseKeyType::kOkp);
  if (!key_id.empty()) {
    map.add(CoseKeyParameter::kKid, Bstr(key_id));
  }
  if (algorithm) {
    map.add(CoseKeyParameter::kAlg, *algorithm);
  }
  if (!key_ops.empty()) {
    Array array;
    for (int64_t key_op : key_ops) {
      array.add(key_op);
    }
    map.add(CoseKeyParameter::kKeyOps, std::move(array));
  }
  if (curve) {
    map.add(CoseKeyParameter::kOkpCrv, *curve);
  }
  if (!x.empty()) {
    map.add(CoseKeyParameter::kOkpX, Bstr(x));
  }
  if (!d.empty()) {
    map.add(CoseKeyParameter::kOkpD, Bstr(d));
  }
  return map.toString();
}

absl::StatusOr<SymmetricKey> SymmetricKey::Decode(absl::string_view encoded) {
  auto [item, end_pos, error] = cppbor::parse(
      reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size());
  if (!error.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to decode SymmetricKey: ", error));
  } else if (end_pos != reinterpret_cast<const uint8_t*>(encoded.data()) +
                            encoded.size()) {
    return absl::InvalidArgumentError(
        "failed to decode SymmetricKey: input contained extra data");
  } else if (item->type() != cppbor::MAP) {
    return absl::InvalidArgumentError("SymmetricKey is invalid");
  }

  // Process the parameters map.
  std::optional<uint64_t> kty;
  SymmetricKey symmetric_key;
  for (const auto& [key, value] : *item->asMap()) {
    if (key->asInt() == nullptr) continue;  // Ignore other key types.
    switch (key->asInt()->value()) {
      case CoseKeyParameter::kKty:
        if (value->asInt() == nullptr) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported kty type ", value->type()));
        }
        kty = value->asInt()->value();
        break;

      case CoseKeyParameter::kAlg:
        if (value->asInt() == nullptr) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported alg type ", value->type()));
        }
        symmetric_key.algorithm = value->asInt()->value();
        break;

      case CoseKeyParameter::kKeyOps:
        if (value->asArray() == nullptr) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported key_ops type ", value->type()));
        }
        for (const auto& entry : *value->asArray()) {
          if (entry->asInt() == nullptr) {
            return absl::InvalidArgumentError(
                absl::StrCat("unsupported key_ops entry type", entry->type()));
          }
          symmetric_key.key_ops.push_back(entry->asInt()->value());
        }
        break;

      case CoseKeyParameter::kSymmetricK:
        if (value->type() != cppbor::BSTR) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported k type ", value->type()));
        }
        symmetric_key.k.assign(value->asBstr()->value().begin(),
                               value->asBstr()->value().end());
        break;

      default:
        break;
    }
  }

  if (!kty.has_value() || *kty != CoseKeyType::kSymmetric) {
    return absl::InvalidArgumentError("missing or wrong Cose_Key type");
  }
  return symmetric_key;
}

absl::StatusOr<std::string> SymmetricKey::Encode() const {
  // Generate a map containing the parameters that are set.
  Map map;
  map.add(CoseKeyParameter::kKty, CoseKeyType::kSymmetric);
  if (algorithm) {
    map.add(CoseKeyParameter::kAlg, *algorithm);
  }
  if (!key_ops.empty()) {
    Array array;
    for (int64_t key_op : key_ops) {
      array.add(key_op);
    }
    map.add(CoseKeyParameter::kKeyOps, std::move(array));
  }
  if (!k.empty()) {
    map.add(CoseKeyParameter::kSymmetricK, Bstr(k));
  }
  return map.toString();
}

absl::StatusOr<std::string> OkpCwt::BuildSigStructureForSigning(
    absl::string_view aad) const {
  std::vector<uint8_t> protected_header =
      BuildProtectedHeader(algorithm, /*src_state=*/std::nullopt,
                           /*dst_state=*/std::nullopt);
  FCP_ASSIGN_OR_RETURN(std::vector<uint8_t> payload, BuildCwtPayload(*this));
  return BuildSigStructure(std::move(protected_header), std::nullopt, aad,
                           std::move(payload));
}

absl::StatusOr<std::string> OkpCwt::GetSigStructureForVerifying(
    absl::string_view encoded, absl::string_view aad) {
  std::vector<uint8_t> body_protected, payload;
  std::optional<std::vector<uint8_t>> sign_protected;
  FCP_ASSIGN_OR_RETURN(
      std::tie(body_protected, sign_protected, payload, std::ignore),
      ParseCoseSign(encoded));
  return BuildSigStructure(std::move(body_protected), std::move(sign_protected),
                           aad, std::move(payload));
}

absl::StatusOr<OkpCwt> OkpCwt::Decode(absl::string_view encoded) {
  std::vector<uint8_t> body_protected, payload, signature;
  std::optional<std::vector<uint8_t>> sign_protected;
  FCP_ASSIGN_OR_RETURN(
      std::tie(body_protected, sign_protected, payload, signature),
      ParseCoseSign(encoded));
  OkpCwt cwt;
  // When decoding a COSE_Sign structure, information will be in the signer
  // protected header instead of the body protected header.
  FCP_RETURN_IF_ERROR(ParseProtectedHeader(
      sign_protected ? *sign_protected : body_protected, &cwt.algorithm,
      /*src_state=*/nullptr,
      /*dst_state=*/nullptr));
  FCP_RETURN_IF_ERROR(ParseCwtPayload(payload, cwt));
  cwt.signature = std::string(signature.begin(), signature.end());
  return cwt;
}

absl::StatusOr<std::string> OkpCwt::Encode() const {
  // See RFC 9052 section 4.2 for the contents of the COSE_Sign1 structure.
  Array array;
  array.add(BuildProtectedHeader(algorithm, /*src_state=*/std::nullopt,
                                 /*dst_state=*/std::nullopt));
  array.add(Map());  // unprotected header
  FCP_ASSIGN_OR_RETURN(std::vector<uint8_t> payload, BuildCwtPayload(*this));
  array.add(std::move(payload));
  array.add(Bstr(signature));
  return array.toString();
}

absl::StatusOr<std::string> ReleaseToken::BuildEncStructureForEncrypting(
    absl::string_view aad) const {
  std::vector<uint8_t> protected_header =
      BuildProtectedHeader(encryption_algorithm, src_state, dst_state);
  return BuildEncStructure(std::move(protected_header), aad);
}

absl::StatusOr<std::string> ReleaseToken::GetEncStructureForDecrypting(
    absl::string_view encoded, absl::string_view aad) {
  std::vector<uint8_t> payload;
  FCP_ASSIGN_OR_RETURN(std::tie(std::ignore, std::ignore, payload, std::ignore),
                       ParseCoseSign(encoded));
  std::vector<uint8_t> protected_header;
  FCP_ASSIGN_OR_RETURN(std::tie(protected_header, std::ignore, std::ignore),
                       ParseCoseEncrypt0(payload));
  return BuildEncStructure(std::move(protected_header), aad);
}

absl::StatusOr<std::string> ReleaseToken::BuildSigStructureForSigning(
    absl::string_view aad) const {
  std::vector<uint8_t> protected_header =
      BuildProtectedHeader(signing_algorithm, /*src_state=*/std::nullopt,
                           /*dst_state=*/std::nullopt);
  std::vector<uint8_t> payload = BuildReleaseTokenPayload(*this);
  return BuildSigStructure(std::move(protected_header), std::nullopt, aad,
                           std::move(payload));
}

absl::StatusOr<std::string> ReleaseToken::GetSigStructureForVerifying(
    absl::string_view encoded, absl::string_view aad) {
  // Like a CWT, a ReleaseToken is also a COSE_Sign1 object, so the
  // Sig_structure is the same.
  return OkpCwt::GetSigStructureForVerifying(encoded, aad);
}

absl::StatusOr<ReleaseToken> ReleaseToken::Decode(absl::string_view encoded) {
  ReleaseToken token;

  // Parse the outer COSE_Sign1 structure.
  std::vector<uint8_t> protected_header, payload, signature;
  FCP_ASSIGN_OR_RETURN(
      std::tie(protected_header, std::ignore, payload, signature),
      ParseCoseSign(encoded));
  FCP_RETURN_IF_ERROR(
      ParseProtectedHeader(protected_header, &token.signing_algorithm,
                           /*src_state=*/nullptr, /*dst_state=*/nullptr));
  token.signature = std::string(signature.begin(), signature.end());

  // Parse the inner COSE_Encrypt0 structure.
  Map unprotected_header;
  FCP_ASSIGN_OR_RETURN(std::tie(protected_header, unprotected_header, payload),
                       ParseCoseEncrypt0(payload));
  FCP_RETURN_IF_ERROR(ParseProtectedHeader(protected_header,
                                           &token.encryption_algorithm,
                                           &token.src_state, &token.dst_state));
  token.encrypted_payload = std::string(payload.begin(), payload.end());

  // Process the COSE_Encrypt0 unprotected header.
  for (const auto& [key, value] : unprotected_header) {
    if (key->asInt() == nullptr) continue;  // Ignore other key types.
    switch (key->asInt()->value()) {
      case CoseHeaderParameter::kHdrKid:
        if (value->type() != cppbor::BSTR) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported kid type ", value->type()));
        }
        token.encryption_key_id = std::string(value->asBstr()->value().begin(),
                                              value->asBstr()->value().end());
        break;

      case CoseHeaderParameter::kEncapsulatedKey:
        if (value->type() != cppbor::BSTR) {
          return absl::InvalidArgumentError(absl::StrCat(
              "unsupported encapsulated key type ", value->type()));
        }
        token.encapped_key = std::string(value->asBstr()->value().begin(),
                                         value->asBstr()->value().end());
        break;

      default:
        break;
    }
  }

  return token;
}

absl::StatusOr<std::string> ReleaseToken::Encode() const {
  // See RFC 9052 section 4.2 for the contents of the COSE_Sign1 structure.
  Array array;
  array.add(BuildProtectedHeader(signing_algorithm, /*src_state=*/std::nullopt,
                                 /*dst_state=*/std::nullopt));
  array.add(Map());  // unprotected header
  array.add(BuildReleaseTokenPayload(*this));
  array.add(Bstr(signature));
  return array.toString();
}

#else  // defined(FCP_CLIENT_SUPPORT_CONFIDENTIAL_AGG)
absl::StatusOr<OkpKey> OkpKey::Decode(absl::string_view encoded) {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

absl::StatusOr<std::string> OkpKey::Encode() const {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

absl::StatusOr<SymmetricKey> SymmetricKey::Decode(absl::string_view encoded) {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

absl::StatusOr<std::string> SymmetricKey::Encode() const {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

absl::StatusOr<std::string> OkpCwt::BuildSigStructureForSigning(
    absl::string_view aad) const {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

absl::StatusOr<std::string> OkpCwt::GetSigStructureForVerifying(
    absl::string_view encoded, absl::string_view aad) {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

absl::StatusOr<OkpCwt> OkpCwt::Decode(absl::string_view encoded) {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

absl::StatusOr<std::string> OkpCwt::Encode() const {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

absl::StatusOr<std::string> ReleaseToken::BuildEncStructureForEncrypting(
    absl::string_view aad) const {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

absl::StatusOr<std::string> ReleaseToken::GetEncStructureForDecrypting(
    absl::string_view encoded, absl::string_view aad) {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

absl::StatusOr<std::string> ReleaseToken::BuildSigStructureForSigning(
    absl::string_view aad) const {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

absl::StatusOr<std::string> ReleaseToken::GetSigStructureForVerifying(
    absl::string_view encoded, absl::string_view aad) {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

absl::StatusOr<ReleaseToken> ReleaseToken::Decode(absl::string_view encoded) {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

absl::StatusOr<std::string> ReleaseToken::Encode() const {
  return absl::UnimplementedError(
      "Confidential Aggregation is not supported on this platform.");
}

#endif  // defined(FCP_CLIENT_SUPPORT_CONFIDENTIAL_AGG)
}  // namespace fcp::confidential_compute
