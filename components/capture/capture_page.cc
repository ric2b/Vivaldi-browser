// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/capture/capture_page.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/png_codec.h"

namespace vivaldi {

namespace {

// Time to wait for the capture result before reporting an error.
constexpr base::TimeDelta kMaxWaitForCapture = base::Seconds(30);

void ReleaseSharedMemoryPixels(void* addr, void* context) {
  // Let std::unique_ptr destructor to release the mapping.
  std::unique_ptr<base::ReadOnlySharedMemoryMapping> mapping(
      static_cast<base::ReadOnlySharedMemoryMapping*>(context));
  DCHECK(mapping->memory() == addr);
}

void OnCopySurfaceDone(float device_scale_factor,
                       CapturePage::CaptureVisibleCallback callback,
                       const SkBitmap& bitmap) {
  bool success = true;
  if (bitmap.drawsNothing()) {
    LOG(ERROR) << "Failed RenderWidgetHostView::CopyFromSurface()";
    success = false;
  }
  std::move(callback).Run(success, device_scale_factor, bitmap);
}

}  // namespace

CapturePage::CapturePage() : weak_ptr_factory_(this) {}

CapturePage::~CapturePage() {}

/* static */
void CapturePage::CaptureVisible(content::WebContents* web_contents,
                                 const gfx::RectF& rect,
                                 CaptureVisibleCallback callback) {
  content::RenderWidgetHostView* view = nullptr;
  if (!web_contents) {
    LOG(ERROR) << "WebContents is null";
  } else {
    view = web_contents->GetRenderWidgetHostView();
    if (!view || !view->GetRenderWidgetHost()) {
      LOG(ERROR) << "View is invisible";
      view = nullptr;
    }
  }
  if (!view) {
    std::move(callback).Run(false, 0.0f, SkBitmap());
    return;
  }

  // CopyFromSurface takes the area in device-independent pixels. However, we
  // want to capture all physical pixels. So scale the bitmap size accordingly.
  gfx::SizeF size_f = rect.size();
  const gfx::NativeView native_view = view->GetNativeView();
  display::Screen* const screen = display::Screen::GetScreen();
  const float device_scale_factor =
      screen->GetDisplayNearestView(native_view).device_scale_factor();
  size_f.Scale(device_scale_factor);

  gfx::Rect capture_area(std::round(rect.x()), std::round(rect.y()),
                         std::round(rect.width()), std::round(rect.height()));
  gfx::Size bitmap_size = gfx::ToRoundedSize(size_f);
  view->CopyFromSurface(capture_area, bitmap_size,
                        base::BindOnce(&OnCopySurfaceDone, device_scale_factor,
                                       std::move(callback)));
}

/* static */
void CapturePage::Capture(content::WebContents* contents,
                          const CaptureParams& params,
                          DoneCallback callback) {
  DCHECK(callback);
  CapturePage* capture_page = new CapturePage();

  capture_page->CaptureImpl(contents, params, std::move(callback));
}

void CapturePage::CaptureImpl(content::WebContents* web_contents,
                              const CaptureParams& input_params,
                              DoneCallback callback) {
  DCHECK(!callback.is_null());
  capture_callback_ = std::move(callback);
  target_size_ = input_params.target_size;
  client_id_ = input_params.client_id;

  WebContentsObserver::Observe(web_contents);

  auto* rfhi =
      static_cast<content::RenderFrameHostImpl*>(web_contents->GetMainFrame());
  rfhi->GetVivaldiFrameService()->RequestThumbnailForFrame(
      input_params.rect, input_params.full_page, input_params.target_size,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&CapturePage::OnRequestThumbnailForFrameResponse,
                         weak_ptr_factory_.GetWeakPtr()),
          gfx::Size(), base::ReadOnlySharedMemoryRegion()));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindRepeating(&CapturePage::OnCaptureTimeout,
                          weak_ptr_factory_.GetWeakPtr()),
      kMaxWaitForCapture);
}

void CapturePage::RespondAndDelete(Result captured) {
  // Delete before calling the callback to release all resources ASAP.
  DoneCallback callback = std::move(capture_callback_);
  delete this;
  std::move(callback).Run(std::move(captured));
}

void CapturePage::WebContentsDestroyed() {
  LOG(ERROR) << "WebContents was destroyed before the renderer replied";
  RespondAndDelete();
}

void CapturePage::RenderViewHostChanged(content::RenderViewHost* old_host,
                                        content::RenderViewHost* new_host) {
  LOG(ERROR) << "RenderViewHost was replaced before the renderer replied";
  RespondAndDelete();
}

void CapturePage::OnRequestThumbnailForFrameResponse(
    const gfx::Size& image_size,
    base::ReadOnlySharedMemoryRegion region) {
  Result captured;
  do {
    if (!region.IsValid() || image_size.IsEmpty()) {
      LOG(ERROR) << "no data from the renderer process";
      break;
    }

    if (!target_size_.IsEmpty() && target_size_ != image_size) {
      LOG(ERROR) << "unexpected image size " << image_size.width() << "x"
                 << image_size.height() << " when " << target_size_.width()
                 << "x" << target_size_.height() << " was expected";
      break;
    }

    SkImageInfo info =
        SkImageInfo::MakeN32Premul(image_size.width(), image_size.height());
    if (info.computeMinByteSize() != region.GetSize()) {
      LOG(ERROR) << "The image size does not match allocated memory";
      break;
    }

    captured.image_info_ = info;
    captured.region_ = std::move(region);
  } while (false);

  captured.client_id_ = client_id_;
  RespondAndDelete(std::move(captured));
}

void CapturePage::OnCaptureTimeout() {
  LOG(ERROR) << "timeout waiting for capture result";
  RespondAndDelete();
}

CapturePage::Result::Result() = default;
CapturePage::Result::~Result() = default;
CapturePage::Result::Result(Result&& other) = default;
CapturePage::Result& CapturePage::Result::operator=(Result&& other) = default;

bool CapturePage::Result::MovePixelsToBitmap(SkBitmap* bitmap) {
  if (!region_.IsValid())
    return false;

  // We transfer the ownership of the mapping into the bitmap, hence we need
  // an instance on the heap.
  auto mapping =
      std::make_unique<base::ReadOnlySharedMemoryMapping>(region_.Map());

  // Release the region now as the mapping is independent of it.
  region_ = base::ReadOnlySharedMemoryRegion();

  if (!mapping->IsValid()) {
    LOG(ERROR) << "failed to map the captured image data";
    return false;
  }

  // installPixels uses void*, not const void* for pixels.
  void* pixels = const_cast<void*>(mapping->memory());

  // SkBitmap calls the release function when it no longer access the memory
  // including failure cases hence calling mapping.release() does not leak
  // if installPixels returns false.
  void* sk_release_context = mapping.release();
  if (!bitmap->installPixels(image_info_, pixels, image_info_.minRowBytes(),
                             ReleaseSharedMemoryPixels, sk_release_context)) {
    LOG(ERROR) << "data could not be copied to bitmap";
    return false;
  }

  return true;
}

int CapturePage::Result::client_id() {
  return client_id_;
}

}  // namespace vivaldi
