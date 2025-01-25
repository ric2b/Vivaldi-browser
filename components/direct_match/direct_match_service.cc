// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "direct_match_service.h"

#include "base/barrier_callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "components/datasource/vivaldi_data_url_utils.h"
#include "components/signature/vivaldi_signature.h"

#if !BUILDFLAG(IS_IOS)
#include "chrome/browser/profiles/profile.h"
#include "components/datasource/vivaldi_image_store.h"
#include "content/public/browser/storage_partition.h"
#else
#include "ios/chrome/browser/shared/model/paths/paths.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/common/chrome_paths.h"
#endif

namespace direct_match {

namespace {
const std::string kDirectMatchImageDirectory = "VivaldiDirectMatchIcons";
constexpr float kIncrementConstant = 0.28;
constexpr int kMaxRequestSize = 2 * 1024 * 1024;
constexpr net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    5000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.1,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    1000 * 60 * 5,  // 5 minutes.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

void WriteIconFileThread(base::FilePath image_path,
                         std::unique_ptr<std::string> response_body) {
  if (!base::WriteFile(image_path, *response_body)) {
    LOG(ERROR) << "Failed to write to " << image_path.value() << " "
               << response_body->size()
               << " bytes";
  }
}

void RemoveUnusedIconsThread(const std::vector<base::FilePath>& icons,
                             base::FilePath user_data_dir) {
  base::FileEnumerator enumerator(user_data_dir, true,
                                  base::FileEnumerator::FILES);
  for (base::FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    if (std::find(icons.begin(), icons.end(), file_path) == icons.end()) {
      base::DeleteFile(file_path);
    }
  }
}
}  // namespace

DirectMatchService::DirectMatchService()
    : qwerty_weighted_distance_(QwertyWeightedDistance(kNeighborWeight)),
      report_backoff_(&kBackoffPolicy) {}

DirectMatchService::~DirectMatchService() {}

DirectMatchService::Observer::~Observer() = default;

#if BUILDFLAG(IS_IOS)
void DirectMatchService::Load(
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory) {
  int dir_key = ios::DIR_USER_DATA;
  // Use the determined directory key to get the user data directory.
  if (!base::PathService::Get(dir_key, &user_data_dir_)) {
    user_data_dir_ = base::FilePath();
  }
  user_data_dir_ = user_data_dir_.AppendASCII(kDirectMatchImageDirectory);
  url_loader_factory_ = url_loader_factory;
  base::ThreadPool::PostTask(
       FROM_HERE, {base::MayBlock()},
       base::BindOnce(base::IgnoreResult(&base::CreateDirectory),
                      user_data_dir_));
  DirectMatchService::RunDirectMatchDownload();
}
#else
void DirectMatchService::Load(Profile* profile) {
  if (!profile) {
    return;
  }
  user_data_dir_ = base::FilePath(profile->GetPath().Append(
      vivaldi_image_store::kDirectMatchImageDirectory));
#if BUILDFLAG(IS_ANDROID)
    int dir_key = chrome::DIR_USER_DATA;
    // Use the determined directory key to get the user data directory.
    if (!base::PathService::Get(dir_key, &user_data_dir_)) {
        user_data_dir_ = base::FilePath();
    }
    user_data_dir_ = user_data_dir_.AppendASCII(kDirectMatchImageDirectory);
#endif
  url_loader_factory_ = profile->GetDefaultStoragePartition()
                            ->GetURLLoaderFactoryForBrowserProcess();
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(base::IgnoreResult(&base::CreateDirectory),
                     user_data_dir_));
  DirectMatchService::RunDirectMatchDownload();
}
#endif

void DirectMatchService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DirectMatchService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DirectMatchService::RunDirectMatchDownload() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("vivaldi_direct_match_fetcher",
                                          R"(
        semantics {
          sender: "Vivaldi Direct Match Fetcher"
          description:
            "This request is used to fetch the Direct Match list."
          trigger:
            "This request is triggered when the browser startup and retry every 5 minutes if it failed."
          data:
            "Direct Match item list."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings."
          chrome_policy {
          }
        })");

  GURL url(GetSignedResourceUrl(vivaldi::SignedResourceUrl::kDirectMatchUrl));
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
          base::BindOnce(&DirectMatchService::OnDirectMatchDownloadDone,
                         base::Unretained(this), loader_idx),
          kMaxRequestSize);

  base::VivaldiScopedAllowBlocking allow_blocking;

  if (!base::DirectoryExists(user_data_dir_)) {
    LOG(INFO) << "Creating DM icons directory: " << user_data_dir_.value();
    base::CreateDirectory(user_data_dir_);
  }
}

void DirectMatchService::OnDirectMatchDownloadDone(
    const size_t loader_idx,
    std::unique_ptr<std::string> response_body) {

  if (!response_body || response_body->empty()) {

    report_backoff_.InformOfRequest(false);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DirectMatchService::RunDirectMatchDownload,
                       base::Unretained(this)),
        report_backoff_.GetTimeUntilRelease());
    LOG(WARNING) << "Downloading Direct Match from server "
                 << "failed with error "
                 << simple_url_loader_.at(loader_idx)->NetError();
    return;
  }
  if (!vivaldi::VerifyJsonSignature(*response_body)) {
    LOG(WARNING) << "Direct Match has invalid signature";

    return;
  } else {
    VLOG(1) << "Direct Match signature verified.";
  }
  absl::optional<base::Value> json =
      base::JSONReader::Read(*response_body, base::JSON_ALLOW_TRAILING_COMMAS |
                                                 base::JSON_ALLOW_COMMENTS);
 if (!json) {
    LOG(ERROR) << "Invalid Direct Match list JSON";

    return;
  }
  /*
    This JSON file should be templated as follow:
    {
      "updated_time": string containing the update time,
      "blocks": [
        {
          "id": number,
          "unit": object containing unit data,
        },
      ],
      "country": string containing which country is served,
    }
  */
  base::Value::Dict* dict = json->GetIfDict();
  if (!dict || dict->empty()) {
    return;
  }
  base::Value::List* block_list = dict->FindList("blocks");
  if (!block_list || block_list->empty()) {
    return;
  }
  for (base::Value& block : *block_list) {
    if (!block.is_dict()) {
      continue;
    }
    base::Value::Dict* unit = block.GetDict().FindDict("unit");
    if (!unit || unit->empty()) {
      return;
    }

    DirectMatchService::DirectMatchUnit data(unit);
    direct_match_units_.push_back(std::move(data));
  }
  LOG(INFO) << "Downloaded Direct Match list from server.";

  for (Observer& observer : observers_) {
    observer.OnFinishedDownloadingDirectMatchUnits();
  }

  HandleIcons();
}

void DirectMatchService::HandleIcons() {
  auto barrier = base::BarrierCallback<base::FilePath>(
      direct_match_units_.size(),
      // Unretained is correct here as HandleIcons is only called from
      // OnDirectMatchDownloadDone which is part of DirectMatchService.
      base::BindOnce(&DirectMatchService::RemoveUnusedIcons,
                     base::Unretained(this)));
  for (auto& unit : direct_match_units_) {
    GURL url(unit.image_url);
    std::string extracted_name = url.ExtractFileName();
    base::FilePath image_path =
        user_data_dir_.Append(base::FilePath::FromUTF8Unsafe(extracted_name));
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&base::PathExists, image_path),
        base::BindOnce(&DirectMatchService::DownloadIcon,
                       base::Unretained(this), url, image_path, barrier));
  }
}

void DirectMatchService::DownloadIcon(
    GURL url,
    base::FilePath image_path,
    base::OnceCallback<void(base::FilePath)> callback,
    bool path_exist) {
  // Don't download icon if it's already downloaded and up-to-date.
  if (path_exist) {
    std::move(callback).Run(image_path);
    return;
  }
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("vivaldi_direct_match_fetcher",
                                          R"(
        semantics {
          sender: "Vivaldi Direct Match Fetcher"
          description:
            "This request is used to fetch a Direct Match icon."
          trigger:
            "This request is triggered when the browser startup."
          data:
            "Direct Match item icon."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings."
          chrome_policy {
          }
        })");

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
          base::BindOnce(&DirectMatchService::OnIconDownloadDone,
                         base::Unretained(this), std::move(image_path),
                         loader_idx, std::move(callback)),
          kMaxRequestSize);
}

void DirectMatchService::OnIconDownloadDone(
    base::FilePath image_path,
    size_t loader_idx,
    base::OnceCallback<void(base::FilePath)> callback,
    std::unique_ptr<std::string> response_body) {
  std::move(callback).Run(image_path);
  if (!response_body || response_body->empty()) {
    LOG(WARNING) << "Downloading Direct Match icon from server "
                 << "failed with error "
                 << simple_url_loader_.at(loader_idx)->NetError();
    return;
  }
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&WriteIconFileThread, std::move(image_path),
                     std::move(response_body)));
}

void DirectMatchService::RemoveUnusedIcons(
    const std::vector<base::FilePath>& icons) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RemoveUnusedIconsThread, std::move(icons),
                     user_data_dir_));

  // Notify observers that all icons are available.
  for (Observer& observer : observers_) {
    observer.OnFinishedDownloadingDirectMatchUnitsIcon();
   }
}

/* static */
// Generate chrome:// url which is linked to kDirectMatch directory.
std::string DirectMatchService::GetIconPath(std::string image_url) {
  GURL url(image_url);
  std::string image_name = url.ExtractFileName();
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_ANDROID)
  int dir_key = chrome::DIR_USER_DATA;
#endif
#if BUILDFLAG(IS_IOS)
  int dir_key = ios::DIR_USER_DATA;
#endif
  base::FilePath iconPath;
  if (!base::PathService::Get(dir_key, &iconPath)) {
    iconPath = base::FilePath();
  }
  iconPath = iconPath.AppendASCII(kDirectMatchImageDirectory)
                     .AppendASCII(image_name);
  return iconPath.AsUTF8Unsafe();
#else
  std::string data_url = vivaldi_data_url_utils::MakeUrl(
      vivaldi_data_url_utils::PathType::kDirectMatch, image_name);
  return data_url;
#endif
}

std::vector<const DirectMatchService::DirectMatchUnit*>
DirectMatchService::GetMatchingUnits(
  std::function<bool(const DirectMatchUnit&)> predicate) const {
  std::vector<const DirectMatchService::DirectMatchUnit*> matching_units;
  for (const auto& unit : direct_match_units_) {
    if (predicate(unit)) {
      matching_units.push_back(&unit);
    }
  }
  // Sort the matching units by 'position' in ascending order
  std::sort(matching_units.begin(), matching_units.end(),
            [](const DirectMatchUnit* a, const DirectMatchUnit* b) {
              return a->position < b->position;
            });
  return matching_units;
}

std::pair<DirectMatchService::DirectMatchUnit*, bool>
DirectMatchService::GetDirectMatch(
    std::string query) {
  std::u16string lowercase_query =
      base::i18n::FoldCase(base::UTF8ToUTF16(query));
  DirectMatchService::DirectMatchUnit* candidate = nullptr;
  bool candidate_allowed_to_be_default_match = false;

  for (auto& unit : direct_match_units_) {
    if (!unit.display_locations.address_bar)
      continue;
    bool has_banned_word =
        std::any_of(unit.blocked_names.begin(), unit.blocked_names.end(),
                    [lowercase_query](const base::Value& blocked_name) {
                      return base::i18n::FoldCase(base::UTF8ToUTF16(
                                 blocked_name.GetString())) == lowercase_query;
                    });
    if (has_banned_word) {
      continue;
    }

    auto updateCandidate =
        [&candidate, &unit, &query, &candidate_allowed_to_be_default_match]() {
      bool unit_allowed_to_be_default_match =
          // VAB-10348 fuzzy match can not be default
          base::StartsWith(unit.name,
                          query,
                          base::CompareCase::INSENSITIVE_ASCII) &&
          // VB-111392 Allow match shorter than match_offset
          query.length() >= unit.match_offset;
      if (!candidate || (!candidate_allowed_to_be_default_match &&
            unit_allowed_to_be_default_match) ||
          // Example: typing 'ali' should match on AliExpress with match_offset
          // 3 instead of Alibaba with match_offset 4.
          candidate->match_offset > unit.match_offset) {
        candidate = &unit;
        candidate_allowed_to_be_default_match =
            unit_allowed_to_be_default_match;
      }
    };

    size_t match_len = lowercase_query.size();
    std::u16string name =
        base::i18n::FoldCase(base::UTF8ToUTF16(unit.name)).substr(0, match_len);
    float acceptable_dist = GetAcceptableDirectMatchDistance(name);
    bool match_on_name =
        qwerty_weighted_distance_.QwertyWeightedDamerauLevenshtein(
            name, lowercase_query, false) <= acceptable_dist;
    // Return unit if distance is ok on name.
    if (match_on_name) {
      updateCandidate();
    } else {
      for (base::Value& alternative_name : unit.alternative_names) {
        std::u16string lowercase_alternative_name =
            base::i18n::FoldCase(base::UTF8ToUTF16(alternative_name.GetString()))
                .substr(0, match_len);
        // Return unit if distance is ok on alternative name.
        if (qwerty_weighted_distance_.QwertyWeightedDamerauLevenshtein(
                lowercase_alternative_name, lowercase_query, false) <=
            acceptable_dist) {
          updateCandidate();
        }
      }
    }
  }
  return { candidate, candidate_allowed_to_be_default_match };
}

std::vector<const DirectMatchService::DirectMatchUnit*>
DirectMatchService::GetDirectMatchesForCategory(size_t category_id) const {
  return GetMatchingUnits([category_id](const DirectMatchUnit& unit) {
    return unit.display_locations.sd_dialog && unit.category == category_id;
  });
}

std::vector<const DirectMatchService::DirectMatchUnit*>
DirectMatchService::GetPopularSites() const {

  std::vector<const DirectMatchService::DirectMatchUnit*> matching_units;
  return GetMatchingUnits([](const DirectMatchUnit& unit) {
    return unit.display_locations.sd_dialog;
  });
}

/**
 * This array represent the number of neighbor and mistype character allowed
 * compared to the number of character typed for incrConstant=0.27
 *
 * |   Nb char typed   |   Nb neighbor allowed   |   Nb mistype allowed   |
 * |         0         |             0           |            0           |
 * |         1         |             0           |            0           |
 * |         2         |             0           |            0           |
 * |         3         |             1           |            0           |
 * |         4         |             1           |            1           |
 * |         5         |             1           |            1           |
 * |         6         |             2           |            1           |
 * |         7         |             2           |            1           |
 * |         8         |             2           |            1           |
 * |         9         |             2           |            1           |
 * |         10        |             3           |            2           |
 * |         11        |             3           |            2           |
 * |         12        |             3           |            2           |
 */
float DirectMatchService::GetAcceptableDirectMatchDistance(
    std::u16string name) {
  // Don't allow mistyped char while typed text is considered too small
  const int no_mistype_allowed = name.size() < 3 ? 0 : 1;
  return ((kNeighborWeight + std::floor(name.size() / 2) * kIncrementConstant) *
          no_mistype_allowed);
}

/*
 * DirectMatch Unit
 */

DirectMatchService::DirectMatchUnit::DirectMatchUnit(base::Value::Dict* unit) {
  for (auto [key, value] : *unit) {
    if (key == "name") {
      name = std::move(value.GetString());
    } else if (key == "title") {
      title = std::move(value.GetString());
    } else if (key == "redirect_url") {
      redirect_url = std::move(value.GetString());
    } else if (key == "image_url") {
      image_url = std::move(value.GetString());
      image_path = GetIconPath(image_url);
    } else if (key == "match_offset") {
      match_offset = static_cast<size_t>(value.GetInt());
    } else if (key == "alternative_names") {
      alternative_names = std::move(value.GetList());
    } else if (key == "blocked_names") {
      blocked_names = std::move(value.GetList());
    } else if (key == "category") {
      category = static_cast<size_t>(value.GetInt());
    }  else if (key == "position") {
      position = static_cast<size_t>(value.GetInt());
    } else if (key == "display_locations") {
      if (auto* display_dict = value.GetIfDict()) {
        if (auto address_bar_value = display_dict->FindBool("address_bar")) {
          display_locations.address_bar = *address_bar_value;
        }

        if (auto sd_dialog_value = display_dict->FindBool("sd_dialog")) {
          display_locations.sd_dialog = *sd_dialog_value;
        }
      }
    }
  }
}

DirectMatchService::DirectMatchUnit::DirectMatchUnit(
    DirectMatchService::DirectMatchUnit&& unit) = default;

DirectMatchService::DirectMatchUnit&
DirectMatchService::DirectMatchUnit::operator=(
    DirectMatchService::DirectMatchUnit&& unit) = default;

DirectMatchService::DirectMatchUnit::~DirectMatchUnit() = default;

}  // namespace direct_match
