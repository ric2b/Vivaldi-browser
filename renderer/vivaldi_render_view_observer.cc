// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "renderer/vivaldi_render_view_observer.h"

#include <memory>
#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/shared_memory.h"
#include "cc/paint/skia_paint_canvas.h"
#include "content/child/child_thread_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/platform/web_scroll_types.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/codec/jpeg_codec.h"
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// Inserts text into input fields.
void VivaldiRenderViewObserver::OnInsertText(const base::string16& text) {
  WebLocalFrame* frame = render_view()->GetWebView()->FocusedFrame();
  unsigned length = text.length();
  // We do not want any selection.
  frame->SetMarkedText(blink::WebString::FromUTF16(text), length, length);
  frame->UnmarkText();                         // Or marked text.
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

void VivaldiRenderViewObserver::OnScrollPage(std::string scroll_type) {
  WebLocalFrame* web_local_frame = render_view()->GetWebView()->FocusedFrame();
  if (!web_local_frame) {
    return;
  }

  // WebLocalFrame doesn't have what we need.
  blink::LocalFrame* local_frame =
      static_cast<blink::WebLocalFrameImpl*>(web_local_frame)
          ->GetFrame();

  if (scroll_type == "up") {
    local_frame->GetEventHandler().BubblingScroll(
        blink::kScrollBlockDirectionBackward, blink::ScrollGranularity::kScrollByPage);
  } else if (scroll_type == "down") {
    local_frame->GetEventHandler().BubblingScroll(
        blink::kScrollBlockDirectionForward, blink::ScrollGranularity::kScrollByPage);
  } else if (scroll_type == "top") {
    local_frame->GetEventHandler().BubblingScroll(
        blink::kScrollBlockDirectionBackward, blink::ScrollGranularity::kScrollByDocument);
  } else if (scroll_type == "bottom") {
    local_frame->GetEventHandler().BubblingScroll(
        blink::kScrollBlockDirectionForward, blink::ScrollGranularity::kScrollByDocument);
  } else {
    NOTREACHED();
  }
}

namespace {

bool ToSkBitmap(
    const scoped_refptr<blink::StaticBitmapImage>& static_bitmap_image,
    SkBitmap* dest) {
  const sk_sp<SkImage> image =
      static_bitmap_image->PaintImageForCurrentFrame().GetSkImage();
  return image && image->asLegacyBitmap(
                      dest, SkImage::LegacyBitmapMode::kRO_LegacyBitmapMode);
}

bool SnapshotPage(blink::LocalFrame* local_frame,
                  bool full_page,
                  blink::IntRect rect,
                  SkBitmap* bitmap) {
  blink::Document* document = local_frame->GetDocument();
  if (!document || !document->GetLayoutView())
    return false;

  /*
  We follow DragController::DragImageForSelection here while making sure that
  we paint the whole document including the parts outside the scroll view.
  TODO: See ChromePrintRenderFrameHelperDelegate::GetPdfElement for
  capture of PDF.

  TODO(igor@vivaldi.com): Find out why when full_page is true and we paint the
  whole page including the invisible parts outside the scroll area and when
  document->Lifecycle() is DocumentLifecycle::kVisualUpdatePending or perhaps is
  anything but DocumentLifecycle::kPaintClean or kPrePaintClean painting here
  may affect painting of the page later when the user scrolls the previously
  invisible parts. In such case the scrolled in areas may contains unpainted
  rectangles. For this reason we can only paint the visible part of the page
  when !full_page and we are drawing thumbnails to avoid rendering regressions
  later on each and every page.
  */
  bool has_accelerated_compositing =
    document->GetSettings()->GetAcceleratedCompositingEnabled();

  // Disable accelerated compositing temporary to make canvas and other
  // normally HWA element show up, restrict to full page rendering for now.
  if (full_page) {
    document->GetSettings()->SetAcceleratedCompositingEnabled(false);
  }

  // Force an update of the lifecycle since we changed the painting method
  // of accelerated elements.
  local_frame->View()->UpdateAllLifecyclePhasesExceptPaint();

  blink::LayoutView* view = document->GetLayoutView();
  blink::IntRect document_rect = view->DocumentRect();
  blink::IntRect visible_content_rect =
    local_frame->View()->LayoutViewport()->VisibleContentRect();

  blink::IntSize page_size;
  if (full_page) {
    blink::FloatSize float_page_size = local_frame->ResizePageRectsKeepingRatio(
      blink::FloatSize(document_rect.Width(), document_rect.Height()),
      blink::FloatSize(document_rect.Width(), document_rect.Height()));
    float_page_size.SetHeight(
        std::min(float_page_size.Height(), static_cast<float>(rect.Height())));
    page_size = ExpandedIntSize(float_page_size);
  } else {
    page_size.SetWidth(visible_content_rect.Width());
    page_size.SetHeight(visible_content_rect.Height());
  }

  blink::IntRect page_rect(0, 0, page_size.Width(), page_size.Height());
  if (full_page) {
    // page_rect is relative to the visible scroll area. To include the
    // document top we must use negative offsets for the upper left
    // corner.
    page_rect.SetX(-visible_content_rect.X());
    page_rect.SetY(-visible_content_rect.Y());
  }

  blink::PaintRecordBuilder picture_builder;
  {
    blink::GraphicsContext& context = picture_builder.Context();
    context.SetShouldAntialias(false);

    blink::GlobalPaintFlags global_paint_flags =
      blink::kGlobalPaintFlattenCompositingLayers;
    if (full_page) {
      global_paint_flags |= blink::kGlobalPaintWholePage;
    }

    local_frame->View()->PaintContentsOutsideOfLifecycle(
        context, global_paint_flags, blink::CullRect(page_rect));
  }

  if (full_page) {
    document->GetSettings()->
      SetAcceleratedCompositingEnabled(has_accelerated_compositing);
  }

  SkSurfaceProps surface_props(0, kUnknown_SkPixelGeometry);
  sk_sp<SkSurface> surface =
    SkSurface::MakeRasterN32Premul(
      page_rect.Width(), page_rect.Height(), &surface_props);
  if (!surface)
    return false;

  cc::SkiaPaintCanvas canvas(surface->getCanvas());

  if (full_page) {
    // Translate scroll view coordinates into page-relative ones.
    blink::AffineTransform transform;
    transform.Translate(visible_content_rect.X(), visible_content_rect.Y());
    canvas.concat(blink::AffineTransformToSkMatrix(transform));

    // Prepare PaintChunksToCcLayer called deep under EndRecording
    // to ignore clipping to the visible area.
    DCHECK(!blink::PaintChunksToCcLayer::TopClipToIgnore());
    const blink::ObjectPaintProperties *root_properties =
      view->FirstFragment().PaintProperties();
    if (root_properties) {
      blink::PaintChunksToCcLayer::TopClipToIgnore() =
        root_properties->OverflowClip();
    }
  }

  blink::PropertyTreeState root_tree_state = blink::PropertyTreeState::Root();
  picture_builder.EndRecording(canvas, root_tree_state);

  if (full_page) {
    blink::PaintChunksToCcLayer::TopClipToIgnore() = nullptr;
  } else {
    DCHECK(!blink::PaintChunksToCcLayer::TopClipToIgnore());
  }

  // Crop to rect if required.
  scoped_refptr<blink::StaticBitmapImage> image;
  if (rect.IsEmpty() || full_page) {
    image = blink::StaticBitmapImage::Create(surface->makeImageSnapshot());
  } else {
    image = blink::StaticBitmapImage::Create(
        surface->makeImageSnapshot(SkIRect(rect)));
  }
  return image ? ToSkBitmap(image, bitmap) : false;
}

bool CopyBitmapToSharedRegionAsN32(
    const SkBitmap& bitmap,
    base::ReadOnlySharedMemoryRegion* shared_region) {
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(bitmap.width(), bitmap.height());

  size_t buf_size = info.computeMinByteSize();
  if (buf_size == 0 || buf_size > static_cast<size_t>(INT32_MAX))
    return false;

  base::MappedReadOnlyRegion region_and_mapping =
      mojo::CreateReadOnlySharedMemoryRegion(buf_size);
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

    if (!SnapshotPage(local_frame, params.full_page, rect, &bitmap))
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
