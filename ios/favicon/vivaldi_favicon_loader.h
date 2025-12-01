// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_FAVICON_VIVALDI_FAVICON_LOADER_H_
#define IOS_FAVICON_VIVALDI_FAVICON_LOADER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/favicon/vivaldi_favicon_service.h"

namespace ios {
class VivaldiFaviconService;
}  // namespace ios

class GURL;

// Forward declare Objective-C types with cross-language compatibility
// This header is included in both C++ (.cc) and Objective-C++ (.mm) files
#ifdef __OBJC__
// In Objective-C++ context: use proper Objective-C block syntax
@class FaviconAttributes;
using FaviconAttributesCompletionBlock = void (^)(FaviconAttributes*);
#else
// In pure C++ context: use forward declaration and base::OnceCallback
class FaviconAttributes;
using FaviconAttributesCompletionBlock =
    base::OnceCallback<void(FaviconAttributes*)>;
#endif

// Use VivaldiFaviconOptions from the service header
using VivaldiFaviconMode = ios::VivaldiFaviconOptions;

namespace ios {

// Pure C++ wrapper for VivaldiFaviconService that provides
// Chromium FaviconLoader-like interface.
//
// IMPORTANT: Following Chromium's FaviconLoaderImpl pattern, the behavior
// varies by mode:
// - kCacheOnly: Returns cached favicon or fallback if not found
// - kAlwaysFresh: Always downloads fresh favicon from network
// - kCacheFirstThenFresh: Checks cache first, downloads fresh if not found
//
// The kCacheFirstThenFresh mode provides cached favicons immediately when
// available, and only downloading fresh ones when cache does not have an icon.

class VivaldiFaviconLoader : public KeyedService {
 public:
  explicit VivaldiFaviconLoader(VivaldiFaviconService* service);

  VivaldiFaviconLoader(const VivaldiFaviconLoader&) = delete;
  VivaldiFaviconLoader& operator=(const VivaldiFaviconLoader&) = delete;

  ~VivaldiFaviconLoader() override;

  // Vivaldi-specific methods with VivaldiFaviconOptions support.
  void GetFaviconForPageUrl(const GURL& page_url,
                            float size_in_points,
                            float min_size_in_points,
                            VivaldiFaviconMode mode,
                            FaviconAttributesCompletionBlock completion);

  void GetFaviconForIconUrl(const GURL& icon_url,
                            float size_in_points,
                            float min_size_in_points,
                            VivaldiFaviconMode mode,
                            FaviconAttributesCompletionBlock completion);

  void GetFaviconForPageUrlOrHost(const GURL& page_url,
                                  float size_in_points,
                                  VivaldiFaviconMode mode,
                                  FaviconAttributesCompletionBlock completion);

  // Original Chromium-favicon loader methods unchanged for
  // backward compatibility, and calls same functions as
  // chromium favicon loader.
  // fallback_to_google_server is always false.
  void FaviconForPageUrl(
      const GURL& page_url,
      float size_in_points,
      float min_size_in_points,
      bool fallback_to_google_server,
      FaviconAttributesCompletionBlock favicon_block_handler);

  void FaviconForPageUrlOrHost(
      const GURL& page_url,
      float size_in_points,
      FaviconAttributesCompletionBlock favicon_block_handler);

  void FaviconForIconUrl(
      const GURL& icon_url,
      float size_in_points,
      float min_size_in_points,
      FaviconAttributesCompletionBlock favicon_block_handler);

  void CancelAllRequests();

 private:
  VivaldiFaviconService* service_;  // Not owned.
  base::WeakPtrFactory<VivaldiFaviconLoader> weak_ptr_factory_{this};

  // Helper methods to reduce code duplication
  base::OnceCallback<void(const favicon_base::LargeIconResult&)>
  CreateLargeIconResultToAttributesCallback(
      FaviconAttributesCompletionBlock completion);

  // Generic helper for handling favicon requests with different service methods
  template <typename ServiceMethod>
  void RequestFaviconWithMode(ServiceMethod service_method,
                              VivaldiFaviconOptions options,
                              FaviconAttributesCompletionBlock completion);

  // Direct Chromium-compatible methods - these call the exact same
  // LargeIconService methods as Chromium's FaviconLoader.
  void GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      const GURL& page_url,
      float min_size_in_points,
      float size_in_points,
      FaviconAttributesCompletionBlock completion);

  void GetIconRawBitmapOrFallbackStyleForPageUrl(
      const GURL& page_url,
      float size_in_points,
      FaviconAttributesCompletionBlock completion);

  void GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
      const GURL& icon_url,
      float min_size_in_points,
      float size_in_points,
      FaviconAttributesCompletionBlock completion);
};

}  // namespace ios

#endif  // IOS_FAVICON_VIVALDI_FAVICON_LOADER_H_
