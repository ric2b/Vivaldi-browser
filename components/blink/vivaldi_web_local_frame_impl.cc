// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"

#include "cc/paint/skia_paint_canvas.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

namespace {

bool ToSkBitmap(
    const scoped_refptr<StaticBitmapImage>& static_bitmap_image,
    SkBitmap & dest) {
   const sk_sp<SkImage> image =
       static_bitmap_image->PaintImageForCurrentFrame().GetSkImage();
   return image && image->asLegacyBitmap(
       &dest, SkImage::LegacyBitmapMode::kRO_LegacyBitmapMode);
}

}  // namespace

bool WebLocalFrameImpl::snapshotPage(SkBitmap& bitmap,
                                     bool full_page,
                                     float width,
                                     float height) {
  if (!GetFrame()->GetDocument() || !GetFrame()->GetDocument()->GetLayoutView())
    return false;

  /*
  We follow DataTransfer::CreateDragImageForFrame here while making sure that
  we paint the whole document including the parts outside the scroll view.
  TODO: See ChromePrintRenderFrameHelperDelegate::GetPdfElement for
  capture of PDF.
  */
  Document* document = GetFrame()->GetDocument();
  bool has_accelerated_compositing =
      document->GetSettings()->GetAcceleratedCompositingEnabled();

  // Disable accelerated compositing temporary to make canvas and other
  // normally HWA element show up, restrict to full page rendering for now.
  if (full_page) {
    document->GetSettings()->SetAcceleratedCompositingEnabled(false);
  }

  // Force an update of the lifecycle since we changed the painting method
  // of accelerated elements.
  GetFrame()->View()->UpdateAllLifecyclePhasesExceptPaint();

  LayoutView* view = document->GetLayoutView();
  IntRect document_rect = view->DocumentRect();
  WebRect visible_content_rect = VisibleContentRect();

  IntSize page_size;
  if (full_page) {
    FloatSize float_page_size = GetFrame()->ResizePageRectsKeepingRatio(
        FloatSize(document_rect.Width(), document_rect.Height()),
        FloatSize(document_rect.Width(), document_rect.Height()));
    float_page_size.SetHeight(std::min(float_page_size.Height(), height));
    page_size = ExpandedIntSize(float_page_size);
  } else {
    page_size.SetWidth(visible_content_rect.width);
    page_size.SetHeight(visible_content_rect.height);
  }

  // page_rect is relative to the visible scroll area. To include the
  // document top we must use negative offsets for the upper left
  // corner.
  IntRect page_rect(-visible_content_rect.x, -visible_content_rect.y,
                    page_size.Width(), page_size.Height());

  PaintRecordBuilder picture_builder;
  {
    GraphicsContext& context = picture_builder.Context();
    context.SetShouldAntialias(false);

    PaintLayer* root_layer = view->Layer();
    PaintLayerPaintingInfo painting_info(
      root_layer, LayoutRect(page_rect),
      kGlobalPaintFlattenCompositingLayers, LayoutSize()
    );

    PaintLayerFlags paint_flags =
        kPaintLayerHaveTransparency |
        kPaintLayerAppliedTransform |
        kPaintLayerUncachedClipRects |
        kPaintLayerPaintingWholePageBackground;

    document->Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
    PaintLayerPainter(*root_layer).Paint(context, painting_info, paint_flags);
    document->Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
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

  // Translate scroll view coordinates into page-relative ones.
  AffineTransform transform;
  transform.Translate(visible_content_rect.x, visible_content_rect.y);
  canvas.concat(AffineTransformToSkMatrix(transform));

  DCHECK(!PaintChunksToCcLayer::TopClipToIgnore());
  const ObjectPaintProperties *root_properties =
    view->FirstFragment().PaintProperties();
  if (root_properties)
    PaintChunksToCcLayer::TopClipToIgnore() = root_properties->OverflowClip();

  PropertyTreeState root_tree_state = PropertyTreeState::Root();
  picture_builder.EndRecording(canvas, root_tree_state);

  PaintChunksToCcLayer::TopClipToIgnore() = nullptr;

  scoped_refptr<StaticBitmapImage> image =
      StaticBitmapImage::Create(surface->makeImageSnapshot());
  return ToSkBitmap(image, bitmap);
}

}  // namespace blink
