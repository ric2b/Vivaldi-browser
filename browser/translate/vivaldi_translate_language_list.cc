// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "browser/translate/vivaldi_translate_language_list.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/vivaldi_switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
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

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile_manager.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#endif

namespace translate {

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

VivaldiTranslateLanguageList::VivaldiTranslateLanguageList()
    : resource_request_allowed_notifier_(
          g_browser_process->local_state(),
          switches::kDisableBackgroundNetworking,
          base::BindOnce(&content::GetNetworkConnectionTracker)),
      weak_factory_(this) {
  // "Set |leaky| to true if this class will not be destructed before shutdown."
  // This is an object created in main-extra-parts and not a singleton service.
  // Must not be leaky. VB-98009.
  resource_request_allowed_notifier_.Init(this, false /* leaky */);
  // See if we can kick it off right now.
  OnResourceRequestsAllowed();

  // Set ours prefs list as default in Chromium.
  SetPrefsListAsDefault();
}

VivaldiTranslateLanguageList::~VivaldiTranslateLanguageList() = default;

void VivaldiTranslateLanguageList::StartUpdateTimer() {
  // Keep a timer firing every hour to check if we need to download
  // an updated list again.
  if (!update_timer_.IsRunning()) {
    update_timer_.Start(
        FROM_HERE, base::Hours(kCheckIntervalHours),
        base::BindOnce(&VivaldiTranslateLanguageList::StartDownload,
                       weak_factory_.GetWeakPtr()));
  }
}

void VivaldiTranslateLanguageList::SetPrefsListAsDefault() {
  PrefService* prefs = g_browser_process->local_state();
  auto& list = prefs->GetList(vivaldiprefs::kVivaldiTranslateLanguageList);
  SetListInChromium(list);
}

void VivaldiTranslateLanguageList::SetListInChromium(
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

void VivaldiTranslateLanguageList::OnResourceRequestsAllowed() {
  bool can_update =
      resource_request_allowed_notifier_.ResourceRequestsAllowed();
  if (can_update && can_update_ != can_update) {
    can_update_ = can_update;
    StartDownload();
  }
}

bool VivaldiTranslateLanguageList::ShouldUpdate() {
  if (!can_update_) {
    return false;
  }
  if (GetServer() != kTranslateLanguageListUrl) {
    // Always update when using a custom url
    return true;
  }
  PrefService* prefs = g_browser_process->local_state();
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
const std::string VivaldiTranslateLanguageList::GetServer() {
  std::string server = kTranslateLanguageListUrl;
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  if (cmd_line.HasSwitch(switches::kTranslateLanguageListUrl)) {
    server = cmd_line.GetSwitchValueASCII(switches::kTranslateLanguageListUrl);
  }
  return server;
}

void VivaldiTranslateLanguageList::StartDownload() {
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

#if BUILDFLAG(IS_ANDROID)
  Profile* profile = ProfileManager::GetLastUsedProfile();
#else
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    // VB-88607 [macOS] Browser crashes randomly
    // If there is no window open GetDefaultStoragePartition will crash on macOS
    // because the profile is destroyed when all windows are closed.
    StartUpdateTimer();
    return;
  }
  Profile* profile = browser->profile();
#endif

  auto url_loader_factory = profile->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  url_loader_->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&VivaldiTranslateLanguageList::OnListDownloaded,
                     weak_factory_.GetWeakPtr()),
      kMaxListSize);
}

void VivaldiTranslateLanguageList::OnListDownloaded(
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
    PrefService* prefs = g_browser_process->local_state();
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

}  // namespace translate
