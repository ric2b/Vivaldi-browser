// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "vivaldi_status/vivaldi_status.h"

#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if !BUILDFLAG(IS_IOS)
#include "content/public/browser/storage_partition.h"
#endif

namespace vivaldi_status {

#if 0
void Dump(VivaldiStatus::IdToHealthMap map) {
  base::Time now = base::Time::Now();
  int64_t n = now.InMillisecondsSinceUnixEpoch() / 1000;
  printf("now %ld\n", n);
  for (VivaldiStatus::IdToHealthMap::iterator it = map.begin(); it != map.end();
       ++it) {
    printf("%s %d %d\n", it->first.c_str(), it->second.id, it->second.mode);
  }
}
#endif

constexpr int kMaxRequestSize = 1024 * 10;
// For how long downloaded data is valid (seconds).
constexpr int kCacheInterval = 60 * 10;
// Limiter to prevent a swarm of requests (seconds).
constexpr int kAttemptInterval = 60;
// Request url. We will append paramters to this url.
constexpr const char* kRequestUrl =
    "https://vivaldistatus.com/api/services-compact";

VivaldiStatus::VivaldiStatus() {
  Health item;
  item.mode = kUnknown;
  for (int i = kServiceMin; i <= kServiceMax; i++) {
    item.id = static_cast<Services>(i);
    id_to_health_map_[ServiceToId(item.id)] = item;
  }
}

#if BUILDFLAG(IS_IOS)
void VivaldiStatus::Init(
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}
#else
void VivaldiStatus::Init(content::BrowserContext* context,
                         bool report_all_changes) {
  url_loader_factory_ = context->GetDefaultStoragePartition()
                            ->GetURLLoaderFactoryForBrowserProcess();
  report_all_changes_ = report_all_changes;
}
#endif

bool VivaldiStatus::IsValid() {
  int64_t now = base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000;
  int64_t last = last_sucessful_update_.InMillisecondsSinceUnixEpoch() / 1000;
  return now >= last && now <= last + kCacheInterval;
}

bool VivaldiStatus::GetMode(Services service, Mode* mode) {
  if (!IsValid()) {
    return false;
  }

  auto it = id_to_health_map_.find(ServiceToId(service));
  if (it == id_to_health_map_.end()) {
    LOG(ERROR) << "Vivaldi status: Unknown service";
    return false;
  }
  *mode = it->second.mode;
  return true;
}

bool VivaldiStatus::Refresh(Services service) {
  if (IsValid()) {
    return false;
  }

  request_map_[ServiceToId(service)] = true;

  if (is_updating_ || timer_.IsRunning()) {
    return true;
  }

  int64_t now = base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000;
  int64_t last = last_attempted_update_.InMillisecondsSinceUnixEpoch() / 1000;
  if (now >= last + kAttemptInterval) {
    Download();
  } else {
    int s = kAttemptInterval - (now - last);
    // Some sanity tests for all sorts of time change / errors. Does not matter
    // if we download a bit later than normal for these rare situations.
    if (s < 0 || s > kAttemptInterval) {
      s = kAttemptInterval;
    }
    timer_.Start(FROM_HERE, base::Seconds(s), this, &VivaldiStatus::Download);
  }
  return true;
}

void VivaldiStatus::Download() {
  timer_.Stop();
  if (is_updating_) {
    return;
  }
  is_updating_ = true;
  last_attempted_update_ = base::Time::Now();

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("vivaldi_service_status_fetcher",
                                          R"(
        semantics {
          sender: "Vivaldi Service Status Fetcher"
          description:
            "This request is used to fetch Vivaldi Service Status."
          trigger:
            "This request is triggered when client requests connectivity and server status."
          data:
            "Service Status list."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings."
          chrome_policy {
          }
        })");

  std::string parameter;
  for (IdToBoolMap::iterator it = request_map_.begin();
      it != request_map_.end(); ++it) {
    std::string entry =(parameter.empty()
        ? std::string("?s=") : std::string("&s=")) + it->first;
    parameter += entry;
  }
  request_map_.clear();

  GURL url(kRequestUrl + parameter);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_BYPASS_CACHE;

  size_t loader_idx = simple_url_loader_.size();
  simple_url_loader_.push_back(network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation));
  simple_url_loader_.at(loader_idx)
      ->DownloadToString(
          url_loader_factory_.get(),
          base::BindOnce(&VivaldiStatus::OnDownloadDone,
                         weak_factory_.GetWeakPtr(), loader_idx),
          kMaxRequestSize);
}

void VivaldiStatus::OnDownloadDone(const size_t loader_idx,
    std::unique_ptr<std::string> response_body) {
  is_updating_ = false;
  IdToHealthMap old_map(id_to_health_map_);
  if (Parse(std::move(response_body))) {
    last_sucessful_update_ = base::Time::Now();
    // Parse will not add new keys to id_to_health_map_ so using [] is safe
    // Make a list of service whose value has changed.
    std::vector<Health> changes;
    for (IdToHealthMap::iterator it = id_to_health_map_.begin();
         it != id_to_health_map_.end(); ++it) {
      if (report_all_changes_ || (it->second != old_map[it->first])) {
        changes.push_back(it->second);
      }
    }
    if (changes.size()) {
      for (Observer& observer : observers_) {
        observer.OnVivaldiStatusUpdated(this, changes);
      }
      // Special handling for sync
      for (size_t i=0; i < changes.size(); i++) {
        if (changes[i].id == kSync) {
          for (Observer& observer : observers_) {
            observer.OnVivaldiSyncStatusUpdated(changes[i].mode);
          }
        }
      }
    }
  } else {
    for (Observer& observer : observers_) {
      observer.OnVivaldiStatusError(this);
    }
  }
}

bool VivaldiStatus::Parse(std::unique_ptr<std::string> response_body) {
  if (!response_body || response_body->empty()) {
    LOG(ERROR) << "Vivaldi status: No data";
    return false;
  }

  // For other json downloads we do we have a signature test. This is not viable
  // for this file with its dynamic content (burden on the signer).

  std::optional<base::Value> json =
      base::JSONReader::Read(*response_body, base::JSON_ALLOW_TRAILING_COMMAS |
                                             base::JSON_ALLOW_COMMENTS);
  if (!json) {
    LOG(ERROR) << "Vivaldi status: Invalid JSON";
    return false;
  }

  base::Value::Dict* dict = json->GetIfDict();
  if (!dict || dict->empty()) {
    LOG(ERROR) << "Vivaldi status: Invalid JSON. Empty dict";
    return false;
  }
  for (auto entry = dict->begin(); entry != dict->end(); ++entry) {
    const std::string& id = entry->first;
    std::optional<int> mode = entry->second.GetIfInt();
    if (!mode.has_value()) {
      LOG(ERROR) << "Vivaldi status: Invalid JSON. Incorrect value";
      return false;
    }
    auto it = id_to_health_map_.find(id);
    if (it != id_to_health_map_.end()) {
      Health health;
      health.mode = kUnknown;
      // This id entry is kept as is.
      health.id = it->second.id;
      switch (mode.value()) {
        case 1:
          health.mode = kOperational;
          break;
        case 2:
          health.mode = kMaintenance;
          break;
        case 3:
          health.mode = kMinorOutage;
          break;
        case 4:
          health.mode = kMajorOutage;
          break;
        default:
           LOG(WARNING) << "Vivaldi status: Invalid JSON. Unknown Status mode";
           break;
      }
      id_to_health_map_[id] = health;
    } else {
      LOG(WARNING) << "Vivaldi status: Invalid JSON. Unknown id";
    }
  }

  return true;
}

std::string VivaldiStatus::ServiceToId(Services service) {
  return std::to_string(service);
}

bool VivaldiStatus::IdToService(std::string id, Services* service) {
  for (auto ch: id) {
    if (!std::isdigit(ch)) {
      return false;
    }
  }
  int val = std::stoi(id);
  if (val >= kServiceMin && val <= kServiceMax) {
    *service = static_cast<Services>(val);
    return true;
  }
  return false;
}

void VivaldiStatus::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VivaldiStatus::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // vivaldi_status
