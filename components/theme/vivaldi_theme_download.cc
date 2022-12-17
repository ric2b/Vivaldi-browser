// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "components/theme/vivaldi_theme_download.h"

#include "base/files/file_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"

#include "components/datasource/vivaldi_theme_io.h"

namespace vivaldi {

VivaldiThemeDownloadHelper::VivaldiThemeDownloadHelper(
    std::string theme_id,
    GURL url,
    ThemeDownloadCallback callback,
    base::WeakPtr<Profile> profile)
    : url_(std::move(url)),
      theme_id_(std::move(theme_id)),
      callback_(std::move(callback)),
      profile_(std::move(profile)) {}

VivaldiThemeDownloadHelper::~VivaldiThemeDownloadHelper() {}

void VivaldiThemeDownloadHelper::DownloadAndInstall() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_BYPASS_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // See
  // https://chromium.googlesource.com/chromium/src/+/lkgr/docs/network_traffic_annotations.md
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("vivaldi_theme_download", R"(
        semantics {
          sender: "Vivaldi Theme Download"
          description: "Download and install a theme from the theme server."
          trigger: "Triggered by user when clicking install button on theme page."
          data: "Binary zip file containing all the files for the theme."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled."
        }
      })");

  // At this point the context must be alive.
  DCHECK(profile_);

  auto url_loader_factory = profile_->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  url_loader_->SetOnDownloadProgressCallback(
      base::BindRepeating(&VivaldiThemeDownloadHelper::OnDownloadProgress,
                          weak_factory_.GetWeakPtr()));

  if (delegate_) {
    delegate_->DownloadStarted(theme_id_);
  }

  url_loader_->DownloadToTempFile(
      url_loader_factory.get(),
      base::BindOnce(&VivaldiThemeDownloadHelper::OnDownloadCompleted,
                     weak_factory_.GetWeakPtr()),
      vivaldi_theme_io::kMaxArchiveSize);
}

void VivaldiThemeDownloadHelper::OnDownloadCompleted(base::FilePath path) {
  temporary_file_ = std::move(path);

  vivaldi_theme_io::Import(
      profile_, temporary_file_, std::vector<uint8_t>(),
      base::BindOnce(&VivaldiThemeDownloadHelper::SendResult,
                     weak_factory_.GetWeakPtr()));
}

void VivaldiThemeDownloadHelper::OnDownloadProgress(uint64_t current) {
  if (delegate_) {
    delegate_->DownloadProgress(theme_id_, current);
  }
}

namespace {
scoped_refptr<base::SequencedTaskRunner> GetOneShotFileTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::BEST_EFFORT});
}
}  // namespace

void VivaldiThemeDownloadHelper::SendResult(
    std::string theme_id,
    std::unique_ptr<vivaldi_theme_io::ImportError> error) {
  if (!error && theme_id != theme_id_) {
    error = std::make_unique<vivaldi_theme_io::ImportError>(
        vivaldi_theme_io::ImportError::kBadSettings,
        "theme ids from download and settings mismatch");
    LOG(ERROR) << error->details;
  }
  if (delegate_) {
    if (error) {
      delegate_->DownloadCompleted(theme_id_, false, error->details);
    } else {
      delegate_->DownloadCompleted(theme_id_, true, std::string());
    }
  }
  GetOneShotFileTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::GetDeleteFileCallback(temporary_file_)));

  // `this` can be deleted after the callback call.
  std::move(callback_).Run(theme_id, std::move(error));
}

}  // namespace vivaldi
