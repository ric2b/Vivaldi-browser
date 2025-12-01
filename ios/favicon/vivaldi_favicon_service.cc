// Copyright 2025 Vivaldi Technologies. All rights reserved.

#include "ios/favicon/vivaldi_favicon_service.h"

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_util.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "ios/chrome/browser/favicon/model/large_icon_cache.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

namespace {

// Common favicon paths to try
const char* kFaviconPaths[] = {"/favicon.ico", "/favicon.png",
                               "/apple-touch-icon.png",
                               "/apple-touch-icon-precomposed.png"};

// Check if a URL appears to be a favicon URL based on path indicators.
bool IsFaviconUrl(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }

  std::string path = url.path();

  // Check if the path contains common favicon indicators
  return path.find("favicon") != std::string::npos ||
         path.find("apple-touch-icon") != std::string::npos ||
         path.find(".ico") != std::string::npos ||
         path.find("icon") != std::string::npos;
}

// Create a default LargeIconResult for fallback cases using Chromium's
// defaults.
favicon_base::LargeIconResult CreateDefaultLargeIconResult() {
  auto fallback_style = std::make_unique<favicon_base::FallbackIconStyle>();
  fallback_style->background_color = SK_ColorGRAY;
  fallback_style->text_color = SK_ColorWHITE;
  fallback_style->is_default_background_color = true;
  return favicon_base::LargeIconResult(std::move(fallback_style));
}

// Helper method to validate URL and callback
bool ValidateRequest(const GURL& url,
                     favicon_base::LargeIconCallback& callback) {
  if (url.is_empty() || !url.is_valid() || callback.is_null()) {
    if (!callback.is_null()) {
      std::move(callback).Run(CreateDefaultLargeIconResult());
    }
    return false;
  }
  return true;
}

}  // namespace

namespace ios {

VivaldiFaviconService::VivaldiFaviconService(
    favicon::FaviconService* favicon_service,
    favicon::LargeIconService* large_icon_service,
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher)
    : favicon_service_(favicon_service),
      large_icon_service_(large_icon_service),
      image_fetcher_(std::move(image_fetcher)) {
  CHECK(favicon_service_);
  CHECK(large_icon_service_);
  CHECK(image_fetcher_);
}

VivaldiFaviconService::~VivaldiFaviconService() {
  CancelAllRequests();
}

// Generic helper for handling favicon requests with different options
template <typename CacheMethod, typename DownloadMethod>
void VivaldiFaviconService::RequestFaviconWithOptions(
    const GURL& url,
    VivaldiFaviconOptions options,
    VivaldiFaviconCallback callback,
    CacheMethod cache_method,
    DownloadMethod download_method) {
  if (!ValidateRequest(url, callback)) {
    return;
  }

  switch (options) {
    case VivaldiFaviconOptions::kCacheOnly:
      cache_method(std::move(callback));
      break;

    case VivaldiFaviconOptions::kCacheFirstThenFresh: {
      auto cache_callback = base::BindOnce(
          [](base::WeakPtr<VivaldiFaviconService> weak_service,
             DownloadMethod download_method,
             VivaldiFaviconCallback final_callback,
             const favicon_base::LargeIconResult& cached_result) {
            if (!weak_service) {
              std::move(final_callback).Run(CreateDefaultLargeIconResult());
              return;
            }

            if (cached_result.bitmap.is_valid()) {
              // Cache hit with a real favicon
              std::move(final_callback).Run(std::move(cached_result));
            } else {
              // No real favicon in cache, download fresh
              download_method(std::move(final_callback));
            }
          },
          weak_ptr_factory_.GetWeakPtr(), download_method, std::move(callback));
      cache_method(std::move(cache_callback));
      break;
    }

    case VivaldiFaviconOptions::kAlwaysFresh:
      download_method(std::move(callback));
      break;
  }
}

void VivaldiFaviconService::FaviconForPageUrl(const GURL& page_url,
                                              int size_in_pixels,
                                              int min_size_in_pixels,
                                              VivaldiFaviconOptions options,
                                              VivaldiFaviconCallback callback) {
  auto cache_method = [this, page_url, size_in_pixels,
                       min_size_in_pixels](VivaldiFaviconCallback cb) {
    GetFaviconFromCache(page_url, size_in_pixels, min_size_in_pixels,
                        std::move(cb));
  };

  auto download_method = [this, page_url](VivaldiFaviconCallback cb) {
    DownloadFreshFavicon(page_url, std::move(cb));
  };

  RequestFaviconWithOptions(page_url, options, std::move(callback),
                            cache_method, download_method);
}

void VivaldiFaviconService::GetFaviconFromCache(
    const GURL& page_url,
    int size_in_pixels,
    int min_size_in_pixels,
    VivaldiFaviconCallback callback) {
  if (!ValidateRequest(page_url, callback)) {
    return;
  }

  large_icon_service_->GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      page_url, min_size_in_pixels, size_in_pixels, std::move(callback),
      &task_tracker_);
}

void VivaldiFaviconService::CancelAllRequests() {
  task_tracker_.TryCancelAll();
}

void VivaldiFaviconService::GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
    const GURL& page_url,
    int min_size_in_pixels,
    int size_in_pixels,
    VivaldiFaviconCallback callback) {
  if (!ValidateRequest(page_url, callback)) {
    return;
  }

  large_icon_service_->GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      page_url, min_size_in_pixels, size_in_pixels, std::move(callback),
      &task_tracker_);
}

void VivaldiFaviconService::GetIconRawBitmapOrFallbackStyleForPageUrl(
    const GURL& page_url,
    int size_in_pixels,
    VivaldiFaviconCallback callback) {
  if (!ValidateRequest(page_url, callback)) {
    return;
  }

  large_icon_service_->GetIconRawBitmapOrFallbackStyleForPageUrl(
      page_url, size_in_pixels, std::move(callback), &task_tracker_);
}

void VivaldiFaviconService::GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
    const GURL& icon_url,
    int min_size_in_pixels,
    int size_in_pixels,
    VivaldiFaviconCallback callback) {
  if (!ValidateRequest(icon_url, callback)) {
    return;
  }

  large_icon_service_->GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
      icon_url, min_size_in_pixels, size_in_pixels, std::move(callback),
      &task_tracker_);
}

void VivaldiFaviconService::FaviconForIconUrl(const GURL& icon_url,
                                              int size_in_pixels,
                                              int min_size_in_pixels,
                                              VivaldiFaviconOptions options,
                                              VivaldiFaviconCallback callback) {
  auto cache_method = [this, icon_url, size_in_pixels,
                       min_size_in_pixels](VivaldiFaviconCallback cb) {
    GetFaviconFromCacheForIconUrl(icon_url, size_in_pixels, min_size_in_pixels,
                                  std::move(cb));
  };

  auto download_method = [this, icon_url](VivaldiFaviconCallback cb) {
    DownloadFreshFaviconFromIconUrl(icon_url, std::move(cb));
  };

  RequestFaviconWithOptions(icon_url, options, std::move(callback),
                            cache_method, download_method);
}

void VivaldiFaviconService::FaviconForPageUrlOrHost(
    const GURL& page_url,
    int size_in_pixels,
    VivaldiFaviconOptions options,
    VivaldiFaviconCallback callback) {
  auto cache_method = [this, page_url,
                       size_in_pixels](VivaldiFaviconCallback cb) {
    GetFaviconFromCacheWithHostFallback(page_url, size_in_pixels,
                                        std::move(cb));
  };

  auto download_method = [this, page_url](VivaldiFaviconCallback cb) {
    DownloadFreshFavicon(page_url, std::move(cb));
  };

  RequestFaviconWithOptions(page_url, options, std::move(callback),
                            cache_method, download_method);
}

void VivaldiFaviconService::OnFaviconDownloaded(
    const GURL& page_url,
    const GURL& icon_url,
    VivaldiFaviconCallback callback,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  if (image.IsEmpty()) {
    std::move(callback).Run(CreateDefaultLargeIconResult());
    return;
  }

  // Save to Chromium's favicon cache for future use
  if (favicon_service_) {
    favicon_service_->SetFavicons({page_url}, icon_url,
                                  favicon_base::IconType::kFavicon, image);
  }

  // Create LargeIconResult with the validated image
  favicon_base::FaviconRawBitmapResult bitmap_result;
  bitmap_result.bitmap_data = image.As1xPNGBytes();
  bitmap_result.icon_url = icon_url;

  favicon_base::LargeIconResult result(bitmap_result);
  std::move(callback).Run(std::move(result));
}

void VivaldiFaviconService::TryDownloadFaviconFromUrls(
    const GURL& page_url,
    std::vector<GURL> favicon_urls,
    VivaldiFaviconCallback callback) {
  if (favicon_urls.empty()) {
    // All URLs failed, return default
    std::move(callback).Run(CreateDefaultLargeIconResult());
    return;
  }

  // Get the last URL and remove it from the vector
  GURL icon_url = std::move(favicon_urls.back());
  favicon_urls.pop_back();

  // Use the standard pattern for image fetching
  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("vivaldi_favicon_fetcher", R"(
        semantics {
          sender: "Vivaldi Favicon Service"
          description:
            "Fetches favicon images for websites displayed in the Vivaldi browser."
          trigger:
            "When favicon is requested for a website URL."
          data: "URL of the favicon image to fetch."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled."
        }
      )");

  image_fetcher_->FetchImage(
      icon_url,
      base::BindOnce(
          [](base::WeakPtr<VivaldiFaviconService> weak_service,
             const GURL& page_url, std::vector<GURL> remaining_urls,
             const GURL& icon_url,
             VivaldiFaviconCallback callback, const gfx::Image& image,
             const image_fetcher::RequestMetadata& metadata) {
            if (!weak_service) {
              if (!callback.is_null()) {
                std::move(callback).Run(CreateDefaultLargeIconResult());
              }
              return;
            }

            // Check if download was successful
            if (image.IsEmpty() || metadata.http_response_code < 200 ||
                metadata.http_response_code >= 300) {
              // Try next URL with remaining URLs
              weak_service->TryDownloadFaviconFromUrls(
                  page_url, std::move(remaining_urls), std::move(callback));
              return;
            }

            // Success! Process the downloaded favicon
            weak_service->OnFaviconDownloaded(
                page_url, icon_url,
                std::move(callback), image, metadata);
          },
          weak_ptr_factory_.GetWeakPtr(), page_url, std::move(favicon_urls),
          icon_url, std::move(callback)),
      image_fetcher::ImageFetcherParams(traffic_annotation,
                                        "VivaldiFaviconService"));
}

void VivaldiFaviconService::DownloadFreshFaviconFromIconUrl(
    const GURL& icon_url,
    VivaldiFaviconCallback callback) {
  // Use Chromium's URL validation pattern
  if (icon_url.is_empty() || !icon_url.is_valid()) {
    std::move(callback).Run(CreateDefaultLargeIconResult());
    return;
  }

  // For icon URLs, use the URL directly without generating fallbacks
  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("vivaldi_favicon_fetcher", R"(
        semantics {
          sender: "Vivaldi Favicon Service"
          description:
            "Fetches favicon images for websites displayed in the Vivaldi browser."
          trigger:
            "When favicon is requested for a specific icon URL."
          data: "URL of the favicon image to fetch."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled."
        }
      )");

  image_fetcher_->FetchImage(
      icon_url,
      base::BindOnce(
          [](base::WeakPtr<VivaldiFaviconService> weak_service,
             const GURL& icon_url,
             VivaldiFaviconCallback callback, const gfx::Image& image,
             const image_fetcher::RequestMetadata& metadata) {
            if (!weak_service) {
              if (!callback.is_null()) {
                std::move(callback).Run(CreateDefaultLargeIconResult());
              }
              return;
            }

            // Check if download was successful
            if (image.IsEmpty() || metadata.http_response_code < 200 ||
                metadata.http_response_code >= 300) {
              // Failed to download from icon URL, return default
              std::move(callback).Run(CreateDefaultLargeIconResult());
              return;
            }

            // Success! Process the downloaded favicon
            weak_service->OnFaviconDownloaded(
                icon_url, icon_url,
                std::move(callback), image, metadata);
          },
          weak_ptr_factory_.GetWeakPtr(), icon_url, std::move(callback)),
      image_fetcher::ImageFetcherParams(traffic_annotation,
                                        "VivaldiFaviconService"));
}

void VivaldiFaviconService::DownloadFreshFavicon(
    const GURL& page_url,
    VivaldiFaviconCallback callback) {
  if (page_url.is_empty() || !page_url.is_valid() ||
      !page_url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(CreateDefaultLargeIconResult());
    return;
  }

  GURL host_url = page_url.GetWithEmptyPath();
  if (host_url.is_empty() || !host_url.is_valid()) {
    std::move(callback).Run(CreateDefaultLargeIconResult());
    return;
  }

  std::vector<GURL> favicon_urls;

  if (IsFaviconUrl(page_url)) {
    favicon_urls.push_back(page_url);
  }

  for (const char* path : kFaviconPaths) {
    GURL favicon_url =
        host_url.Resolve(path);  // Resolving a valid url results in a valid URL
    if (favicon_url != page_url) {
      favicon_urls.push_back(favicon_url);
    }
  }

  CHECK(!favicon_urls.empty());

  // Reverse the vector so we can efficiently remove from the back
  std::reverse(favicon_urls.begin(), favicon_urls.end());

  // Try downloading from URLs, removing them as we go
  TryDownloadFaviconFromUrls(page_url, std::move(favicon_urls), std::move(callback));
}

void VivaldiFaviconService::GetFaviconFromCacheWithHostFallback(
    const GURL& page_url,
    int size_in_pixels,
    VivaldiFaviconCallback callback) {
  if (!ValidateRequest(page_url, callback)) {
    return;
  }

  large_icon_service_->GetIconRawBitmapOrFallbackStyleForPageUrl(
      page_url, size_in_pixels, std::move(callback), &task_tracker_);
}

void VivaldiFaviconService::GetFaviconFromCacheForIconUrl(
    const GURL& icon_url,
    int size_in_pixels,
    int min_size_in_pixels,
    VivaldiFaviconCallback callback) {
  if (!ValidateRequest(icon_url, callback)) {
    return;
  }

  large_icon_service_->GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
      icon_url, min_size_in_pixels, size_in_pixels, std::move(callback),
      &task_tracker_);
}

}  // namespace ios
