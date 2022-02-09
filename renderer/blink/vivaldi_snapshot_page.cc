// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "renderer/blink/vivaldi_snapshot_page.h"

#include <memory>
#include <string>

#include "cc/paint/skia_paint_canvas.h"
#include "skia/ext/image_operations.h"
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
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace {

bool ToSkBitmap(sk_sp<SkImage> image, SkBitmap* bitmap) {
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(image->width(), image->height());
  bitmap->reset();
  if (!bitmap->tryAllocPixels(info, info.minRowBytes())) {
    LOG(ERROR) << "Failed to allocate memory for capture bitmap";
    return false;
  }
  if (!image->readPixels(info, bitmap->getPixels(), info.minRowBytes(), 0, 0)) {
    LOG(ERROR) << "Failed toread pixels into the capture bitmap";
    return false;
  }
  return true;
}

}  // namespace

bool VivaldiSnapshotPage(blink::LocalFrame* local_frame,
                         bool full_page,
                         const blink::IntRect& rect,
                         SkBitmap* bitmap) {
  // This is based on DragController::DragImageForSelection.
  //
  // TODO(igor@vivaldi.com): Find out why when full_page is true and we paint
  // the whole page including the invisible parts outside the scroll area and
  // when the lifecycle is DocumentLifecycle::kVisualUpdatePending or
  // perhaps is anything but DocumentLifecycle::kPaintClean or kPrePaintClean
  // painting here may affect painting of the page later when the user scrolls
  // the previously invisible parts. In such case the scrolled in areas may
  // contains unpainted rectangles. For this reason we can only paint the
  // visible part of the page when !full_page and we are drawing thumbnails to
  // avoid rendering regressions later on each and every page.
  if (!local_frame->View()) {
    LOG(ERROR) << "no view";
    return false;
  }

  blink::Document* document = local_frame->GetDocument();
  if (!document) {
    LOG(ERROR) << "no document";
    return false;
  }

  blink::IntRect visible_content_rect =
      local_frame->View()->LayoutViewport()->VisibleContentRect();
  if (visible_content_rect.IsEmpty()) {
    LOG(ERROR) << "empty visible content rect";
    return false;
  }

  blink::IntRect page_rect;
  if (full_page) {
    blink::LayoutView* layout_view = document->GetLayoutView();
    if (!layout_view) {
      LOG(ERROR) << "no layout view";
      return false;
    }
    blink::PhysicalRect document_rect = layout_view->DocumentRect();
    blink::FloatSize float_page_size = local_frame->ResizePageRectsKeepingRatio(
        blink::FloatSize(document_rect.Width(), document_rect.Height()),
        blink::FloatSize(document_rect.Width(), document_rect.Height()));
    float_page_size.SetHeight(
        std::min(float_page_size.Height(), static_cast<float>(rect.Height())));
    blink::IntSize page_size = ExpandedIntSize(float_page_size);
    page_rect.SetWidth(page_size.Width());
    page_rect.SetHeight(page_size.Height());

    // page_rect is relative to the visible scroll area. To include the
    // document top we must use negative offsets for the upper left
    // corner.
    page_rect.SetX(-visible_content_rect.X());
    page_rect.SetY(-visible_content_rect.Y());
  } else {
    page_rect.SetWidth(visible_content_rect.Width());
    page_rect.SetHeight(visible_content_rect.Height());
  }

  local_frame->View()->UpdateAllLifecyclePhasesExceptPaint(
      blink::DocumentUpdateReason::kSelection);

  switch (document->Lifecycle().GetState()) {
    case blink::DocumentLifecycle::kPrePaintClean:
    case blink::DocumentLifecycle::kPaintClean:
      break;
    default:
      // DragController::DragImageForSelection proceeds with a call to
      // LocalFrameView::PaintContentsOutsideOfLifecycle() in any paint state
      // even if the documentation for the latter requres that the state must
      // be one of the above. For now do the same but log it.
      LOG(WARNING) << "Unexpected lifecycle state "
                   << document->Lifecycle().GetState();
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

  SkSurfaceProps surface_props(0, kUnknown_SkPixelGeometry);
  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(
      page_rect.Width(), page_rect.Height(), &surface_props);
  if (!surface) {
    LOG(ERROR) << "failed to allocate surface width=" << page_rect.Width()
               << " height=" << page_rect.Height();
    return false;
  }

  cc::SkiaPaintCanvas canvas(surface->getCanvas());

  if (full_page) {
    // Translate scroll view coordinates into page-relative ones.
    blink::AffineTransform transform;
    transform.Translate(visible_content_rect.X(), visible_content_rect.Y());
    canvas.concat(blink::AffineTransformToSkMatrix(transform));

    // Prepare PaintChunksToCcLayer called deep under EndRecording
    // to ignore clipping to the visible area.
    DCHECK(!blink::PaintChunksToCcLayer::TopClipToIgnore());
    blink::LayoutView* layout_view = document->GetLayoutView();
    DCHECK(layout_view);
    const blink::ObjectPaintProperties* root_properties =
        layout_view->FirstFragment().PaintProperties();
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

  sk_sp<SkImage> snapshot;
  if (rect.IsEmpty() || full_page) {
    snapshot = surface->makeImageSnapshot();
  } else {
    snapshot = surface->makeImageSnapshot(SkIRect(rect));
  }
  if (!snapshot) {
    LOG(ERROR) << "failed to create image snapshot";
    return false;
  }
  return ToSkBitmap(std::move(snapshot), bitmap);
}
