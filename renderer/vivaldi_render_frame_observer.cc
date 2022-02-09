// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "renderer/vivaldi_render_frame_observer.h"

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "renderer/mojo/vivaldi_tabs_private.mojom.h"
#include "renderer/tabs_private_service.h"
#include "renderer/vivaldi_render_messages.h"
#include "renderer/vivaldi_snapshot_page.h"
#include "renderer/vivaldi_spatial_navigation.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/skia/include/core/SkBitmap.h"

#include "ui/events/types/scroll_types.h"
#include "ui/vivaldi_skia_utils.h"

using blink::WebDocument;
using blink::WebLocalFrame;
using blink::WebView;

namespace vivaldi {

VivaldiRenderFrameObserver::VivaldiRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  DCHECK(render_frame);
  tabs_private_service_ =
      std::make_unique<VivaldiTabsPrivateService>(render_frame);
}

VivaldiRenderFrameObserver::~VivaldiRenderFrameObserver() {}

bool VivaldiRenderFrameObserver::OnMessageReceived(
    const IPC::Message& message) {

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(VivaldiRenderFrameObserver, message)
    IPC_MESSAGE_HANDLER(VivaldiFrameHostMsg_ResumeParser, OnResumeParser)

    IPC_MESSAGE_HANDLER(VivaldiMsg_InsertText, OnInsertText)
    IPC_MESSAGE_HANDLER(VivaldiViewMsg_RequestThumbnailForFrame,
      OnRequestThumbnailForFrame)
    IPC_MESSAGE_HANDLER(VivaldiViewMsg_AccessKeyAction, OnAccessKeyAction)
    IPC_MESSAGE_HANDLER(VivaldiViewMsg_ScrollPage, OnScrollPage)
    IPC_MESSAGE_HANDLER(VivaldiViewMsg_ActivateElementFromPoint,
      OnActivateElementFromPoint);

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void VivaldiRenderFrameObserver::OnResumeParser() {
  blink::WebDocumentLoader* loader =
      render_frame()->GetWebFrame()->GetDocumentLoader();

  if (!loader) {
    NOTREACHED();
    return;
  }
  loader->ResumeParser();
}

void VivaldiRenderFrameObserver::FocusedElementChanged(
    const blink::WebElement& element) {
  std::string tagname = "";
  std::string type = "";
  std::string role = "";
  bool editable = false;
  if (!element.IsNull()) {
    tagname = element.TagName().Utf8();
    type =
        element.HasAttribute("type") ? element.GetAttribute("type").Utf8() : "";
    editable = element.IsEditable();
    role =
        element.HasAttribute("role") ? element.GetAttribute("role").Utf8() : "";
  }
  Send(new VivaldiMsg_DidUpdateFocusedElementInfo(routing_id(), tagname, type,
                                                  editable, role));
}

void VivaldiRenderFrameObserver::OnDestruct() {
  delete this;
}


// Inserts text into input fields.
void VivaldiRenderFrameObserver::OnInsertText(const std::u16string& text) {
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  const std::vector<ui::ImeTextSpan> ime_text_spans;
  frame->GetInputMethodController()->CommitText(
    blink::WebString::FromUTF16(text), ime_text_spans,
    blink::WebRange(), 0);
}

void VivaldiRenderFrameObserver::OnActivateElementFromPoint(int x,
  int y,
  int modifiers) {
  WebLocalFrame* frame = render_frame()->GetRenderView()->GetWebView()->FocusedFrame();
  if (!frame) {
    return;
  }
  blink::Document* document =
    static_cast<blink::WebLocalFrameImpl*>(frame)->GetFrame()->GetDocument();
  blink::Element* elm = document->ElementFromPoint(x, y);

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

void VivaldiRenderFrameObserver::OnAccessKeyAction(std::string access_key) {
  WebLocalFrame* frame = render_frame()->GetRenderView()->GetWebView()->FocusedFrame();
  if (!frame) {
    return;
  }
  blink::Document* document =
    static_cast<blink::WebLocalFrameImpl*>(frame)->GetFrame()->GetDocument();
  String wtf_key(access_key.c_str());
  blink::Element* elem = document->GetElementByAccessKey(wtf_key);
  if (elem) {
    elem->AccessKeyAction(blink::SimulatedClickCreationScope::kFromUserAgent);
  }
}

void VivaldiRenderFrameObserver::OnScrollPage(std::string scroll_type,
  int scroll_amount) {
  WebLocalFrame* web_local_frame = render_frame()->GetRenderView()->GetWebView()->FocusedFrame();
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
  }
  else if (scroll_type == "down") {
    window->scrollBy(0.0, scroll_amount);
  }
  else if (scroll_type == "left") {
    window->scrollBy(-scroll_amount, 0.0);
  }
  else if (scroll_type == "right") {
    window->scrollBy(scroll_amount, 0.0);

  }
  else if (scroll_type == "pageup") {
    local_frame->GetEventHandler().BubblingScroll(
      blink::mojom::blink::ScrollDirection::kScrollBlockDirectionBackward,
      blink::ScrollGranularity::kScrollByPage);
  }
  else if (scroll_type == "pagedown") {
    local_frame->GetEventHandler().BubblingScroll(
      blink::mojom::blink::ScrollDirection::kScrollBlockDirectionForward,
      blink::ScrollGranularity::kScrollByPage);
  }
  else if (scroll_type == "pageleft") {
    local_frame->GetEventHandler().BubblingScroll(
      blink::mojom::blink::ScrollDirection::kScrollLeftIgnoringWritingMode,
      blink::ScrollGranularity::kFirstScrollGranularity);
  }
  else if (scroll_type == "pageright") {
    local_frame->GetEventHandler().BubblingScroll(
      blink::mojom::blink::ScrollDirection::kScrollRightIgnoringWritingMode,
      blink::ScrollGranularity::kFirstScrollGranularity);
  }
  else if (scroll_type == "top") {
    local_frame->GetEventHandler().BubblingScroll(
      blink::mojom::blink::ScrollDirection::kScrollBlockDirectionBackward,
      blink::ScrollGranularity::kScrollByDocument);
  }
  else if (scroll_type == "bottom") {
    local_frame->GetEventHandler().BubblingScroll(
      blink::mojom::blink::ScrollDirection::kScrollBlockDirectionForward,
      blink::ScrollGranularity::kScrollByDocument);
  }
  else {
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

void VivaldiRenderFrameObserver::OnRequestThumbnailForFrame(
  VivaldiViewMsg_RequestThumbnailForFrame_Params params) {
  base::ReadOnlySharedMemoryRegion shared_region;
  gfx::Size ack_size;
  WebView* webview = render_frame()->GetRenderView()->GetWebView();
  do {
    if (!webview)
      break;
    blink::WebFrame* main_frame = webview->MainFrame();
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

  render_frame()->Send(new VivaldiViewHostMsg_RequestThumbnailForFrame_ACK(
      render_frame()->GetRoutingID(), params.callback_id, ack_size, shared_region,
        params.client_id));
}

}  // namespace vivaldi
