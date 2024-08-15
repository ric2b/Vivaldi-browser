// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "app/vivaldi_apptools.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

void WebViewImpl::UpdateVivaldiRendererPreferences() {
    SetServeResourceFromCacheOnly(
        renderer_preferences_.serve_resources_only_from_cache);
    SetAllowTabCycleIntoUI(
        renderer_preferences_.allow_tab_cycle_from_webpage_into_ui);
}

void WebViewImpl::SetImagesEnabled(const bool images_enabled) {
  auto* main_local_frame = DynamicTo<LocalFrame>(GetPage()->MainFrame());
  if (!main_local_frame) {
    return;
  }
  Document* document = main_local_frame->GetDocument();

  if (document) {
    web_settings_->SetImagesEnabled(images_enabled);

    // NOTE(andre@vivaldi.com): This is all images, but images with data-urls
    // will load without this.
    HTMLCollection* images = document->images();
    size_t sourceLength = images->length();
    for (size_t i = 0; i < sourceLength; ++i) {
      Element* element = images->item(static_cast<unsigned>(i));
      if (element) {
        auto* imageElement = DynamicTo<HTMLImageElement>(*element);

        if (images_enabled) {
          imageElement->setAttribute(html_names::kSrcAttr,
                                     element->ImageSourceURL());
        } else {
          int width = 10, height = 10;
          SkBitmap bitmap;
          bitmap.allocN32Pixels(width, height, true);
          bitmap.eraseColor(0xFFFFFFFF);
          imageElement->SetImageForTest(ImageResourceContent::CreateLoaded(
              blink::UnacceleratedStaticBitmapImage::Create(
                  SkImages::RasterFromBitmap(bitmap))));
        }
      }
    }
  }
}

void WebViewImpl::SetServeResourceFromCacheOnly(
    const bool only_load_from_cache) {
  web_settings_->SetServeResourceFromCacheOnly(only_load_from_cache);
}

void WebViewImpl::SetAllowTabCycleIntoUI(
    bool allow_tab_cycle_from_webpage_into_ui) {
  web_settings_->SetAllowTabCycleIntoUI(allow_tab_cycle_from_webpage_into_ui);
}

void WebViewImpl::LoadImageAt(const gfx::Point& point) {
  WebHitTestResult result = HitTestResultForTap(point, gfx::Size(0, 0));

  Node* node = result.GetNode().Unwrap<Node>();
  DCHECK(node);
  if (!node || !IsA<HTMLImageElement>(*node))
    return;

  auto* imageElement = DynamicTo<HTMLImageElement>(blink::To<Element>(*node));
  imageElement->ForceReload();
}

}  // namespace blink
