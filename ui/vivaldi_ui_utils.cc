// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_ui_utils.h"

#include <string>
#include <vector>

#include "app/vivaldi_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/thumbnails/simple_thumbnail_crop.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/api/extension_types.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/skbitmap_operations.h"
#if defined(OS_WIN) || defined(OS_LINUX)
#include "ui/vivaldi_browser_window.h"
#endif  // OS_WIN || OS_LINUX
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "ui/views/widget/widget.h"

using extensions::AppWindowRegistry;

namespace vivaldi {
namespace ui_tools {

/*static*/
extensions::WebViewGuest* GetActiveWebViewGuest() {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return nullptr;

  return GetActiveWebGuestFromBrowser(browser);
}

/*static*/
extensions::WebViewGuest* GetActiveWebViewGuest(
    extensions::NativeAppWindow* app_window) {
  Browser* browser =
      chrome::FindBrowserWithWindow(app_window->GetNativeWindow());

  if (!browser)
    return nullptr;

  return GetActiveWebGuestFromBrowser(browser);
}

/*static*/
extensions::WebViewGuest* GetActiveWebGuestFromBrowser(Browser* browser) {
  content::WebContents* active_web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!active_web_contents)
    return nullptr;

  return extensions::WebViewGuest::FromWebContents(active_web_contents);
}

/*static*/
extensions::AppWindow* GetActiveAppWindow() {
#if defined(OS_WIN) || defined(OS_LINUX)
  Browser* browser = chrome::FindLastActive();
  if (browser && browser->is_vivaldi())
    return static_cast<const VivaldiBrowserWindow*>(browser->window())
        ->GetAppWindow();
#endif
  return nullptr;
}

content::WebContents* GetWebContentsFromTabStrip(int tab_id, Profile* profile) {
  content::WebContents* contents = nullptr;
  bool include_incognito = true;
  Browser* browser;
  int tab_index;
  extensions::ExtensionTabUtil::GetTabById(tab_id, profile, include_incognito,
                                           &browser, NULL, &contents,
                                           &tab_index);
  return contents;
}

bool IsOutsideAppWindow(int screen_x, int screen_y, Profile* profile) {
  gfx::Point screen_point(screen_x, screen_y);

  AppWindowRegistry* app_window_registry =
      AppWindowRegistry::Factory::GetForBrowserContext(profile, false);
  AppWindowRegistry::AppWindowList list =
      app_window_registry->GetAppWindowsForApp(::vivaldi::kVivaldiAppId);

  bool outside = true;
  for (auto* win : list) {
    gfx::Rect rect = win->GetBaseWindow()->GetBounds();
    if (rect.Contains(screen_point)) {
      outside = false;
      break;
    }
  }
  return outside;
}

bool EncodeBitmap(const SkBitmap& screen_capture,
                  std::vector<unsigned char>* data,
                  std::string* mime_type,
                  extensions::api::extension_types::ImageFormat image_format,
                  gfx::Size size,
                  double scale,
                  int image_quality,
                  bool resize) {
  gfx::Size dst_size_pixels;
  if (size.width() && size.height()) {
    dst_size_pixels.SetSize(size.width(), size.height());
  } else {
    dst_size_pixels = gfx::ScaleToRoundedSize(
        gfx::Size(screen_capture.width(), screen_capture.height()), scale);
  }
  SkBitmap bitmap;
  if (resize) {
    bitmap = skia::ImageOperations::Resize(
        screen_capture, skia::ImageOperations::RESIZE_BEST,
        dst_size_pixels.width(), dst_size_pixels.height());
  } else {
    bitmap = screen_capture;
  }
  bool encoded = false;

  switch (image_format) {
    case extensions::api::extension_types::IMAGE_FORMAT_JPEG:
      if (bitmap.getPixels()) {
        encoded = gfx::JPEGCodec::Encode(
            reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
            gfx::JPEGCodec::FORMAT_SkBitmap, bitmap.width(), bitmap.height(),
            static_cast<int>(bitmap.rowBytes()), image_quality, data);
        *mime_type = "image/jpeg";  // kMimeTypeJpeg;
      }
      break;
    case extensions::api::extension_types::IMAGE_FORMAT_PNG:
      encoded =
          gfx::PNGCodec::EncodeBGRASkBitmap(bitmap,
                                            true,  // Discard transparency.
                                            data);
      *mime_type = "image/png";  // kMimeTypePng;
      break;
    default:
      NOTREACHED() << "Invalid image format.";
  }

  return encoded;
}

SkBitmap SmartCropAndSize(const SkBitmap& capture,
                          int target_width,
                          int target_height) {
  thumbnails::ClipResult clip_result = thumbnails::CLIP_RESULT_NOT_CLIPPED;
  // Clip it to a more reasonable position.
  SkBitmap clipped_bitmap = thumbnails::SimpleThumbnailCrop::GetClippedBitmap(
      capture, target_width, target_height, &clip_result);
  // Resize the result to the target size.
  SkBitmap result = skia::ImageOperations::Resize(
      clipped_bitmap, skia::ImageOperations::RESIZE_BEST, target_width,
      target_height);

// NOTE(pettern): Copied from SimpleThumbnailCrop::CreateThumbnail():
#if !defined(USE_AURA)
  // This is a bit subtle. SkBitmaps are refcounted, but the magic
  // ones in PlatformCanvas can't be assigned to SkBitmap with proper
  // refcounting.  If the bitmap doesn't change, then the downsampler
  // will return the input bitmap, which will be the reference to the
  // weird PlatformCanvas one insetad of a regular one. To get a
  // regular refcounted bitmap, we need to copy it.
  //
  // On Aura, the PlatformCanvas is platform-independent and does not have
  // any native platform resources that can't be refounted, so this issue does
  // not occur.
  //
  // Note that GetClippedBitmap() does extractSubset() but it won't copy
  // the pixels, hence we check result size == clipped_bitmap size here.
  if (clipped_bitmap.width() == result.width() &&
      clipped_bitmap.height() == result.height()) {
    clipped_bitmap.readPixels(result.info(), result.getPixels(),
                              result.rowBytes(), 0, 0);
  }
#endif
  return result;
}

Browser* GetBrowserFromWebContents(content::WebContents* web_contents) {
  DCHECK(web_contents);
  gfx::NativeWindow window =
    platform_util::GetTopLevel(web_contents->GetNativeView());
  DCHECK(window);
  return chrome::FindBrowserWithWindow(window);
}

}  // namespace ui_tools
}  // namespace vivaldi
