// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "browser/translate/vivaldi_translate_server_request.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/vivaldi_switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "prefs/vivaldi_pref_names.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"

namespace vivaldi {

namespace {
constexpr char kTranslateLanguageServerUrl[] =
    "https://mimir2.vivaldi.com/api/translate";

constexpr char kSourceLanguageKey[] = "source";
constexpr char kTargetLanguageKey[] = "target";
constexpr char kStringsLanguageKey[] = "q";

constexpr int kMaxTranslateResponse = 1024 * 1024;
}  // namespace

VivaldiTranslateServerRequest::VivaldiTranslateServerRequest(
    base::WeakPtr<Profile> profile,
    VivaldiTranslateTextCallback callback)
    : profile_(std::move(profile)), callback_(std::move(callback)) {}

VivaldiTranslateServerRequest::VivaldiTranslateServerRequest() {}

VivaldiTranslateServerRequest::~VivaldiTranslateServerRequest() = default;

const std::string VivaldiTranslateServerRequest::GetServer() {
  std::string server = kTranslateLanguageServerUrl;
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  if (cmd_line.HasSwitch(switches::kTranslateServerUrl)) {
    server = cmd_line.GetSwitchValueASCII(switches::kTranslateServerUrl);
  }
  return server;
}

std::string VivaldiTranslateServerRequest::GenerateJSON(
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

void VivaldiTranslateServerRequest::StartRequest(
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

  auto url_loader_factory = profile_->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  url_loader_->SetAllowHttpErrorResults(true);

  std::string body = GenerateJSON(data, source_language, destination_language);

  url_loader_->AttachStringForUpload(body, "application/json");

  url_loader_->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&VivaldiTranslateServerRequest::OnRequestResponse,
                     weak_factory_.GetWeakPtr()),
      kMaxTranslateResponse);
}

bool VivaldiTranslateServerRequest::IsRequestInProgress() {
  return url_loader_ != nullptr;
}

void VivaldiTranslateServerRequest::AbortRequest() {
  if (url_loader_) {
    url_loader_.reset(nullptr);
  }
}

void VivaldiTranslateServerRequest::OnRequestResponse(
    std::unique_ptr<std::string> response_body) {

  if (!response_body || response_body->empty()) {
    LOG(WARNING) << "Translate from server "
                 << " failed with error " << url_loader_->NetError();
    std::move(callback_).Run(TranslateError::kNetwork, std::string(),
                             std::vector<std::string>(),
                             std::vector<std::string>());
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

        // `this` can be deleted after the callback call.
        std::move(callback_).Run(
            error, detected_source_language ? *detected_source_language : "",
            source_strings, translated_strings);
      }
    }
  }
}

}  // namespace vivaldi
