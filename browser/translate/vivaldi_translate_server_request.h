// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_VIVALDI_TRANSLATE_SERVER_REQUEST_H_
#define BROWSER_VIVALDI_TRANSLATE_SERVER_REQUEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/timer/timer.h"
#include "base/values.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace vivaldi {

FORWARD_DECLARE_TEST(VivaldiTranslateServerRequestTest, GenerateJSON);
FORWARD_DECLARE_TEST(VivaldiTranslateServerRequestTest, OnRequestResponse);

class VivaldiTranslateServerRequestTest;

enum TranslateError {
  kNoError = 0,
  kNetwork,                    // No connectivity.
  kUnknownLanguage,            // The page's language could not be detected.
  kUnsupportedLanguage,        // The server detected a language that the browser
                               // does not know.
  kTranslationError,           // General translation error.
  kTranslationTimeout,         // Timeout during translation.
};

using VivaldiTranslateTextCallback =
    base::OnceCallback<void(TranslateError error,
                            std::string detected_source_language,
                            std::vector<std::string> sourceText,
                            std::vector<std::string> translatedText)>;

class VivaldiTranslateServerRequest {
 public:
  VivaldiTranslateServerRequest(base::WeakPtr<Profile> profile,
                                VivaldiTranslateTextCallback callback);
  VivaldiTranslateServerRequest();

  virtual ~VivaldiTranslateServerRequest();
  VivaldiTranslateServerRequest(const VivaldiTranslateServerRequest&) = delete;
  VivaldiTranslateServerRequest& operator=(const VivaldiTranslateServerRequest&) =
      delete;

  // Given an array of strings and language codes, it will request
  // a translation from the server.
  void StartRequest(const std::vector<std::string>& data,
                    const std::string& source_language,
                    const std::string& destination_language);

  // Returns true if there is an ongoing network request to the translation
  // server.
  bool IsRequestInProgress();

  // Abort the ongoing network request. Can be safely called even if no request
  // is ongoing.
  void AbortRequest();

 private:
  FRIEND_TEST(VivaldiTranslateServerRequestTest, GenerateJSON);
  FRIEND_TEST(VivaldiTranslateServerRequestTest, OnRequestResponse);

  std::string GenerateJSON(const std::vector<std::string>& data,
                           const std::string& source_language,
                           const std::string& destination_language);
  void OnRequestResponse(std::unique_ptr<std::string> response_body);
  const std::string GetServer();

  void SetCallbackForTesting(VivaldiTranslateTextCallback callback) {
    callback_ = std::move(callback);
  }

  base::WeakPtr<Profile> profile_;

  VivaldiTranslateTextCallback callback_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<VivaldiTranslateServerRequest> weak_factory_{this};
};

}  // namespace vivaldi

#endif  // BROWSER_TRANSLATE_VIVALDI_LANGUAGE_LIST_H_
