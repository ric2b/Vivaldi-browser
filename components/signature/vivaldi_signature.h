#ifndef COMPONENTS_SIGNATURE_VIVALDI_SIGNATURE_H_
#define COMPONENTS_SIGNATURE_VIVALDI_SIGNATURE_H_

#include <string>

namespace vivaldi {

enum struct SignedResourceUrl {
  kSearchEnginesUrl,
  kDirectMatchUrl,
};

bool VerifyJsonSignature(const std::string& json);
std::string GetSignedResourceUrl(SignedResourceUrl url_id);
}

#endif // COMPONENTS_SIGNATURE_VIVALDI_SIGNATURE_H_
