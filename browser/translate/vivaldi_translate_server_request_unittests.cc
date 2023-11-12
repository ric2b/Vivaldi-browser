// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include <memory>
#include <string>

#include "browser/translate/vivaldi_translate_server_request.h"

#include "app/vivaldi_constants.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vivaldi {

class VivaldiTranslateServerRequestTest : public testing::Test {
 public:
  void CheckRequestResponse(TranslateError error,
                            std::string detected_source_language,
                            std::vector<std::string> sourceText,
                            std::vector<std::string> translatedText) {
    EXPECT_EQ(error, TranslateError::kNoError);
    EXPECT_EQ(detected_source_language, "nb");
    EXPECT_EQ(translatedText[0], "Dette er en test.");
    EXPECT_EQ(translatedText[1], "Hallo verden!");
  }
};

constexpr char kGenerateJSONResult[] =
    "{\"q\":[\"This is a test.\",\"Hello "
    "world!\"],\"source\":\"en\",\"target\":\"ru\"}";

constexpr char kServerResponseJSON[] =
    "{\"detectedSourceLanguage\": \"nb\", \"sourceText\": [\"This is a "
    "test.\", \"Hello world!\"], \"translatedText\": [\"Dette er en test.\", "
    "\"Hallo verden!\"]}";

TEST_F(VivaldiTranslateServerRequestTest, GenerateJSON) {
  std::unique_ptr<VivaldiTranslateServerRequest> request =
      std::make_unique<VivaldiTranslateServerRequest>();

  std::vector<std::string> strings;

  strings.push_back("This is a test.");
  strings.push_back("Hello world!");

  std::string data =
      request->GenerateJSON(strings, "en", "ru");
  EXPECT_EQ(data, kGenerateJSONResult);
}

TEST_F(VivaldiTranslateServerRequestTest, OnRequestResponse) {
  std::unique_ptr<VivaldiTranslateServerRequest> request =
      std::make_unique<VivaldiTranslateServerRequest>();

  std::unique_ptr<std::string> response =
      std::make_unique<std::string>(kServerResponseJSON);

  request->SetCallbackForTesting(
      base::BindOnce(&VivaldiTranslateServerRequestTest::CheckRequestResponse,
                     base::Unretained(this)));
  request->OnRequestResponse(std::move(response));
}

}  // namespace vivaldi