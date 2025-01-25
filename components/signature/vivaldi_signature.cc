#include <string>
#include <vector>
#include <optional>

#include "base/base64.h"
#include "base/logging.h"
#include "base/version_info/version_info.h"
#include "chromium/base/command_line.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"

#include "app/vivaldi_version_info.h"
#include "components/signature/vivaldi_signature.h"

namespace vivaldi {
namespace {

static constexpr const char * kDebuggingSearchEngines = "debug-search-engines";
static constexpr const char * kSearchEnginesUrl = "search-engines-url";
static constexpr const char * kSearchEnginesPromptUrl = "search-engines-prompt-url";

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

std::string GetSearchEnginesTemplate() {
  if (!version_info::IsOfficialBuild()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(kSearchEnginesUrl)) {
      return command_line->GetSwitchValueASCII(kSearchEnginesUrl);
    }
    return kSignedResourceSearchEnginesSopranos;
  }
  if (ReleaseKind() <= Release::kSnapshot) {
    return kSignedResourceSearchEnginesSnapshot;
  }
  return kSignedResourceSearchEngines;
}

std::string GetSearchEnginesPromptTemplate() {
  if (!version_info::IsOfficialBuild()) {
    return kSignedResourceSearchEnginesPromptSopranos;
  }
  if (ReleaseKind() <= Release::kSnapshot) {
    return kSignedResourceSearchEnginesPromptSnapshot;
  }
  return kSignedResourceSearchEnginesPrompt;
}
}  // namespace

bool VerifyJsonSignature(const std::string& json) {
  if (IsDebuggingSearchEngines()) {
    return true;
  }

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
      url_template = GetSearchEnginesTemplate();
      break;
    case SignedResourceUrl::kDirectMatchUrl:
      url_template = kSignedResourceDirectMatch;
      break;
    case SignedResourceUrl::kSearchEnginesPromptUrl:
      url_template = GetSearchEnginesPromptTemplate();
      break;
  }

  size_t pos = url_template.find("{}");
  if (pos == std::string::npos) {
    return url_template;
  }

  url_template.replace(pos, 2, kECDSAPublicKeyCreationTime);
  return url_template;
}

bool UsesCustomSearchEnginesUrl() {
  if (version_info::IsOfficialBuild()) {
    return false;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kSearchEnginesUrl);
}

bool UsesCustomSearchEnginesPromptUrl() {
  if (version_info::IsOfficialBuild()) {
    return false;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kSearchEnginesPromptUrl);
}

bool IsDebuggingSearchEngines() {
  static std::optional<bool> result;

  if (result) {
    return *result;
  }

  result = false;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kDebuggingSearchEngines)) {
    return false;
  }

  if (version_info::IsOfficialBuild()) {
    VLOG(1) << "Option not supported";
    return false;
  }

  VLOG(1) << "Debugging search engines.";

  result = true;
  return true;
}

}  // namespace vivaldi
