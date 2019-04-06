// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "app/vivaldi_apptools.h"
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/plugins/plugin_data.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

void WebViewImpl::SetPluginsEnabled(const bool plugins_enabled) {
  web_settings_->SetPluginsEnabled(plugins_enabled);
  PluginData::RefreshBrowserSidePluginCache();
}

void WebViewImpl::SetImagesEnabled(const bool images_enabled) {
  Document* document = page_->MainFrame()->IsLocalFrame()
                           ? page_->DeprecatedLocalMainFrame()->GetDocument()
                           : 0;

  if (document && document->GetSettings()->GetImagesEnabled() == images_enabled)
    return;

  // Visibility, boxes are not collapsed.
  web_settings_->SetImagesEnabled(images_enabled);

  if (document) {
    document->Fetcher()->SetImagesEnabled(images_enabled);

    // NOTE(andre@vivaldi.com): This is all images, but images with data-urls
    // will load without this.
    HTMLCollection* images = document->images();
    size_t sourceLength = images->length();
    for (size_t i = 0; i < sourceLength; ++i) {
      Element* element = images->item(i);
      if (element) {
        HTMLImageElement& imageElement = ToHTMLImageElement(*element);

        if (images_enabled) {
          imageElement.setAttribute(HTMLNames::srcAttr,
                                    element->ImageSourceURL());
        } else {
          int width = 10, height = 10;
          SkBitmap bitmap;
          bitmap.allocN32Pixels(width, height, true);
          bitmap.eraseColor(0xFFFFFFFF);
          imageElement.SetImageForTest(ImageResourceContent::CreateLoaded(
              StaticBitmapImage::Create(SkImage::MakeFromBitmap(bitmap))));
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

void WebViewImpl::LoadImageAt(const WebPoint& point) {
  HitTestResult result = HitTestResultForRootFramePos(LayoutPoint(point));
  Node* node = result.InnerNode();
  DCHECK(node);
  if (!node || !IsHTMLImageElement(*node))
    return;

  HTMLImageElement& imageElement = ToHTMLImageElement(ToElement(*node));
  imageElement.ForceReload();
}

}  // namespace blink
