// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_TRANSLATE_VIVALDI_IOS_TRANSLATE_SERVER_REQUEST_H_
#define IOS_TRANSLATE_VIVALDI_IOS_TRANSLATE_SERVER_REQUEST_H_

#include <memory>
#include <string>
#include <vector>

#import "base/gtest_prod_util.h"
#import "base/timer/timer.h"
#import "base/values.h"

class PrefService;

namespace content {
class BrowserContext;
}

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace vivaldi {

FORWARD_DECLARE_TEST(VivaldiIOSTranslateServerRequestTest, GenerateJSON);
FORWARD_DECLARE_TEST(VivaldiIOSTranslateServerRequestTest, OnRequestResponse);

class VivaldiIOSTranslateServerRequestTest;

enum TranslateError {
  kNoError = 0,
  kNetwork,                    // No connectivity.
  kUnknownLanguage,            // The page's language could not be detected.
  kUnsupportedLanguage,        // The server detected a language that the browser
                               // does not know.
  kTranslationError,           // General translation error.
  kTranslationTimeout,         // Timeout during translation.
};

typedef void (^VivaldiTranslateTextCallback)(
    vivaldi::TranslateError error,
    const std::string& detected_source_language,
    const std::vector<std::string>& sourceText,
    const std::vector<std::string>& translatedText);

class VivaldiIOSTranslateServerRequest {
 public:
  VivaldiIOSTranslateServerRequest(PrefService* prefs,
                                   VivaldiTranslateTextCallback callback);
  VivaldiIOSTranslateServerRequest();

  virtual ~VivaldiIOSTranslateServerRequest();
  VivaldiIOSTranslateServerRequest(const VivaldiIOSTranslateServerRequest&) = delete;
  VivaldiIOSTranslateServerRequest& operator=(const VivaldiIOSTranslateServerRequest&) =
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
  FRIEND_TEST(VivaldiIOSTranslateServerRequestTest, GenerateJSON);
  FRIEND_TEST(VivaldiIOSTranslateServerRequestTest, OnRequestResponse);

  std::string GenerateJSON(const std::vector<std::string>& data,
                           const std::string& source_language,
                           const std::string& destination_language);
  void OnRequestResponse(std::unique_ptr<std::string> response_body);
  const std::string GetServer();

  void SetCallbackForTesting(VivaldiTranslateTextCallback callback) {
    callback_ = std::move(callback);
  }

  PrefService* prefs_;

  VivaldiTranslateTextCallback callback_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<VivaldiIOSTranslateServerRequest> weak_factory_{this};
};

}  // namespace vivaldi

#endif  // BROWSER_TRANSLATE_VIVALDI_LANGUAGE_LIST_H_
