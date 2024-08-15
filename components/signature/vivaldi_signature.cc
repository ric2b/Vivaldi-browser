#include <string>
#include <vector>
#include <optional>

#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"
#include "base/base64.h"

#include "components/signature/vivaldi_signature.h"

namespace vivaldi {
namespace {

#include "vivaldi_key.inc"

std::optional<std::vector<uint8_t>> ParseSignature(std::string_view& json) {
  if (!json.starts_with("// ")) {
    return std::nullopt;
  }

  auto first_line_break = json.find_first_of('\n');
  if (first_line_break == std::string::npos) {
    return std::nullopt;
  }

  auto decoded = base::Base64Decode(json.substr(3, first_line_break - 3));
  if (!decoded) {
    return std::nullopt;
  }

  json.remove_prefix(first_line_break + 1);
  return decoded;
}

}  // namespace

bool VerifyJsonSignature(const std::string& json) {
  std::string_view json_view = json;
  auto signature = ParseSignature(json_view);
  if (!signature) {
    return false;
  }

  base::span<const uint8_t> signature_span(signature->data(),
                                           signature->size());
  base::span<const uint8_t> public_key_span(kECDSAPublicKey,
                                            kECDSAPublicKeyLen);

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(crypto::SignatureVerifier::ECDSA_SHA256,
                           signature_span, public_key_span)) {
    return false;
  }

  base::span<const uint8_t> data_span(
      reinterpret_cast<const uint8_t*>(json_view.data()), json_view.size());

  verifier.VerifyUpdate(data_span);
  if (!verifier.VerifyFinal()) {
    return false;
  }

  return true;
}

std::string GetSignedResourceUrl(vivaldi::SignedResourceUrl url_id) {
  std::string url_template;
  switch (url_id) {
    case SignedResourceUrl::kSearchEnginesUrl:
      url_template = kSignedResourceSearchEngines;
      break;
    case SignedResourceUrl::kDirectMatchUrl:
      url_template = kSignedResourceDirectMatch;
      break;
  }

  size_t pos = url_template.find("{}");
  if (pos == std::string::npos) {
    return url_template;
  }

  url_template.replace(pos, 2, kECDSAPublicKeyCreationTime);
  return url_template;
}
}  // namespace vivaldi
