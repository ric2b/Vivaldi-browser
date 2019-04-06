// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_ui_utils.h"

#include <string>
#include <vector>

#include "app/vivaldi_constants.h"
#include "base/json/json_reader.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "chrome/browser/thumbnails/thumbnail_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/vivaldi_browser_window.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "ui/views/widget/widget.h"

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
VivaldiBrowserWindow* GetActiveAppWindow() {
#if defined(OS_WIN) || defined(OS_LINUX)
  Browser* browser = chrome::FindLastActive();
  if (browser && browser->is_vivaldi())
    return static_cast<VivaldiBrowserWindow*>(browser->window());
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

  bool outside = true;
  for (auto* browser : *BrowserList::GetInstance()) {
    gfx::Rect rect =
        browser->is_devtools() ? gfx::Rect() : browser->window()->GetBounds();
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
            bitmap, image_quality, data);
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

SkBitmap GetClippedBitmap(const SkBitmap& bitmap,
                          int desired_width,
                          int desired_height,
                          thumbnails::ClipResult* clip_result) {
  gfx::Rect clipping_rect =
      thumbnails::GetClippingRect(gfx::Size(bitmap.width(), bitmap.height()),
                      gfx::Size(desired_width, desired_height), clip_result);
  SkIRect src_rect = { clipping_rect.x(), clipping_rect.y(),
    clipping_rect.right(), clipping_rect.bottom() };
  SkBitmap clipped_bitmap;
  bitmap.extractSubset(&clipped_bitmap, src_rect);
  return clipped_bitmap;
}

SkBitmap SmartCropAndSize(const SkBitmap& capture,
                          int target_width,
                          int target_height) {
  thumbnails::ClipResult clip_result = thumbnails::CLIP_RESULT_NOT_CLIPPED;
  // Clip it to a more reasonable position.
  SkBitmap clipped_bitmap =
      GetClippedBitmap(capture, target_width, target_height, &clip_result);
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

bool IsMainVivaldiBrowserWindow(Browser* browser) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::unique_ptr<base::Value> json =
      base::JSONReader::Read(browser->ext_data(), options);
  base::DictionaryValue* dict = NULL;
  std::string window_type;
  if (json && json->GetAsDictionary(&dict)) {
    dict->GetString("windowType", &window_type);
    // Don't track popup windows (like settings) in the session.
    // We have "", "popup" and "settings".
    // TODO(pettern): Popup windows still rely on extData, this
    // should go away and we should use the type sent to the apis
    // instead.
    if (window_type == "popup" || window_type == "settings") {
      return false;
    }
  }
  if (static_cast<VivaldiBrowserWindow*>(browser->window())->type() ==
      VivaldiBrowserWindow::WindowType::NORMAL) {
    return true;
  }
  return false;
}


Browser* FindBrowserForPinnedTabs(Browser* current_browser) {
  if (current_browser->profile()->IsOffTheRecord()) {
    // Pinned tabs can never be moved to another browser
    return nullptr;
  }
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser == current_browser) {
      continue;
    }
    if (!browser->window()) {
      continue;
    }
    if (browser->type() != current_browser->type()) {
      continue;
    }
    if (browser->is_devtools()) {
      continue;
    }
    if (!IsMainVivaldiBrowserWindow(browser)) {
      continue;
    }
    return browser;
  }
  return nullptr;
}

// Based on TabsMoveFunction::MoveTab() but greatly simplified.
bool MoveTabToWindow(Browser* source_browser,
                     Browser* target_browser,
                     int tab_index,
                     int* new_index,
                     int iteration) {
  DCHECK(source_browser != target_browser);

  // Insert the tabs one after another.
  *new_index += iteration;

  TabStripModel* source_tab_strip = source_browser->tab_strip_model();
  std::unique_ptr<content::WebContents> web_contents =
      source_tab_strip->DetachWebContentsAt(tab_index);
  if (!web_contents) {
    return false;
  }
  TabStripModel* target_tab_strip = target_browser->tab_strip_model();

  // Clamp move location to the last position.
  // This is ">" because it can append to a new index position.
  // -1 means set the move location to the last position.
  if (*new_index > target_tab_strip->count() || *new_index < 0)
    *new_index = target_tab_strip->count();

  target_tab_strip->InsertWebContentsAt(
    *new_index, std::move(web_contents), TabStripModel::ADD_PINNED);

  return true;
}

bool GetTabById(int tab_id, content::WebContents** contents, int* index) {
  for (auto* target_browser : *BrowserList::GetInstance()) {
    TabStripModel* target_tab_strip = target_browser->tab_strip_model();
    for (int i = 0; i < target_tab_strip->count(); ++i) {
      content::WebContents* target_contents =
          target_tab_strip->GetWebContentsAt(i);
      if (SessionTabHelper::IdForTab(target_contents).id() == tab_id) {
        if (contents)
          *contents = target_contents;
        if (index)
          *index = i;
        return true;
      }
    }
  }
  return false;
}

}  // namespace ui_tools
}  // namespace vivaldi
