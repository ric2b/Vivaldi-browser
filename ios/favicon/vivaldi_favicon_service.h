// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_FAVICON_VIVALDI_FAVICON_SERVICE_H_
#define IOS_FAVICON_VIVALDI_FAVICON_SERVICE_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace favicon {
class FaviconService;
class LargeIconService;
}  // namespace favicon

namespace image_fetcher {
class ImageFetcher;
struct RequestMetadata;
}  // namespace image_fetcher

namespace ios {

// Options for favicon fetching behavior.
enum class VivaldiFaviconOptions {
  kAlwaysFresh,          // Always download fresh from origin
  kCacheFirstThenFresh,  // Check cache first, download fresh if not found
  kCacheOnly             // Use existing cache-only behavior
};

// Use Chromium's favicon data structures directly instead of custom types.
// This eliminates the need for custom conversion logic.
using VivaldiFaviconCallback = favicon_base::LargeIconCallback;

// Core C++ service for managing favicon downloads and caching.
// Uses Chromium's LargeIconService for consistent behavior with the platform.
//
// This service provides favicon handling with three modes:
// - kCacheOnly: Returns cached favicon or default if not found
// - kAlwaysFresh: Always downloads fresh favicon from network
// - kCacheFirstThenFresh: Checks cache first, downloads fresh if not found
//
// The service now uses Chromium's favicon_base::LargeIconResult directly,
// eliminating the need for custom data structures and conversion logic.
//
// IMPORTANT: All size parameters are in pixels, not points. Platform-specific
// loaders (like FaviconLoader) should handle points-to-pixels conversion
// before calling this service, following Chromium's architecture pattern.
class VivaldiFaviconService final : public KeyedService {
 public:
  VivaldiFaviconService(
      favicon::FaviconService* favicon_service,
      favicon::LargeIconService* large_icon_service,
      std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher);

  VivaldiFaviconService(const VivaldiFaviconService&) = delete;
  VivaldiFaviconService& operator=(const VivaldiFaviconService&) = delete;

  ~VivaldiFaviconService() override;

  // Requests a favicon with specified caching behavior.
  // For cached icons: min_size_in_pixels is used by Chromium's LargeIconService
  // For downloaded icons: OnFaviconDownloaded function handles the validation.
  void FaviconForPageUrl(const GURL& page_url,
                         int size_in_pixels,
                         int min_size_in_pixels,
                         VivaldiFaviconOptions options,
                         VivaldiFaviconCallback callback);

  // Requests a favicon directly from an icon URL.
  // For cached icons: min_size_in_pixels is used by Chromium's LargeIconService
  // For downloaded icons: OnFaviconDownloaded function handles the validation.
  void FaviconForIconUrl(const GURL& icon_url,
                         int size_in_pixels,
                         int min_size_in_pixels,
                         VivaldiFaviconOptions options,
                         VivaldiFaviconCallback callback);

  // Requests a favicon for a page URL with host fallback.
  void FaviconForPageUrlOrHost(const GURL& page_url,
                               int size_in_pixels,
                               VivaldiFaviconOptions options,
                               VivaldiFaviconCallback callback);

  // Cancels all pending requests.
  void CancelAllRequests();

  // Direct Chromium LargeIconService wrappers - these call the exact same
  // methods as Chromium's FaviconLoader for complete compatibility.
  void GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      const GURL& page_url,
      int min_size_in_pixels,
      int size_in_pixels,
      VivaldiFaviconCallback callback);

  void GetIconRawBitmapOrFallbackStyleForPageUrl(
      const GURL& page_url,
      int size_in_pixels,
      VivaldiFaviconCallback callback);

  void GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
      const GURL& icon_url,
      int min_size_in_pixels,
      int size_in_pixels,
      VivaldiFaviconCallback callback);

 private:
  // Generic helper for handling favicon requests with different options
  template <typename CacheMethod, typename DownloadMethod>
  void RequestFaviconWithOptions(const GURL& url,
                                 VivaldiFaviconOptions options,
                                 VivaldiFaviconCallback callback,
                                 CacheMethod cache_method,
                                 DownloadMethod download_method);

  // Cache access methods.
  void GetFaviconFromCache(const GURL& page_url,
                           int size_in_pixels,
                           int min_size_in_pixels,
                           VivaldiFaviconCallback callback);

  void GetFaviconFromCacheForIconUrl(const GURL& icon_url,
                                     int size_in_pixels,
                                     int min_size_in_pixels,
                                     VivaldiFaviconCallback callback);

  void GetFaviconFromCacheWithHostFallback(const GURL& page_url,
                                           int size_in_pixels,
                                           VivaldiFaviconCallback callback);

  // Fresh download methods.
  void DownloadFreshFavicon(const GURL& page_url,
                            VivaldiFaviconCallback callback);

  void DownloadFreshFaviconFromIconUrl(const GURL& icon_url,
                                       VivaldiFaviconCallback callback);

  // Download completion callback. Downloaded icons are not validated against
  // the provided minimum or desired sizes following same approach as Chromium.
  // Any valid favicon is better than no favicon and default fallbacks.
  void OnFaviconDownloaded(const GURL& page_url,
                           const GURL& icon_url,
                           VivaldiFaviconCallback callback,
                           const gfx::Image& image,
                           const image_fetcher::RequestMetadata& metadata);

  // Fallback download helper.
  void TryDownloadFaviconFromUrls(const GURL& page_url,
                                  std::vector<GURL> favicon_urls,
                                  VivaldiFaviconCallback callback);

  // Dependencies.
  raw_ptr<favicon::FaviconService> favicon_service_;
  raw_ptr<favicon::LargeIconService> large_icon_service_;
  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  // Task tracker for cleanup.
  base::CancelableTaskTracker task_tracker_;

  // Weak pointer factory for safe callbacks.
  base::WeakPtrFactory<VivaldiFaviconService> weak_ptr_factory_{this};
};

}  // namespace ios

#endif  // IOS_FAVICON_VIVALDI_FAVICON_SERVICE_H_
