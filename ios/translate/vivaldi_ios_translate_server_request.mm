// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/translate/vivaldi_ios_translate_server_request.h"

#import <algorithm>
#import <string>
#import <vector>

#import "base/command_line.h"
#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/vivaldi_switches.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "net/base/load_flags.h"
#import "prefs/vivaldi_pref_names.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#import "services/network/public/cpp/simple_url_loader.h"

namespace vivaldi {

namespace {
constexpr char kTranslateLanguageServerUrl[] =
    "https://mimir2.vivaldi.com/api/translate";

constexpr char kSourceLanguageKey[] = "source";
constexpr char kTargetLanguageKey[] = "target";
constexpr char kStringsLanguageKey[] = "q";

constexpr int kMaxTranslateResponse = 1024 * 1024;
}  // namespace

VivaldiIOSTranslateServerRequest::VivaldiIOSTranslateServerRequest(
    PrefService* prefs,
    VivaldiTranslateTextCallback callback)
    : prefs_(prefs), callback_(callback) {}

VivaldiIOSTranslateServerRequest::VivaldiIOSTranslateServerRequest() {}

VivaldiIOSTranslateServerRequest::~VivaldiIOSTranslateServerRequest() = default;

const std::string VivaldiIOSTranslateServerRequest::GetServer() {
  std::string server = kTranslateLanguageServerUrl;
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  if (cmd_line.HasSwitch(switches::kTranslateServerUrl)) {
    server = cmd_line.GetSwitchValueASCII(switches::kTranslateServerUrl);
  }
  return server;
}

std::string VivaldiIOSTranslateServerRequest::GenerateJSON(
    const std::vector<std::string>& data,
    const std::string& source_language,
    const std::string& destination_language) {
  base::Value request(base::Value::Type::DICT);
  base::Value string_collection(base::Value::Type::LIST);

  if (source_language != "") {
    request.GetDict().Set(kSourceLanguageKey, source_language);
  }
  request.GetDict().Set(kTargetLanguageKey, destination_language);

  for (auto& item : data) {
    string_collection.GetList().Append(item);
  }
  request.GetDict().Set(kStringsLanguageKey, std::move(string_collection));

  std::string output;

  base::JSONWriter::Write(request, &output);

  return output;
}

void VivaldiIOSTranslateServerRequest::StartRequest(
    const std::vector<std::string>& data,
    const std::string& source_language,
    const std::string& destination_language) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(GetServer());
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_BYPASS_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // See
  // https://chromium.googlesource.com/chromium/src/+/lkgr/docs/network_traffic_annotations.md
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("vivaldi_translate_server_request", R"(
        semantics {
          sender: "Vivaldi Translate Server Request"
          description: "Translate text to a specific language."
          trigger: "Triggered on user action, such as using translate in a panel."
          data: "JSON format array of text to be translated."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled."
        }
      })");

  auto url_loader_factory = GetApplicationContext()->GetSharedURLLoaderFactory();

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  url_loader_->SetAllowHttpErrorResults(true);

  std::string body = GenerateJSON(data, source_language, destination_language);

  url_loader_->AttachStringForUpload(body, "application/json");

  url_loader_->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&VivaldiIOSTranslateServerRequest::OnRequestResponse,
                     weak_factory_.GetWeakPtr()),
      kMaxTranslateResponse);
}

bool VivaldiIOSTranslateServerRequest::IsRequestInProgress() {
  return url_loader_ != nullptr;
}

void VivaldiIOSTranslateServerRequest::AbortRequest() {
  if (url_loader_) {
    url_loader_.reset(nullptr);
  }
}

void VivaldiIOSTranslateServerRequest::OnRequestResponse(
    std::unique_ptr<std::string> response_body) {

  if (!response_body || response_body->empty()) {
    LOG(WARNING) << "Translate from server "
                 << " failed with error " << url_loader_->NetError();
    if (callback_) {
      callback_(TranslateError::kNetwork,
                std::string(),
                std::vector<std::string>(),
                std::vector<std::string>());
    }
  } else {
    base::JSONParserOptions options = base::JSON_ALLOW_TRAILING_COMMAS;
    std::optional<base::Value> json =
        base::JSONReader::Read(*response_body, options);
    if (json) {
      const base::Value::List* translated_values;
      const base::Value::List* source_values;
      if (json->is_dict()) {
        std::vector<std::string> translated_strings;
        std::vector<std::string> source_strings;
        /*
          Typical error, check for that first:
          {
            "message": "Both or one lang is invalid - source: [object Object], target: en",
            "code": "INVALID_LANG_CODE"
          }
          {
            "message": "Unable to recognize source language",
            "code": "LANGUAGE_NOT_RECOGNIZED"
          }
        */
        const std::string* detected_source_language = nullptr;
        ::vivaldi::TranslateError error = TranslateError::kNoError;
        const std::string* error_code = json->GetDict().FindString("code");
        if (error_code) {
          if (error_code->compare("LANGUAGE_NOT_RECOGNIZED") == 0) {
            error = TranslateError::kUnknownLanguage;
          } else if (error_code->compare("INVALID_LANG_CODE") == 0) {
            error = TranslateError::kUnsupportedLanguage;
          } else if (error_code->compare("TIMEOUT_ERROR") == 0) {
            error = TranslateError::kTranslationTimeout;
          } else {
            error = TranslateError::kTranslationError;
          }
        } else {
          detected_source_language =
              json->GetDict().FindString("detectedSourceLanguage");

          translated_values = json->GetDict().FindList("translatedText");
          if (translated_values) {
            for (const auto& value : *translated_values) {
              translated_strings.push_back(value.GetString());
            }
          }
          source_values = json->GetDict().FindList("sourceText");
          if (source_values) {
            for (const auto& value : *source_values) {
              source_strings.push_back(value.GetString());
            }
          }
        }
        url_loader_.reset(nullptr);

        if (callback_) {
          callback_(error,
                    detected_source_language ? *detected_source_language : "",
                    source_strings,
                    translated_strings);
        }

        // Reset the callback to prevent multiple invocations
        callback_ = nil;
      }
    }
  }
}

}  // namespace vivaldi
