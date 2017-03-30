// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/gpu_rasterizer.h"

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/frame_viewer_instrumentation.h"
#include "cc/output/context_provider.h"
#include "cc/playback/image_hijack_canvas.h"
#include "cc/playback/raster_source.h"
#include "cc/raster/raster_buffer.h"
#include "cc/raster/scoped_gpu_raster.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_provider.h"
#include "cc/tiles/tile_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/core/SkMultiPictureDraw.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {

GpuRasterizer::GpuRasterizer(ContextProvider* context_provider,
                             ResourceProvider* resource_provider,
                             bool use_distance_field_text,
                             int msaa_sample_count)
    : resource_provider_(resource_provider),
      use_distance_field_text_(use_distance_field_text),
      msaa_sample_count_(msaa_sample_count) {
}

GpuRasterizer::~GpuRasterizer() {
}

void GpuRasterizer::RasterizeSource(
    ResourceProvider::ScopedWriteLockGr* write_lock,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& playback_rect,
    float scale,
    const RasterSource::PlaybackSettings& playback_settings) {
  // Play back raster_source into temp SkPicture.
  SkPictureRecorder recorder;
  const gfx::Size size = write_lock->GetResourceSize();
  const int flags = SkPictureRecorder::kComputeSaveLayerInfo_RecordFlag;
  sk_sp<SkCanvas> canvas = sk_ref_sp(
      recorder.beginRecording(size.width(), size.height(), NULL, flags));
  canvas->save();
  // The GPU image decode controller assumes that Skia is done with an image
  // when playback is complete. However, in this case, where we play back to a
  // picture, we don't actually finish with the images until the picture is
  // rasterized later. This can cause lifetime issues in the GPU image decode
  // controller. To avoid this, we disable the image hijack canvas (and image
  // decode controller) for this playback step, instead enabling it for the
  // later picture rasterization.
  RasterSource::PlaybackSettings settings = playback_settings;
  settings.use_image_hijack_canvas = false;
  raster_source->PlaybackToCanvas(canvas.get(), raster_full_rect, playback_rect,
                                  scale, settings);
  canvas->restore();
  sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

  // Turn on distance fields for layers that have ever animated.
  bool use_distance_field_text =
      use_distance_field_text_ ||
      raster_source->ShouldAttemptToUseDistanceFieldText();

  // Playback picture into resource.
  {
    ScopedGpuRaster gpu_raster(
        resource_provider_->output_surface()->worker_context_provider());
    write_lock->InitSkSurface(use_distance_field_text,
                              raster_source->CanUseLCDText(),
                              msaa_sample_count_);

    SkSurface* sk_surface = write_lock->sk_surface();

    // Allocating an SkSurface will fail after a lost context.  Pretend we
    // rasterized, as the contents of the resource don't matter anymore.
    if (!sk_surface)
      return;

    // As we did not use the image hijack canvas during the initial playback to
    // |picture| (see PlaybackToPicture), we must enable it here if requested.
    SkCanvas* canvas = sk_surface->getCanvas();
    std::unique_ptr<ImageHijackCanvas> hijack_canvas;
    if (playback_settings.use_image_hijack_canvas) {
      const SkImageInfo& info = canvas->imageInfo();
      hijack_canvas.reset(
          new ImageHijackCanvas(info.width(), info.height(),
                                raster_source->image_decode_controller()));
      SkIRect raster_bounds;
      canvas->getClipDeviceBounds(&raster_bounds);
      hijack_canvas->clipRect(SkRect::MakeFromIRect(raster_bounds));
      hijack_canvas->setMatrix(canvas->getTotalMatrix());
      hijack_canvas->addCanvas(canvas);

      // Replace canvas with our ImageHijackCanvas which is wrapping it.
      canvas = hijack_canvas.get();
    }

    SkMultiPictureDraw multi_picture_draw;
    multi_picture_draw.add(canvas, picture.get());
    multi_picture_draw.draw(false);
    write_lock->ReleaseSkSurface();
  }
}

}  // namespace cc
