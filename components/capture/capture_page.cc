// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/capture/capture_page.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/paint_preview/browser/compositor_utils.h"
#include "components/paint_preview/browser/paint_preview_client.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/recording_map.h"
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

SkBitmap ConvertCaptureMemoryToBitmap(gfx::Size image_size,
                                      gfx::Size target_size,
                                      base::ReadOnlySharedMemoryRegion region) {
  do {
    if (!region.IsValid() || image_size.IsEmpty()) {
      LOG(ERROR) << "no data from the renderer process";
      break;
    }

    if (!target_size.IsEmpty() && target_size != image_size) {
      LOG(ERROR) << "unexpected image size " << image_size.width() << "x"
                 << image_size.height() << " when " << target_size.width()
                 << "x" << target_size.height() << " was expected";
      break;
    }

    SkImageInfo info =
        SkImageInfo::MakeN32Premul(image_size.width(), image_size.height());
    if (info.computeMinByteSize() != region.GetSize()) {
      LOG(ERROR) << "The image size does not match allocated memory";
      break;
    }

    // We transfer the ownership of the mapping into the bitmap, hence we need
    // an instance on the heap.
    auto mapping =
        std::make_unique<base::ReadOnlySharedMemoryMapping>(region.Map());

    // Release the region now as the mapping is independent of it.
    region = base::ReadOnlySharedMemoryRegion();

    if (!mapping->IsValid()) {
      LOG(ERROR) << "failed to map the captured image data";
      break;
    }

    // installPixels uses void*, not const void* for pixels.
    void* pixels = const_cast<void*>(mapping->memory());

    // SkBitmap calls the release function when it no longer access the memory
    // including failure cases hence calling mapping.release() does not leak
    // if installPixels returns false.
    void* sk_release_context = mapping.release();
    SkBitmap bitmap;
    if (!bitmap.installPixels(info, pixels, info.minRowBytes(),
                              ReleaseSharedMemoryPixels, sk_release_context)) {
      LOG(ERROR) << "data could not be copied to bitmap";
      break;
    }
    return bitmap;
  } while (false);

  return SkBitmap();
}

// This is based on
// chromium/components/paint_preview/browser/paint_preview_base_service.cc and
// chromium/components/paint_preview\player/player_compositor_delegate.cc.
//
// TODO(igor@vivaldi.com): Figure out how to use those classes directly without
// duplicating their code here. The main problem is that their usage is tailored
// for Android.
class PaintPreviewCaptureState {
 public:
  static void StartCapture(content::WebContents* web_contents,
                           const gfx::Rect& clip_rect,
                           CapturePage::BitmapCallback callback) {
    VLOG(2) << "Capture start, clip=(" << clip_rect.x() << " " << clip_rect.y()
            << " " << clip_rect.width() << " " << clip_rect.height() << ")";
    content::RenderFrameHost* render_frame_host =
        web_contents->GetPrimaryMainFrame();

    paint_preview::PaintPreviewClient::CreateForWebContents(web_contents);
    auto* client =
        paint_preview::PaintPreviewClient::FromWebContents(web_contents);
    if (!client) {
      VLOG(1) << "Failed to create PaintPreviewClient";
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), SkBitmap()));
      return;
    }

    paint_preview::PaintPreviewClient::PaintPreviewParams params(
        paint_preview::RecordingPersistence::kMemoryBuffer);
    params.inner.is_main_frame = true;
    params.inner.capture_links = false;
    params.inner.max_capture_size = 100 * 1024 * 1024;
    params.inner.max_decoded_image_size_bytes = 1024 * 1024 * 1024;
    params.inner.skip_accelerated_content = false;
    params.inner.clip_rect = clip_rect;

    // We manually delete `state` either after a successful capture or on any
    // error or timeout. The pattern is safe as long as the weak ptr is used for
    // any callback.
    auto* state =
        new PaintPreviewCaptureState(web_contents, std::move(callback));
    client->CapturePaintPreview(
        params, render_frame_host,
        base::BindOnce(&PaintPreviewCaptureState::OnCaptureResult,
                       state->weak_factory_.GetWeakPtr()));
  }

 private:
  PaintPreviewCaptureState(content::WebContents* web_contents,
                           CapturePage::BitmapCallback callback)
      : result_callback_(std::move(callback)),
        capture_handle_(
            web_contents->IncrementCapturerCount(gfx::Size(),
                                                 /*stay_hidden=*/true,
                                                 /*stay_awake=*/true,
                                                 /*is_activity=*/false)) {
    DCHECK(result_callback_);
    timeout_timer_.Start(
        FROM_HERE, kMaxWaitForCapture,
        base::BindOnce(&PaintPreviewCaptureState::OnCaptureTimeout,
                       weak_factory_.GetWeakPtr()));
  }

  ~PaintPreviewCaptureState() {
    std::move(result_callback_).Run(std::move(bitmap_));
  }

  void OnCaptureResult(
      base::UnguessableToken guid,
      paint_preview::mojom::PaintPreviewStatus status,
      std::unique_ptr<paint_preview::CaptureResult> capture_result) {
    capture_handle_.RunAndReset();
    if (!(status == paint_preview::mojom::PaintPreviewStatus::kOk ||
          status ==
              paint_preview::mojom::PaintPreviewStatus::kPartialSuccess) ||
        !capture_result->capture_success) {
      VLOG(1) << "Failed capture, status=" << status;
      delete this;
      return;
    }

    VLOG(2) << "Starting composition";
    capture_result_ = std::move(capture_result);

    auto disconnect_handler =
        base::BindRepeating(&PaintPreviewCaptureState::OnComposerDisconnect,
                            weak_factory_.GetWeakPtr());
    compositor_ = paint_preview::StartCompositorService(disconnect_handler);
    compositor_client_ = compositor_->CreateCompositor(
        base::BindOnce(&PaintPreviewCaptureState::OnCompositorStarted,
                       weak_factory_.GetWeakPtr()));
    compositor_client_->SetDisconnectHandler(disconnect_handler);
  }

  void OnCaptureTimeout() {
    VLOG(1) << "Capture timeout";
    delete this;
  }

  void OnComposerDisconnect() {
    VLOG(1) << "Paint Preview Composer disconnected";
    delete this;
  }

  void OnCompositorStarted() {
    VLOG(2) << "Composition has started";

    compositor_client_->BeginMainFrameComposite(
        PrepareCompositeRequest(std::move(capture_result_)),
        base::BindOnce(&PaintPreviewCaptureState::OnCompositorReadyStatus,
                       weak_factory_.GetWeakPtr()));
  }

  void OnCompositorReadyStatus(
      paint_preview::mojom::PaintPreviewCompositor::BeginCompositeStatus status,
      paint_preview::mojom::PaintPreviewBeginCompositeResponsePtr
          composite_response) {
    if (status != paint_preview::mojom::PaintPreviewCompositor::
                      BeginCompositeStatus::kSuccess) {
      VLOG(1) << "Failed begin compose, status=" << static_cast<int>(status);
      delete this;
      return;
    }
    VLOG(2) << "Composition is ready";

    compositor_client_->BitmapForMainFrame(
        gfx::Rect(), 1.0,
        base::BindOnce(&PaintPreviewCaptureState::OnBitmapReady,
                       weak_factory_.GetWeakPtr()));
  }

  void OnBitmapReady(
      paint_preview::mojom::PaintPreviewCompositor::BitmapStatus status,
      const ::SkBitmap& bitmap) {
    if (status ==
        paint_preview::mojom::PaintPreviewCompositor::BitmapStatus::kSuccess) {
      VLOG(2) << "Successfully got bitmap for the main frame";
      success_ = true;
      bitmap_ = std::move(bitmap);
    } else {
      VLOG(1) << "Failed bitmap creation, status=" << static_cast<int>(status);
    }
    delete this;
  }

  static std::optional<base::ReadOnlySharedMemoryRegion>
  ToReadOnlySharedMemory(paint_preview::PaintPreviewProto&& proto) {
    auto region =
        base::WritableSharedMemoryRegion::Create(proto.ByteSizeLong());
    if (!region.IsValid())
      return std::nullopt;

    auto mapping = region.Map();
    if (!mapping.IsValid())
      return std::nullopt;

    proto.SerializeToArray(mapping.memory(), mapping.size());
    return base::WritableSharedMemoryRegion::ConvertToReadOnly(
        std::move(region));
  }

  static paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
  PrepareCompositeRequest(
      std::unique_ptr<paint_preview::CaptureResult> capture_result) {
    paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
        begin_composite_request =
            paint_preview::mojom::PaintPreviewBeginCompositeRequest::New();
    std::pair<paint_preview::RecordingMap, paint_preview::PaintPreviewProto>
        map_and_proto = paint_preview::RecordingMapFromCaptureResult(
            std::move(*capture_result));
    begin_composite_request->recording_map = std::move(map_and_proto.first);
    if (begin_composite_request->recording_map.empty())
      return nullptr;

    begin_composite_request->preview =
        mojo_base::ProtoWrapper(std::move(map_and_proto.second));

    return begin_composite_request;
  }

  bool success_ = false;
  SkBitmap bitmap_;
  CapturePage::BitmapCallback result_callback_;
  base::TimeTicks start_time = base::TimeTicks::Now();
  base::ScopedClosureRunner capture_handle_;
  std::unique_ptr<paint_preview::CaptureResult> capture_result_;
  base::OneShotTimer timeout_timer_;

  std::unique_ptr<paint_preview::PaintPreviewCompositorService,
                  base::OnTaskRunnerDeleter>
      compositor_{nullptr, base::OnTaskRunnerDeleter(nullptr)};
  std::unique_ptr<paint_preview::PaintPreviewCompositorClient,
                  base::OnTaskRunnerDeleter>
      compositor_client_{nullptr, base::OnTaskRunnerDeleter(nullptr)};
  base::WeakPtrFactory<PaintPreviewCaptureState> weak_factory_{this};
};

}  // namespace

CapturePage::CapturePage() = default;

CapturePage::~CapturePage() = default;

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
                          BitmapCallback callback) {
  DCHECK(callback);
  if (params.full_page) {
    PaintPreviewCaptureState::StartCapture(contents, gfx::Rect(),
                                           std::move(callback));
    return;
  }

  CapturePage* capture_page = new CapturePage();
  capture_page->CaptureImpl(contents, params, std::move(callback));
}

void CapturePage::CaptureImpl(content::WebContents* web_contents,
                              const CaptureParams& input_params,
                              BitmapCallback callback) {
  DCHECK(callback);
  capture_callback_ = std::move(callback);
  target_size_ = input_params.target_size;

  WebContentsObserver::Observe(web_contents);

  auto* rfhi = static_cast<content::RenderFrameHostImpl*>(
      web_contents->GetPrimaryMainFrame());
  rfhi->GetVivaldiFrameService()->RequestThumbnailForFrame(
      input_params.rect, input_params.full_page, input_params.target_size,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&CapturePage::OnRequestThumbnailForFrameResponse,
                         weak_ptr_factory_.GetWeakPtr()),
          gfx::Size(), base::ReadOnlySharedMemoryRegion()));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindRepeating(&CapturePage::OnCaptureTimeout,
                          weak_ptr_factory_.GetWeakPtr()),
      kMaxWaitForCapture);
}

void CapturePage::RespondAndDelete(SkBitmap bitmap) {
  // Delete before calling the callback to release all resources ASAP and stop
  // observing WebContents as the callback may delete the contents which in turn
  // triggers WebContentsDestroyed and recursive delete this.
  BitmapCallback callback = std::move(capture_callback_);
  delete this;
  std::move(callback).Run(std::move(bitmap));
}

void CapturePage::OnCaptureDone(SkBitmap bitmap) {
  RespondAndDelete(std::move(bitmap));
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
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ConvertCaptureMemoryToBitmap, image_size, target_size_,
                     std::move(region)),
      base::BindOnce(&CapturePage::OnCaptureDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CapturePage::OnCaptureTimeout() {
  LOG(ERROR) << "timeout waiting for capture result";
  RespondAndDelete();
}

}  // namespace vivaldi
