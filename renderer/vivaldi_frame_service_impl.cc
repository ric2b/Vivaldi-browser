// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "renderer/vivaldi_frame_service_impl.h"

#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "ui/events/types/scroll_types.h"

#include "renderer/blink/vivaldi_snapshot_page.h"
#include "renderer/blink/vivaldi_spatnav_quad.h"
#include "ui/vivaldi_skia_utils.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#endif

namespace {

const char kFrameServiceKey[1] = "";

}  // namespace

VivaldiFrameServiceImpl::VivaldiFrameServiceImpl(
    content::RenderFrame* render_frame)
    : render_frame_(render_frame) {
  spatnav_controller_ =
      std::make_unique<VivaldiSpatialNavigationController>(render_frame);
}

VivaldiFrameServiceImpl::~VivaldiFrameServiceImpl() {}

// static
void VivaldiFrameServiceImpl::Register(content::RenderFrame* render_frame) {
  DCHECK(render_frame);
  DCHECK(!render_frame->GetUserData(kFrameServiceKey));
  blink::AssociatedInterfaceRegistry* registry =
      render_frame->GetAssociatedInterfaceRegistry();

  auto service = std::make_unique<VivaldiFrameServiceImpl>(render_frame);

  // Unretained() is safe as the render frame owns both the registry and the
  // service.
  registry->AddInterface<vivaldi::mojom::VivaldiFrameService>(
      base::BindRepeating(&VivaldiFrameServiceImpl::BindService,
                          base::Unretained(service.get())));
  render_frame->SetUserData(kFrameServiceKey, std::move(service));
}

void VivaldiFrameServiceImpl::BindService(
    mojo::PendingAssociatedReceiver<vivaldi::mojom::VivaldiFrameService>
        receiver) {
  // According to PaintPreviewRecorderImpl::BindPaintPreviewRecorder() it is
  // possible that we can be called multiple times. So reset receiver_ first.
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void VivaldiFrameServiceImpl::GetAccessKeysForPage(
    GetAccessKeysForPageCallback callback) {
  std::vector<vivaldi::mojom::AccessKeyPtr> access_keys;

  blink::WebLocalFrame* frame =
      render_frame_->GetWebView()->FocusedFrame();
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

void VivaldiFrameServiceImpl::AccessKeyAction(const std::string& access_key) {
  blink::WebLocalFrame* frame =
      render_frame_->GetWebView()->FocusedFrame();
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

blink::Document* VivaldiFrameServiceImpl::GetDocument() {
  blink::WebLocalFrame* frame =
      render_frame_->GetWebView()->FocusedFrame();
  if (!frame) {
    return nullptr;
  }
  blink::Document* document =
      static_cast<blink::WebLocalFrameImpl*>(frame)->GetFrame()->GetDocument();
  return document;
}

void VivaldiFrameServiceImpl::ScrollPage(
    ::vivaldi::mojom::ScrollType scroll_type,
    int scroll_amount) {
  using ScrollType = ::vivaldi::mojom::ScrollType;
  blink::Document* document = GetDocument();

  blink::WebLocalFrame* web_local_frame =
      render_frame_->GetWebView()->FocusedFrame();


  if (!web_local_frame) {
    return;
  }
  blink::LocalDOMWindow* window = document->domWindow();

  // WebLocalFrame doesn't have what we need.
  blink::LocalFrame* local_frame =
      static_cast<blink::WebLocalFrameImpl*>(web_local_frame)->GetFrame();

  switch (scroll_type) {
    case ScrollType::kUp:
      window->scrollBy(0.0, -scroll_amount);
      break;
    case ScrollType::kDown:
      window->scrollBy(0.0, scroll_amount);
      break;
    case ScrollType::kLeft:
      window->scrollBy(-scroll_amount, 0.0);
      break;
    case ScrollType::kRight:
      window->scrollBy(scroll_amount, 0.0);
      break;
    case ScrollType::kPageUp:
      local_frame->GetEventHandler().BubblingScroll(
          blink::mojom::blink::ScrollDirection::kScrollBlockDirectionBackward,
          ui::ScrollGranularity::kScrollByPage);
      break;
    case ScrollType::kPageDown:
      local_frame->GetEventHandler().BubblingScroll(
          blink::mojom::blink::ScrollDirection::kScrollBlockDirectionForward,
          ui::ScrollGranularity::kScrollByPage);
      break;
    case ScrollType::kPageLeft:
      local_frame->GetEventHandler().BubblingScroll(
          blink::mojom::blink::ScrollDirection::kScrollLeftIgnoringWritingMode,
          ui::ScrollGranularity::kFirstScrollGranularity);
      break;
    case ScrollType::kPageRight:
      local_frame->GetEventHandler().BubblingScroll(
          blink::mojom::blink::ScrollDirection::kScrollRightIgnoringWritingMode,
          ui::ScrollGranularity::kFirstScrollGranularity);
      break;
    case ScrollType::kTop:
      local_frame->GetEventHandler().BubblingScroll(
          blink::mojom::blink::ScrollDirection::kScrollBlockDirectionBackward,
          ui::ScrollGranularity::kScrollByDocument);
      break;
    case ScrollType::kBottom:
      local_frame->GetEventHandler().BubblingScroll(
          blink::mojom::blink::ScrollDirection::kScrollBlockDirectionForward,
          ui::ScrollGranularity::kScrollByDocument);
      break;
  }
}

bool VivaldiFrameServiceImpl::UpdateSpatnavQuads() {
  return spatnav_controller_->UpdateQuads();
}

void VivaldiFrameServiceImpl::MoveSpatnavRect(
    vivaldi::mojom::SpatnavDirection direction,
    MoveSpatnavRectCallback callback) {
  vivaldi::mojom::SpatnavRectPtr spatnav_rect(
      vivaldi::mojom::SpatnavRect::New());
  std::string href;

  blink::DOMRect new_rect(0, 0, 0, 0);
  spatnav_controller_->MoveRect(direction, &new_rect, &href);

  spatnav_rect->x = new_rect.x();
  spatnav_rect->y = new_rect.y();
  spatnav_rect->width = new_rect.width();
  spatnav_rect->height = new_rect.height();
  spatnav_rect->href = href;

  std::move(callback).Run(std::move(spatnav_rect));
}

void VivaldiFrameServiceImpl::GetFocusedElementInfo(
    GetFocusedElementInfoCallback callback) {
  blink::WebLocalFrame* frame =
      render_frame_->GetWebView()->FocusedFrame();
  if (!frame) {
    frame = render_frame_->GetWebFrame();
  }
  if (!frame) {
    std::move(callback).Run("", "", false, "");
    return;
  }
  blink::WebElement element = frame->GetDocument().FocusedElement();

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

  // In case we are in a web plugin such as a PDF, the focused html element is
  // not going to tell us what we need, so we get the plugin instance and ask it
  // directly.
#if BUILDFLAG(ENABLE_PLUGINS)
  auto* plugin_container = static_cast<blink::WebLocalFrameImpl*>(frame)
                               ->GetFrame()
                               ->GetWebPluginContainer();
  if (plugin_container) {
    auto *plugin = plugin_container->Plugin();
    if (plugin) {
      editable = plugin->CanEditText();
    }
  }
#endif
  std::move(callback).Run(tagname, type, editable, role);
}

void VivaldiFrameServiceImpl::DetermineTextLanguage(
    const std::string& text,
    DetermineTextLanguageCallback callback) {
  bool is_model_reliable = false;
  float model_reliability_score = 0.0;
  std::string language;

  if (translate::IsTFLiteLanguageDetectionEnabled()) {
    // Implement when relevant
    NOTREACHED();
  } else {
    language = translate::DetermineTextLanguage(text, &is_model_reliable,
                                                model_reliability_score);
  }
  std::move(callback).Run(std::move(language));
}

void VivaldiFrameServiceImpl::ActivateSpatnavElement(int modifiers) {
  spatnav_controller_->ActivateElement(modifiers);
}

void VivaldiFrameServiceImpl::HideSpatnavIndicator() {
  spatnav_controller_->HideIndicator();
}

// NOTE(daniel@vivaldi.com): This doesn't always work correctly. Should we fall back
// on just CloseSpatnav?
void VivaldiFrameServiceImpl::CloseSpatnavOrCurrentOpenMenu(
    CloseSpatnavOrCurrentOpenMenuCallback callback) {

  bool layout_changed = false;
  bool element_valid = false;
  spatnav_controller_->CloseSpatnavOrCurrentOpenMenu(layout_changed,
                                                     element_valid);

  std::move(callback).Run(layout_changed, element_valid);
}

void VivaldiFrameServiceImpl::InsertText(const std::string& text) {
  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
  const std::vector<ui::ImeTextSpan> ime_text_spans;
  frame->GetInputMethodController()->CommitText(
      blink::WebString::FromUTF8(text), ime_text_spans, blink::WebRange(), 0);
}

void VivaldiFrameServiceImpl::ResumeParser() {
  blink::WebDocumentLoader* loader =
      render_frame_->GetWebFrame()->GetDocumentLoader();
  if (!loader)
    return;
  loader->ResumeParser();
}

void VivaldiFrameServiceImpl::SetSupportsDraggableRegions(
    bool supports_draggable_regions) {
  render_frame_->GetWebView()->SetSupportsDraggableRegions(supports_draggable_regions);
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

void VivaldiFrameServiceImpl::RequestThumbnailForFrame(
    const ::gfx::Rect& rect_arg,
    bool full_page,
    const ::gfx::Size& target_size,
    RequestThumbnailForFrameCallback callback) {
  base::ReadOnlySharedMemoryRegion shared_region;
  gfx::Size ack_size;
  blink::WebView* webview = render_frame_->GetWebView();
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

    if (!VivaldiSnapshotPage(local_frame, full_page, rect_arg, &bitmap))
      break;

    if (!target_size.IsEmpty()) {
      // Scale and crop it now.
      bitmap = vivaldi::skia_utils::SmartCropAndSize(
          bitmap, target_size.width(), target_size.height());
    }

    if (!CopyBitmapToSharedRegionAsN32(bitmap, &shared_region))
      break;

    ack_size.SetSize(bitmap.width(), bitmap.height());
  } while (false);

  std::move(callback).Run(ack_size, std::move(shared_region));
}
