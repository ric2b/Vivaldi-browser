// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "renderer/vivaldi_render_view_observer.h"

#include <memory>
#include <string>

#include "content/child/child_thread_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "renderer/vivaldi_snapshot_page.h"
#include "renderer/vivaldi_spatial_navigation.h"
#include "third_party/blink/public/mojom/input/scroll_direction.mojom-blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/events/types/scroll_types.h"
#include "ui/vivaldi_skia_utils.h"

using blink::WebDocument;
using blink::WebLocalFrame;
using blink::WebRect;
using blink::WebView;

namespace vivaldi {

VivaldiRenderViewObserver::VivaldiRenderViewObserver(
    content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {}

VivaldiRenderViewObserver::~VivaldiRenderViewObserver() {}

void VivaldiRenderViewObserver::OnDestruct() {
  delete this;
}

bool VivaldiRenderViewObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(VivaldiRenderViewObserver, message)
    IPC_MESSAGE_HANDLER(VivaldiMsg_InsertText, OnInsertText)
    IPC_MESSAGE_HANDLER(VivaldiViewMsg_RequestThumbnailForFrame,
                        OnRequestThumbnailForFrame)
    IPC_MESSAGE_HANDLER(VivaldiViewMsg_GetAccessKeysForPage,
                        OnGetAccessKeysForPage)
    IPC_MESSAGE_HANDLER(VivaldiViewMsg_AccessKeyAction, OnAccessKeyAction)
    IPC_MESSAGE_HANDLER(VivaldiViewMsg_ScrollPage, OnScrollPage)
    IPC_MESSAGE_HANDLER(VivaldiViewMsg_GetSpatialNavigationRects,
                        OnGetSpatialNavigationRects)
    IPC_MESSAGE_HANDLER(VivaldiViewMsg_GetScrollPosition,
                        OnGetScrollPosition)
    IPC_MESSAGE_HANDLER(VivaldiViewMsg_ActivateElementFromPoint,
                        OnActivateElementFromPoint);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// Inserts text into input fields.
void VivaldiRenderViewObserver::OnInsertText(const base::string16& text) {
  WebLocalFrame* frame = render_view()->GetWebView()->FocusedFrame();
  const std::vector<ui::ImeTextSpan> ime_text_spans;
  frame->GetInputMethodController()->CommitText(
      blink::WebString::FromUTF16(text), ime_text_spans,
       blink::WebRange(), 0);
}

void VivaldiRenderViewObserver::OnGetSpatialNavigationRects() {
  WebLocalFrame* frame = render_view()->GetWebView()->FocusedFrame();
  if (!frame) {
    return;
  }

  float scale = render_view()->GetWebView()->ZoomFactorForDeviceScaleFactor();
  if (scale == 0) {
    scale = 1.0;
  }

  blink::Document* document =
    static_cast<blink::WebLocalFrameImpl*>(frame)->GetFrame()->GetDocument();
  blink::LocalDOMWindow* window = document->domWindow();

  std::vector<blink::WebElement> spatnav_elements;
  blink::WebElementCollection all_elements = frame->GetDocument().All();
  for (blink::WebElement element = all_elements.FirstItem(); !element.IsNull();
       element = all_elements.NextItem()) {
    blink::WebRect rect =
        RevertDeviceScaling(element.BoundsInViewport(), scale);
    if (IsInViewport(document, rect, window->innerHeight()) &&
        HasNavigableTag(element) && IsVisible(element) &&
        !IsTooSmall(rect) && !IsCovered(document, rect)) {
      spatnav_elements.push_back(element);
    }
  }

  std::vector<VivaldiViewMsg_NavigationRect> navigation_rects;
  for (auto& element: spatnav_elements) {
    blink::WebRect rect = element.BoundsInViewport();
    if (element.IsLink()) {
      blink::IntRect r = FindImageElementRect(element);
      if (!r.IsEmpty()) {
        rect.x = r.X();
        rect.y = r.Y();
        rect.width = r.Width();
        rect.height = r.Height();
      }
    }
    rect = RevertDeviceScaling(rect, scale);
    std::string href = "";
    if (element.IsLink()) {
      href = element.GetAttribute("href").Utf8();
    }
    VivaldiViewMsg_NavigationRect navigation_rect;
    navigation_rect.x = rect.x;
    navigation_rect.y = rect.y;
    navigation_rect.width = rect.width;
    navigation_rect.height = rect.height;
    navigation_rect.href = href;
    navigation_rect.path = ElementPath(element);
    navigation_rects.push_back(navigation_rect);
  }
  Send(new VivaldiViewHostMsg_GetSpatialNavigationRects_ACK(routing_id(),
                                                            navigation_rects));
}

void VivaldiRenderViewObserver::OnGetScrollPosition() {
  WebLocalFrame* frame = render_view()->GetWebView()->FocusedFrame();
  if (!frame) {
    return;
  }
  blink::Document* document =
    static_cast<blink::WebLocalFrameImpl*>(frame)->GetFrame()->GetDocument();
  blink::LocalDOMWindow* window = document->domWindow();
  int x = window->scrollX();
  int y = window->scrollY();
  Send(new VivaldiViewHostMsg_GetScrollPosition_ACK(routing_id(), x, y));
}

void VivaldiRenderViewObserver::OnActivateElementFromPoint(int x,
                                                           int y,
                                                           int modifiers) {
  WebLocalFrame* frame = render_view()->GetWebView()->FocusedFrame();
  if (!frame) {
    return;
  }
  blink::Document* document =
    static_cast<blink::WebLocalFrameImpl*>(frame)->GetFrame()->GetDocument();
  blink::Element *elm = document->ElementFromPoint(x, y);

  if (elm) {
    // This key event is just to pass along modifier info
    blink::WebKeyboardEvent web_keyboard_event(
        blink::WebInputEvent::Type::kRawKeyDown, modifiers, base::TimeTicks());
    blink::KeyboardEvent* key_evt =
        blink::KeyboardEvent::Create(web_keyboard_event, nullptr);
    elm->DispatchSimulatedClick(key_evt);

    blink::FocusParams params;
    elm->focus(params);
  }
}

void VivaldiRenderViewObserver::OnGetAccessKeysForPage() {
  std::vector<VivaldiViewMsg_AccessKeyDefinition> access_keys;

  WebLocalFrame* frame = render_view()->GetWebView()->FocusedFrame();
  if (!frame) {
    return;
  }

  blink::WebElementCollection elements = frame->GetDocument().All();
  for (blink::WebElement element = elements.FirstItem(); !element.IsNull();
       element = elements.NextItem()) {
    if (element.HasAttribute("accesskey")) {
      VivaldiViewMsg_AccessKeyDefinition entry;

      entry.access_key = element.GetAttribute("accesskey").Utf8();
      entry.title = element.GetAttribute("title").Utf8();
      entry.href = element.GetAttribute("href").Utf8();
      entry.value = element.GetAttribute("value").Utf8();
      entry.id = element.GetAttribute("id").Utf8();
      entry.tagname = element.TagName().Utf8();
      entry.textContent = element.TextContent().Utf8();

      access_keys.push_back(entry);
    }
  }
  Send(new VivaldiViewHostMsg_GetAccessKeysForPage_ACK(routing_id(),
                                                       access_keys));
}

void VivaldiRenderViewObserver::OnAccessKeyAction(std::string access_key) {
  WebLocalFrame* frame = render_view()->GetWebView()->FocusedFrame();
  if (!frame) {
    return;
  }
  blink::Document* document =
      static_cast<blink::WebLocalFrameImpl*>(frame)->GetFrame()->GetDocument();
  String wtf_key(access_key.c_str());
  blink::Element *elem = document->GetElementByAccessKey(wtf_key);
  if (elem) {
    elem->AccessKeyAction(false);
  }
}

void VivaldiRenderViewObserver::OnScrollPage(std::string scroll_type,
                                             int scroll_amount) {
  WebLocalFrame* web_local_frame = render_view()->GetWebView()->FocusedFrame();
  if (!web_local_frame) {
    return;
  }

  blink::Document* document =
      static_cast<blink::WebLocalFrameImpl*>(web_local_frame)
          ->GetFrame()
          ->GetDocument();
  blink::LocalDOMWindow* window = document->domWindow();

  // WebLocalFrame doesn't have what we need.
  blink::LocalFrame* local_frame =
      static_cast<blink::WebLocalFrameImpl*>(web_local_frame)
          ->GetFrame();

  if (scroll_type == "up") {
    window->scrollBy(0.0, -scroll_amount);
  } else if (scroll_type == "down") {
    window->scrollBy(0.0, scroll_amount);
  } else if (scroll_type == "left") {
    window->scrollBy(-scroll_amount, 0.0);
  } else if (scroll_type == "right") {
    window->scrollBy(scroll_amount, 0.0);

  } else if (scroll_type == "pageup") {
    local_frame->GetEventHandler().BubblingScroll(
        blink::mojom::blink::ScrollDirection::kScrollBlockDirectionBackward,
        blink::ScrollGranularity::kScrollByPage);
  } else if (scroll_type == "pagedown") {
    local_frame->GetEventHandler().BubblingScroll(
        blink::mojom::blink::ScrollDirection::kScrollBlockDirectionForward,
        blink::ScrollGranularity::kScrollByPage);
  } else if (scroll_type == "pageleft") {
    local_frame->GetEventHandler().BubblingScroll(
        blink::mojom::blink::ScrollDirection::kScrollLeftIgnoringWritingMode,
        blink::ScrollGranularity::kFirstScrollGranularity);
  } else if (scroll_type == "pageright") {
    local_frame->GetEventHandler().BubblingScroll(
        blink::mojom::blink::ScrollDirection::kScrollRightIgnoringWritingMode,
        blink::ScrollGranularity::kFirstScrollGranularity);
  } else if (scroll_type == "top") {
    local_frame->GetEventHandler().BubblingScroll(
        blink::mojom::blink::ScrollDirection::kScrollBlockDirectionBackward,
        blink::ScrollGranularity::kScrollByDocument);
  } else if (scroll_type == "bottom") {
    local_frame->GetEventHandler().BubblingScroll(
        blink::mojom::blink::ScrollDirection::kScrollBlockDirectionForward,
        blink::ScrollGranularity::kScrollByDocument);
  } else {
    NOTREACHED();
  }
}

namespace {
bool CopyBitmapToSharedRegionAsN32(
    const SkBitmap& bitmap,
    base::ReadOnlySharedMemoryRegion* shared_region) {
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(bitmap.width(), bitmap.height());

  size_t buf_size = info.computeMinByteSize();
  if (buf_size == 0 || buf_size > static_cast<size_t>(INT32_MAX))
    return false;

  base::MappedReadOnlyRegion region_and_mapping =
      base::ReadOnlySharedMemoryRegion::Create(buf_size);
  if (!region_and_mapping.IsValid())
    return false;

  if (!bitmap.readPixels(info, region_and_mapping.mapping.memory(),
                         info.minRowBytes(), 0, 0)) {
    return false;
  }

  *shared_region = std::move(region_and_mapping.region);
  return true;
}

}  // namespace

void VivaldiRenderViewObserver::OnRequestThumbnailForFrame(
    VivaldiViewMsg_RequestThumbnailForFrame_Params params) {
  base::ReadOnlySharedMemoryRegion shared_region;
  gfx::Size ack_size;
  do {
    if (!render_view()->GetWebView())
      break;
    blink::WebFrame* main_frame = render_view()->GetWebView()->MainFrame();
    if (!main_frame || !main_frame->IsWebLocalFrame())
      break;
    blink::WebLocalFrameImpl* web_local_frame =
        static_cast<blink::WebLocalFrameImpl*>(main_frame->ToWebLocalFrame());
    blink::LocalFrame* local_frame = web_local_frame->GetFrame();
    if (!local_frame)
      break;

    SkBitmap bitmap;
    blink::IntRect rect;
    rect.SetX(params.rect.x());
    rect.SetY(params.rect.y());
    rect.SetWidth(params.rect.width());
    rect.SetHeight(params.rect.height());

    if (!VivaldiSnapshotPage(local_frame, params.full_page, rect, &bitmap))
      break;

    if (!params.target_size.IsEmpty()) {
      // Scale and crop it now.
      bitmap = vivaldi::skia_utils::SmartCropAndSize(
          bitmap, params.target_size.width(), params.target_size.height());
    }

    if (!CopyBitmapToSharedRegionAsN32(bitmap, &shared_region))
      break;

    ack_size.SetSize(bitmap.width(), bitmap.height());
  } while (false);

  Send(new VivaldiViewHostMsg_RequestThumbnailForFrame_ACK(
      routing_id(), params.callback_id, ack_size, shared_region));
}

}  // namespace vivaldi
