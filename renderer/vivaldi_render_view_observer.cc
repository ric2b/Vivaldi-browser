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
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/codec/jpeg_codec.h"

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

namespace {

bool ToSkBitmap(
    const scoped_refptr<blink::StaticBitmapImage>& static_bitmap_image,
    SkBitmap& dest) {
  const sk_sp<SkImage> image =
      static_bitmap_image->PaintImageForCurrentFrame().GetSkImage();
  return image && image->asLegacyBitmap(
                      &dest, SkImage::LegacyBitmapMode::kRO_LegacyBitmapMode);
}

bool SnapshotPage(blink::LocalFrame* local_frame,
                  bool full_page,
                  float width,
                  float height,
                  SkBitmap& bitmap) {
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
    float_page_size.SetHeight(std::min(float_page_size.Height(), height));
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

    local_frame->View()->PaintContents(context, global_paint_flags, page_rect);
  }

  if (full_page) {
    document->GetSettings()->
      SetAcceleratedCompositingEnabled(has_accelerated_compositing);
  }

  SkSurfaceProps surface_props(0, kUnknown_SkPixelGeometry);
  sk_sp<SkSurface> surface =
    SkSurface::MakeRasterN32Premul(
      page_size.Width(), page_size.Height(), &surface_props);
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

  scoped_refptr<blink::StaticBitmapImage> image =
    blink::StaticBitmapImage::Create(surface->makeImageSnapshot());
  return ToSkBitmap(image, bitmap);
}

bool CopyBitmapToSharedMem(const SkBitmap& bitmap,
                           base::SharedMemoryHandle* shared_mem_handle) {
  // Only 32-bit bitmaps are supported.
  DCHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  const gfx::Size size(bitmap.width(), bitmap.height());
  std::unique_ptr<base::SharedMemory> shared_buf;
  {
    void* pixels = bitmap.getPixels();
    if (!pixels)
      return false;

    base::CheckedNumeric<uint32_t> checked_buf_size = 4;
    checked_buf_size *= size.width();
    checked_buf_size *= size.height();
    if (!checked_buf_size.IsValid())
      return false;

    // Allocate a shared memory buffer to hold the bitmap bits.
    uint32_t buf_size = checked_buf_size.ValueOrDie();
    shared_buf =
        content::ChildThreadImpl::current()->AllocateSharedMemory(buf_size);
    if (!shared_buf)
      return false;
    if (!shared_buf->Map(buf_size))
      return false;
    // Copy the bits into shared memory
    DCHECK(shared_buf->memory());
    memcpy(shared_buf->memory(), pixels, buf_size);
    shared_buf->Unmap();
  }
  *shared_mem_handle =
      base::SharedMemory::DuplicateHandle(shared_buf->handle());
  return true;
}

}  // namespace

void VivaldiRenderViewObserver::OnRequestThumbnailForFrame(
    VivaldiViewMsg_RequestThumbnailForFrame_Params params) {
  base::SharedMemoryHandle shared_memory_handle;
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

    gfx::Size size = params.size;
    SkBitmap bitmap;
    if (!SnapshotPage(local_frame, params.full_page,
                      size.width(), size.height(), bitmap))
      break;

    SkBitmap thumbnail;
    if (bitmap.colorType() == kN32_SkColorType) {
      thumbnail = bitmap;
    } else {
      if (thumbnail.tryAllocPixels(bitmap.info())) {
        bitmap.readPixels(thumbnail.info(), thumbnail.getPixels(),
                          thumbnail.rowBytes(), 0, 0);
      }
    }
    if (!CopyBitmapToSharedMem(thumbnail, &shared_memory_handle))
      break;

    size = gfx::Size(thumbnail.width(), thumbnail.height());

    Send(new VivaldiViewHostMsg_RequestThumbnailForFrame_ACK(
        routing_id(), shared_memory_handle, size, params.callback_id, true));
    return;
  } while (false);

  Send(new VivaldiViewHostMsg_RequestThumbnailForFrame_ACK(
      routing_id(), shared_memory_handle, gfx::Size(), params.callback_id,
      false));
}

}  // namespace vivaldi
