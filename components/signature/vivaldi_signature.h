#ifndef COMPONENTS_SIGNATURE_VIVALDI_SIGNATURE_H_
#define COMPONENTS_SIGNATURE_VIVALDI_SIGNATURE_H_

#include <string>

namespace vivaldi {

enum struct SignedResourceUrl {
  kSearchEnginesUrl,
  kSearchEnginesPromptUrl,
  kDirectMatchUrl,
};

bool VerifyJsonSignature(const std::string& json);
std::string GetSignedResourceUrl(SignedResourceUrl url_id);
bool IsDebuggingSearchEngines();
bool UsesCustomSearchEnginesUrl();
bool UsesCustomSearchEnginesPromptUrl();
}

#endif // COMPONENTS_SIGNATURE_VIVALDI_SIGNATURE_H_
