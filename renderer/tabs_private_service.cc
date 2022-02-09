// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "renderer/tabs_private_service.h"

#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "content/public/renderer/render_view.h"
#include "renderer/vivaldi_spatial_navigation.h"

namespace vivaldi {

VivaldiTabsPrivateService::VivaldiTabsPrivateService(
    content::RenderFrame* render_frame) {
  DCHECK(render_frame);
  render_frame_ = render_frame;
  blink::AssociatedInterfaceRegistry* registry =
      render_frame->GetAssociatedInterfaceRegistry();
  registry->AddInterface(
      base::BindRepeating(&VivaldiTabsPrivateService::BindTabsPrivateService,
                          base::Unretained(this)));
}

VivaldiTabsPrivateService::~VivaldiTabsPrivateService() {}

void VivaldiTabsPrivateService::BindTabsPrivateService(
    mojo::PendingAssociatedReceiver<
        vivaldi::mojom::VivaldiTabsPrivate> receiver) {
  receiver_.Bind(std::move(receiver));
}

void VivaldiTabsPrivateService::GetAccessKeysForPage(
    GetAccessKeysForPageCallback callback) {
  std::vector<vivaldi::mojom::AccessKeyPtr> access_keys;

  blink::WebLocalFrame* frame =
      render_frame_->GetRenderView()->GetWebView()->FocusedFrame();
  if (!frame) {
    std::move(callback).Run(std::move(access_keys));
    return;
  }

  blink::WebElementCollection elements = frame->GetDocument().All();
  for (blink::WebElement element = elements.FirstItem(); !element.IsNull();
    element = elements.NextItem()) {
    if (element.HasAttribute("accesskey")) {
      ::vivaldi::mojom::AccessKeyPtr entry(::vivaldi::mojom::AccessKey::New());

      entry->access_key = element.GetAttribute("accesskey").Utf8();
      entry->title = element.GetAttribute("title").Utf8();
      entry->href = element.GetAttribute("href").Utf8();
      entry->value = element.GetAttribute("value").Utf8();
      entry->id = element.GetAttribute("id").Utf8();
      entry->tagname = element.TagName().Utf8();
      entry->text_content = element.TextContent().Utf8();

      access_keys.push_back(std::move(entry));
    }
  }
  std::move(callback).Run(std::move(access_keys));
}

void VivaldiTabsPrivateService::GetScrollPosition(
    GetScrollPositionCallback callback) {
  blink::WebLocalFrame* frame =
      render_frame_->GetRenderView()->GetWebView()->FocusedFrame();
  if (!frame) {
    std::move(callback).Run(0, 0);
    return;
  }
  blink::Document* document =
    static_cast<blink::WebLocalFrameImpl*>(frame)->GetFrame()->GetDocument();
  blink::LocalDOMWindow* window = document->domWindow();
  int x = window->scrollX();
  int y = window->scrollY();
  std::move(callback).Run(x, y);
}

void VivaldiTabsPrivateService::GetSpatialNavigationRects(
    GetSpatialNavigationRectsCallback callback) {
  std::vector<::vivaldi::mojom::SpatnavRectPtr> navigation_rects;
  blink::WebLocalFrame* frame =
    render_frame_->GetRenderView()->GetWebView()->FocusedFrame();
  if (!frame) {
    std::move(callback).Run(std::move(navigation_rects));
    return;
  }

  float scale = render_frame_->GetRenderView()
                    ->GetWebView()
                    ->ZoomFactorForDeviceScaleFactor();
  if (scale == 0) {
    scale = 1.0;
  }

  std::vector<blink::WebElement> spatnav_elements;

  blink::Document* document =
    static_cast<blink::WebLocalFrameImpl*>(frame)->GetFrame()->GetDocument();
  blink::LocalDOMWindow* window = document->domWindow();

  blink::WebElementCollection all_elements = frame->GetDocument().All();
  for (blink::WebElement element = all_elements.FirstItem(); !element.IsNull();
    element = all_elements.NextItem()) {
    gfx::Rect rect =
      RevertDeviceScaling(element.BoundsInViewport(), scale);
    if (IsInViewport(document, rect, window->innerHeight()) &&
      IsNavigableElement(element) && IsVisible(element) &&
      !IsTooSmall(rect) && !IsCovered(document, rect)) {
      spatnav_elements.push_back(element);
    }
  }

  for (auto& element : spatnav_elements) {
    gfx::Rect rect = element.BoundsInViewport();
    if (element.IsLink()) {
      blink::IntRect r = FindImageElementRect(element);
      if (!r.IsEmpty()) {
        rect.SetRect(r.X(), r.Y(), r.Width(), r.Height());
      }
    }
    rect = RevertDeviceScaling(rect, scale);
    std::string href = "";
    if (element.IsLink()) {
      href = element.GetAttribute("href").Utf8();
    }

    ::vivaldi::mojom::SpatnavRectPtr spatnav_rect(
        ::vivaldi::mojom::SpatnavRect::New());
    spatnav_rect->x = rect.x();
    spatnav_rect->y = rect.y();
    spatnav_rect->width = rect.width();
    spatnav_rect->height = rect.height();
    spatnav_rect->href = href;
    spatnav_rect->path = ElementPath(element);
    navigation_rects.push_back(std::move(spatnav_rect));
  }
  std::move(callback).Run(std::move(navigation_rects));
}

}  // namespace vivaldi
