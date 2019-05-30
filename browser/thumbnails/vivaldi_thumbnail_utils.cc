// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "browser/thumbnails/vivaldi_thumbnail_utils.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/api/extension_types.h"
#include "renderer/vivaldi_render_messages.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/vivaldi_ui_utils.h"

using content::BrowserThread;
using content::RenderWidgetHost;
using content::RenderWidgetHostView;
using content::WebContents;
using vivaldi::ui_tools::EncodeBitmap;
using vivaldi::ui_tools::SmartCropAndSize;

namespace vivaldi {
void ReleaseSharedMemoryPixels(void* addr, void* context) {
  delete reinterpret_cast<base::SharedMemory*>(context);
}

CaptureLoadedPage::CaptureLoadedPage(content::BrowserContext* context) {}

CaptureLoadedPage::~CaptureLoadedPage() {
}

void CaptureLoadedPage::CaptureAndScale(
    content::WebContents* contents,
    CaptureParams& params,
    int request_id,
    const CaptureAndScaleDoneCallback& callback) {
  capture_scale_callback_ = callback;

  Capture(contents, params, request_id,
          base::Bind(&CaptureLoadedPage::OnCaptureCompleted, this));
}

void CaptureLoadedPage::Capture(content::WebContents* contents,
                                CaptureParams& input_params,
                                int request_id,
                                const CaptureDoneCallback& callback) {
  VivaldiViewMsg_RequestThumbnailForFrame_Params param;
  gfx::Rect rect = contents->GetContainerBounds();

  input_params_ = input_params;

  param.callback_id = request_id;
  if (input_params_.full_page_) {
    param.size = input_params_.capture_size_;
  } else {
    param.size = rect.size();
  }
  capture_callback_ = callback;
  param.full_page = input_params_.full_page_;

  WebContentsObserver::Observe(contents);

  contents->GetRenderViewHost()->Send(
      new VivaldiViewMsg_RequestThumbnailForFrame(
          contents->GetRenderViewHost()->GetRoutingID(), param));
}

bool CaptureLoadedPage::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CaptureLoadedPage, message)
    IPC_MESSAGE_HANDLER(VivaldiViewHostMsg_RequestThumbnailForFrame_ACK,
                        OnRequestThumbnailForFrameResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CaptureLoadedPage::OnRequestThumbnailForFrameResponse(
    base::SharedMemoryHandle handle,
    const gfx::Size image_size,
    int callback_id,
    bool success) {
  DCHECK(!capture_callback_.is_null());
  if (!capture_callback_.is_null()) {
    capture_callback_.Run(handle, image_size, callback_id, success);
    capture_callback_.Reset();
  }
}

void CaptureLoadedPage::OnCaptureCompleted(
  base::SharedMemoryHandle handle,
  const gfx::Size size,
  int callback_id,
  bool success) {
  if (!success || !base::SharedMemory::IsHandleValid(handle)) {
    LOG(ERROR) << "Failed to capture tab: no data from the renderer process";
    return;
  }
  base::PostTaskWithTraits(FROM_HERE,
                           {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
                            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
                           base::Bind(&CaptureLoadedPage::ScaleAndConvertImage,
                                      this, handle, size, callback_id));
}

void CaptureLoadedPage::ScaleAndConvertImage(base::SharedMemoryHandle handle,
                                             const gfx::Size image_size,
                                             int callback_id) {
  std::unique_ptr<base::SharedMemory> bitmap_buffer(
      new base::SharedMemory(handle, true));

  SkBitmap screen_capture;

  // Let Skia do some sanity checking for (no negative widths/heights, no
  // overflows while calculating bytes per row, etc).
  if (!screen_capture.setInfo(SkImageInfo::MakeN32Premul(
          image_size.width(), image_size.height()))) {
    LOG(ERROR)
        << "Failed to capture tab: Sanity check failed on captured image data";
    return;
  }
  // Make sure the size is representable as a signed 32-bit int, so
  // SkBitmap::getSize() won't be truncated.
  if (!bitmap_buffer->Map(screen_capture.computeByteSize())) {
    LOG(ERROR) << "Failed to capture tab: size mismatch";
    return;
  }

  if (!screen_capture.installPixels(
          screen_capture.info(), bitmap_buffer->memory(),
          screen_capture.rowBytes(), &ReleaseSharedMemoryPixels,
          bitmap_buffer.get())) {
    LOG(ERROR) << "Failed to capture tab: data could not be copied to bitmap";
    return;
  }
  // On success, SkBitmap now owns the SharedMemory.
  ignore_result(bitmap_buffer.release());

  SkBitmap bitmap;
  bool need_resize = true;

  if (input_params_.scaled_size_.width() != 0 &&
      input_params_.scaled_size_.height() != 0) {
    bitmap =
        SmartCropAndSize(screen_capture, input_params_.scaled_size_.width(),
                         input_params_.scaled_size_.height());
    need_resize = false;
  } else {
    bitmap = screen_capture;
  }

  base::PostTaskWithTraits(
    FROM_HERE, { BrowserThread::UI },
    base::Bind(
      &CaptureLoadedPage::ScaleAndConvertImageDoneOnUIThread,
      this, bitmap, callback_id));
}

void CaptureLoadedPage::ScaleAndConvertImageDoneOnUIThread(
    const SkBitmap& bitmap,
    int callback_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!capture_scale_callback_.is_null());

  capture_scale_callback_.Run(bitmap, callback_id, !bitmap.isNull());
}

}  // namespace namespace vivaldi
