// Copyright 2025 Vivaldi Technologies. All rights reserved.

#include "ios/favicon/vivaldi_favicon_loader.h"

#include <UIKit/UIKit.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "components/favicon_base/favicon_types.h"
#include "ios/chrome/common/ui/favicon/favicon_attributes.h"
#include "ios/favicon/vivaldi_favicon_service.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace {

int PointsToPixels(float points) {
  if (points <= 0) {
    return 0;
  }
  // Follow scaling approach from Chromium.
  const CGFloat scale = UIScreen.mainScreen.scale;
  return static_cast<int>(scale * points);
}

// Helper function to create FaviconAttributes from LargeIconResult with
// Chromium's validation, following the same pattern as FaviconLoaderImpl.
FaviconAttributes* CreateValidatedFaviconAttributes(
    const favicon_base::LargeIconResult& result) {
  // Follow Chromium's FaviconLoaderImpl pattern for handling LargeIconResult
  if (result.bitmap.is_valid()) {
    scoped_refptr<base::RefCountedMemory> data =
        result.bitmap.bitmap_data.get();

    // The favicon code assumes favicons are PNG-encoded.
    UIImage* favicon = [UIImage
        imageWithData:[NSData dataWithBytes:data->front() length:data->size()]];

    if (favicon && favicon.size.width > 0 && favicon.size.height > 0) {
      return [FaviconAttributes attributesWithImage:favicon];
    }
  }

  // Did not get valid favicon, return default
  return [FaviconAttributes attributesWithDefaultImage];
}

}  // namespace

namespace ios {

VivaldiFaviconLoader::VivaldiFaviconLoader(VivaldiFaviconService* service)
    : service_(service) {
  CHECK(service_);
}

VivaldiFaviconLoader::~VivaldiFaviconLoader() = default;

// Helper to create a callback that converts LargeIconResult to
// FaviconAttributes
base::OnceCallback<void(const favicon_base::LargeIconResult&)>
VivaldiFaviconLoader::CreateLargeIconResultToAttributesCallback(
    FaviconAttributesCompletionBlock completion) {
  return base::BindOnce(
      [](FaviconAttributesCompletionBlock completion,
         const favicon_base::LargeIconResult& result) {
        FaviconAttributes* attributes =
            CreateValidatedFaviconAttributes(result);
        completion(attributes);
      },
      completion);
}

// Generic helper for handling favicon requests with different service methods
template <typename ServiceMethod>
void VivaldiFaviconLoader::RequestFaviconWithMode(
    ServiceMethod service_method,
    VivaldiFaviconOptions options,
    FaviconAttributesCompletionBlock completion) {
  auto callback = CreateLargeIconResultToAttributesCallback(completion);
  service_method(options, std::move(callback));
}

void VivaldiFaviconLoader::GetFaviconForPageUrl(
    const GURL& page_url,
    float size_in_points,
    float min_size_in_points,
    VivaldiFaviconMode mode,
    FaviconAttributesCompletionBlock completion) {
  auto service_method = [this, page_url, size_in_points, min_size_in_points](
                            VivaldiFaviconOptions opts,
                            VivaldiFaviconCallback callback) {
    service_->FaviconForPageUrl(page_url, PointsToPixels(size_in_points),
                                PointsToPixels(min_size_in_points), opts,
                                std::move(callback));
  };

  RequestFaviconWithMode(service_method, mode, completion);
}

void VivaldiFaviconLoader::GetFaviconForIconUrl(
    const GURL& icon_url,
    float size_in_points,
    float min_size_in_points,
    VivaldiFaviconMode mode,
    FaviconAttributesCompletionBlock completion) {
  auto service_method = [this, icon_url, size_in_points, min_size_in_points](
                            VivaldiFaviconOptions opts,
                            VivaldiFaviconCallback callback) {
    service_->FaviconForIconUrl(icon_url, PointsToPixels(size_in_points),
                                PointsToPixels(min_size_in_points), opts,
                                std::move(callback));
  };

  RequestFaviconWithMode(service_method, mode, completion);
}

void VivaldiFaviconLoader::GetFaviconForPageUrlOrHost(
    const GURL& page_url,
    float size_in_points,
    VivaldiFaviconMode mode,
    FaviconAttributesCompletionBlock completion) {
  auto service_method = [this, page_url, size_in_points](
                            VivaldiFaviconOptions opts,
                            VivaldiFaviconCallback callback) {
    service_->FaviconForPageUrlOrHost(page_url, PointsToPixels(size_in_points),
                                      opts, std::move(callback));
  };

  RequestFaviconWithMode(service_method, mode, completion);
}

void VivaldiFaviconLoader::FaviconForPageUrl(
    const GURL& page_url,
    float size_in_points,
    float min_size_in_points,
    bool fallback_to_google_server,
    FaviconAttributesCompletionBlock favicon_block_handler) {
  GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      page_url, min_size_in_points, size_in_points, favicon_block_handler);
}

void VivaldiFaviconLoader::FaviconForPageUrlOrHost(
    const GURL& page_url,
    float size_in_points,
    FaviconAttributesCompletionBlock favicon_block_handler) {
  GetIconRawBitmapOrFallbackStyleForPageUrl(page_url, size_in_points,
                                            favicon_block_handler);
}

void VivaldiFaviconLoader::FaviconForIconUrl(
    const GURL& icon_url,
    float size_in_points,
    float min_size_in_points,
    FaviconAttributesCompletionBlock favicon_block_handler) {
  GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
      icon_url, min_size_in_points, size_in_points, favicon_block_handler);
}

void VivaldiFaviconLoader::CancelAllRequests() {
  service_->CancelAllRequests();
}

void VivaldiFaviconLoader::GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
    const GURL& page_url,
    float min_size_in_points,
    float size_in_points,
    FaviconAttributesCompletionBlock completion) {
  auto cpp_callback = base::BindOnce(
      [](FaviconAttributesCompletionBlock completion,
         const favicon_base::LargeIconResult& result) {
        FaviconAttributes* attributes =
            CreateValidatedFaviconAttributes(result);
        completion(attributes);
      },
      completion);

  service_->GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      page_url, PointsToPixels(min_size_in_points),
      PointsToPixels(size_in_points), std::move(cpp_callback));
}

void VivaldiFaviconLoader::GetIconRawBitmapOrFallbackStyleForPageUrl(
    const GURL& page_url,
    float size_in_points,
    FaviconAttributesCompletionBlock completion) {
  auto cpp_callback = base::BindOnce(
      [](FaviconAttributesCompletionBlock completion,
         const favicon_base::LargeIconResult& result) {
        FaviconAttributes* attributes =
            CreateValidatedFaviconAttributes(result);
        completion(attributes);
      },
      completion);

  service_->GetIconRawBitmapOrFallbackStyleForPageUrl(
      page_url, PointsToPixels(size_in_points), std::move(cpp_callback));
}

void VivaldiFaviconLoader::GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
    const GURL& icon_url,
    float min_size_in_points,
    float size_in_points,
    FaviconAttributesCompletionBlock completion) {
  auto cpp_callback = base::BindOnce(
      [](FaviconAttributesCompletionBlock completion,
         const favicon_base::LargeIconResult& result) {
        FaviconAttributes* attributes =
            CreateValidatedFaviconAttributes(result);
        completion(attributes);
      },
      completion);

  service_->GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
      icon_url, PointsToPixels(min_size_in_points),
      PointsToPixels(size_in_points), std::move(cpp_callback));
}

}  // namespace ios
