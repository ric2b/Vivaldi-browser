// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "vivaldi_search_engines_updater.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/search_engines/search_engines_manager.h"

#include "net/base/load_flags.h"
#include "prefs/vivaldi_pref_names.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"

#if !BUILDFLAG(IS_IOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#else
#include "services/network/public/cpp/shared_url_loader_factory.h"
#endif

#include "components/signature/vivaldi_signature.h"

namespace vivaldi {

namespace {
constexpr int kSearchEnginesResponse = 1024 * 1024;
}  // namespace


void SearchEnginesUpdater::Update(scoped_refptr<network::SharedURLLoaderFactory>
                                  url_loader_factory) {
  LOG(INFO) << "Search engines updater started.";
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      GURL(GetSignedResourceUrl(SignedResourceUrl::kSearchEnginesUrl));
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // See
  // https://chromium.googlesource.com/chromium/src/+/lkgr/docs/network_traffic_annotations.md
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("vivaldi_search_engines_request", R"(
        semantics {
          sender: "Vivaldi request for the list of the default search engines"
          description: "The search engines description."
          trigger: "On startup"
          data: "JSON"
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled."
        }
      })");

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  url_loader->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  url_loader->SetAllowHttpErrorResults(false);

  url_loader->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&SearchEnginesUpdater::OnRequestResponse,
                     std::move(url_loader)),
      kSearchEnginesResponse);
}

void SearchEnginesUpdater::OnRequestResponse(
    std::unique_ptr<network::SimpleURLLoader> guard,
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    LOG(WARNING) << "Unable to download search_engines.json";
    return;
  }
  auto download_path =
      SearchEnginesManager::GetInstance()->GetSearchEnginesJsonUpdatePath();
  if (!download_path) {
    LOG(ERROR)
        << "search_engines.json downloaded, but don't know where to save the "
           "content.";
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          [](base::FilePath path, std::unique_ptr<std::string> data) {
            if (!VerifyJsonSignature(*data)) {
              LOG(WARNING) << "The downloaded search_engines.json has invalid signature.";
              return;
            }
            if (base::WriteFile(path, *data)) {
              VLOG(1) << path << " downloaded and saved, signature verified.";
            } else {
              LOG(WARNING) << "Failed to store " << path;
            }
          },
          *download_path, std::move(response_body)));
}

}  // namespace vivaldi
