// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/translate/vivaldi_ios_translate_service.h"

#import "app/vivaldi_constants.h"
#import "base/command_line.h"
#import "base/functional/bind.h"
#import "base/json/json_reader.h"
#import "base/notreached.h"
#import "base/vivaldi_switches.h"
#import "components/language/core/browser/language_model.h"
#import "components/prefs/pref_service.h"
#import "components/translate/core/browser/translate_download_manager.h"
#import "components/translate/core/browser/translate_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/translate/vivaldi_ios_translate_client.h"
#import "net/base/load_flags.h"
#import "prefs/vivaldi_pref_names.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/simple_url_loader.h"
#import "url/gurl.h"

namespace {
// The singleton instance of VivaldiIOSTranslateService.
VivaldiIOSTranslateService* g_translate_service = nullptr;
}  // namespace

namespace {
constexpr char kTranslateLanguageListUrl[] =
    "https://mimir2.vivaldi.com/list/languages.json";
// Hours between language list downloads.  Static file on the server,
// so short interval is acceptable.
constexpr int kLanguageListUpdateInterval = 8;

// Check if we should download list every hour
constexpr int kCheckIntervalHours = 1;

// 10k should be more than enough
constexpr int kMaxListSize = 1024 * 10;

}  // namespace

VivaldiIOSTranslateService::VivaldiIOSTranslateService()
    : resource_request_allowed_notifier_(
          GetApplicationContext()->GetLocalState(),
          nullptr,
          base::BindOnce(&ApplicationContext::GetNetworkConnectionTracker,
              base::Unretained(GetApplicationContext()))), weak_factory_(this) {
  resource_request_allowed_notifier_.Init(this, true /* leaky */);
}

VivaldiIOSTranslateService::~VivaldiIOSTranslateService() {}

// static
void VivaldiIOSTranslateService::Initialize() {
  if (g_translate_service) {
    return;
  }

  g_translate_service = new VivaldiIOSTranslateService;
  // Initialize the allowed state for resource requests.
  g_translate_service->OnResourceRequestsAllowed();
  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  download_manager->set_url_loader_factory(
      GetApplicationContext()->GetSharedURLLoaderFactory());
  download_manager->set_application_locale(
      GetApplicationContext()->GetApplicationLocale());

  // Set ours prefs list as default in Chromium.
  g_translate_service->SetPrefsListAsDefault();
}

// static
void VivaldiIOSTranslateService::Shutdown() {
  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  download_manager->Shutdown();
  delete g_translate_service;
  g_translate_service = nullptr;
}

void VivaldiIOSTranslateService::OnResourceRequestsAllowed() {
  bool can_update =
      resource_request_allowed_notifier_.ResourceRequestsAllowed();
  if (can_update && can_update_ != can_update) {
    can_update_ = can_update;
    StartDownload();
  }
}

void VivaldiIOSTranslateService::StartUpdateTimer() {
  // Keep a timer firing every hour to check if we need to download
  // an updated list again.
  if (!update_timer_.IsRunning()) {
    update_timer_.Start(
        FROM_HERE, base::Hours(kCheckIntervalHours),
        base::BindOnce(&VivaldiIOSTranslateService::StartDownload,
                       weak_factory_.GetWeakPtr()));
  }
}

void VivaldiIOSTranslateService::SetPrefsListAsDefault() {
  PrefService* prefs = GetApplicationContext()->GetLocalState();
  auto& list = prefs->GetList(vivaldiprefs::kVivaldiTranslateLanguageList);
  SetListInChromium(list);
}

void VivaldiIOSTranslateService::SetListInChromium(
    const base::Value::List& list) {
  translate::TranslateLanguageList* language_list =
      translate::TranslateDownloadManager::GetInstance()->language_list();
  if (language_list) {
    std::vector<std::string> lang_list;
    for (const auto& value : list) {
      if (value.is_string()) {
        lang_list.push_back(value.GetString());
      }
    }
    if (lang_list.size()) {
      // The chromium language list code likes the list sorted.
      std::sort(lang_list.begin(), lang_list.end());

      language_list->SetLanguageList(lang_list);
    }
  } else {
    NOTREACHED();
  }
}

bool VivaldiIOSTranslateService::ShouldUpdate() {
  if (!can_update_) {
    return false;
  }
  if (GetServer() != kTranslateLanguageListUrl) {
    // Always update when using a custom url
    return true;
  }
  PrefService* prefs = GetApplicationContext()->GetLocalState();
  base::Time last_update =
      prefs->GetTime(vivaldiprefs::kVivaldiTranslateLanguageListLastUpdate);
  if (last_update.is_null()) {
    return true;
  }
  if ((last_update + base::Hours(kLanguageListUpdateInterval)) <
      base::Time::Now()) {
    return true;
  }
  return false;
}
const std::string VivaldiIOSTranslateService::GetServer() {
  std::string server = kTranslateLanguageListUrl;
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  if (cmd_line.HasSwitch(switches::kTranslateLanguageListUrl)) {
    server = cmd_line.GetSwitchValueASCII(switches::kTranslateLanguageListUrl);
  }
  return server;
}

void VivaldiIOSTranslateService::StartDownload() {
  if (!ShouldUpdate()) {
    StartUpdateTimer();
    return;
  }
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(GetServer());
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_BYPASS_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // See
  // https://chromium.googlesource.com/chromium/src/+/lkgr/docs/network_traffic_annotations.md
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("vivaldi_translate_language_list", R"(
        semantics {
          sender: "Vivaldi Translate Server Language List"
          description: "Download supported languages from the translate server."
          trigger: "Triggered at regular intervals."
          data: "JSON format array of language codes currently supported by the server."
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

  url_loader_->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&VivaldiIOSTranslateService::OnListDownloaded,
                     weak_factory_.GetWeakPtr()),
      kMaxListSize);
}

void VivaldiIOSTranslateService::OnListDownloaded(
    std::unique_ptr<std::string> response_body) {
  do {
    if (!response_body || response_body->empty()) {
      LOG(WARNING) << "Downloading language list from server "
                   << " failed with error " << url_loader_->NetError();
      break;
    }
    base::JSONParserOptions options = base::JSON_ALLOW_TRAILING_COMMAS;
    std::optional<base::Value> json =
        base::JSONReader::Read(*response_body, options);
    if (!json) {
      LOG(ERROR) << "Invalid language list JSON";
      break;
    }
    base::Value::List* list = json->GetIfList();
    if (!list ||
        !std::all_of(list->begin(), list->end(),
                     [](const base::Value& v) { return v.is_string(); })) {
      LOG(ERROR) << "Invalid language list structure";
      break;
    }
    if (list->empty())
      break;

    // The chromium language list code likes the list sorted.
    std::sort(list->begin(), list->end(),
              [](const base::Value& v1, const base::Value& v2) {
                return v1.GetString() < v2.GetString();
              });
    PrefService* prefs = GetApplicationContext()->GetLocalState();
    prefs->Set(vivaldiprefs::kVivaldiTranslateLanguageList, *json);
    if (GetServer() != kTranslateLanguageListUrl) {
      // Only update last update on the main server
      prefs->SetTime(vivaldiprefs::kVivaldiTranslateLanguageListLastUpdate,
                     base::Time::Now());
    }
    LOG(INFO) << "Downloaded language list from server.";

    SetListInChromium(*list);
  } while (false);

  url_loader_.reset(nullptr);
  StartUpdateTimer();
}

// static
std::string VivaldiIOSTranslateService::GetTargetLanguage(
    PrefService* prefs,
    language::LanguageModel* language_model) {
  return translate::TranslateManager::GetTargetLanguage(
      VivaldiIOSTranslateClient::CreateTranslatePrefs(prefs).get(),
      language_model);
}

// static
bool VivaldiIOSTranslateService::IsTranslatableURL(const GURL& url) {
  // A URL is translatable unless it is one of the following:
  // - empty (can happen for popups created with window.open(""))
  // - an internal URL
  return !url.is_empty() &&
      (!url.SchemeIs(kChromeUIScheme) ||
       !url.SchemeIs(vivaldi::kVivaldiUIScheme));
}
