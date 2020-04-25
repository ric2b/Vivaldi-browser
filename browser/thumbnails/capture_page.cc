// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "browser/thumbnails/capture_page.h"

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
using content::WebContents;

namespace vivaldi {

namespace {

// Time to wait for the capture result before reporting an error.
constexpr base::TimeDelta kMaxWaitForCapture = base::TimeDelta::FromSeconds(30);

void ReleaseSharedMemoryPixels(void* addr, void* context) {
  delete reinterpret_cast<base::SharedMemory*>(context);
}

}  // namespace

int CapturePage::s_callback_id = 0;

CapturePage::CapturePage() : weak_ptr_factory_(this) {}

CapturePage::~CapturePage() {}

// static
void CapturePage::Capture(content::WebContents* contents,
                          const CaptureParams& params,
                          DoneCallback callback) {
  DCHECK(callback);
  CapturePage* capture_page = new CapturePage();

  capture_page->CaptureImpl(contents, params, std::move(callback));
}

void CapturePage::CaptureImpl(content::WebContents* contents,
                              const CaptureParams& input_params,
                              DoneCallback callback) {
  DCHECK(!callback.is_null());
  capture_callback_ = std::move(callback);
  callback_id_ = ++s_callback_id;
  once_per_contents_ = input_params.once_per_contents;

  VivaldiViewMsg_RequestThumbnailForFrame_Params param;
  param.callback_id = callback_id_;
  param.rect = input_params.rect;
  param.full_page = input_params.full_page;

  WebContentsObserver::Observe(contents);

  contents->GetRenderViewHost()->Send(
      new VivaldiViewMsg_RequestThumbnailForFrame(
          contents->GetRenderViewHost()->GetRoutingID(), param));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindRepeating(&CapturePage::OnCaptureTimeout,
                          weak_ptr_factory_.GetWeakPtr()),
      kMaxWaitForPageLoad);
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

bool CapturePage::OnMessageReceived(const IPC::Message& message) {
  DCHECK(WebContentsObserver::web_contents());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CapturePage, message)
    IPC_MESSAGE_HANDLER(VivaldiViewHostMsg_RequestThumbnailForFrame_ACK,
                        OnRequestThumbnailForFrameResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CapturePage::OnRequestThumbnailForFrameResponse(
    base::SharedMemoryHandle handle,
    gfx::Rect image_rect,
    int callback_id,
    bool success) {
  if (callback_id != callback_id_) {
    if (once_per_contents_) {
      LOG(ERROR) << "unexpected callback id " << callback_id << " when "
                 << callback_id_ << " was expected";
      RespondAndDelete();
    }
    return;
  }

  if (!success || !base::SharedMemory::IsHandleValid(handle)) {
    LOG(ERROR) << "no data from the renderer process";
    RespondAndDelete();
    return;
  }

  Result captured;
  captured.handle_ = std::move(handle);
  captured.rect_ = image_rect;
  captured.success_ = true;
  RespondAndDelete(std::move(captured));
}

void CapturePage::OnCaptureTimeout() {
  LOG(ERROR) << "timeout waiting for capture result";
  RespondAndDelete();
}

CapturePage::Result::Result() = default;

CapturePage::Result::Result(Result&& other)
    : handle_(other.handle_),
      rect_(std::move(other.rect_)),
      success_(std::move(other.success_)) {
  // Workaround for lack of move sematics in SharedMemoryHandle to prevent
  // calling Close() in destructor for other.
  other.handle_ = base::SharedMemoryHandle();
}

CapturePage::Result& CapturePage::Result::operator=(Result&& other) {
  handle_ = other.handle_;
  other.handle_ = base::SharedMemoryHandle();
  rect_ = std::move(other.rect_);
  success_ = std::move(other.success_);
  return *this;
}

CapturePage::Result::~Result() {
  if (handle_.IsValid()) {
    handle_.Close();
  }
}

bool CapturePage::Result::MovePixelsToBitmap(SkBitmap* bitmap) {
  if (!success_)
    return false;

  // This should only be called once.
  DCHECK(handle_.IsValid());

  // Let Skia do some sanity checking for (no negative widths/heights, no
  // overflows while calculating bytes per row, etc).
  if (!bitmap->setInfo(
          SkImageInfo::MakeN32Premul(rect_.width(), rect_.height()))) {
    LOG(ERROR) << "sanity check failed on captured image data";
    return false;
  }
  auto bitmap_buffer = std::make_unique<base::SharedMemory>(handle_, true);

  // bitmap_buffer now owns the handle and will close it in the destructor.
  handle_ = base::SharedMemoryHandle();

  if (!bitmap_buffer->Map(bitmap->computeByteSize())) {
    LOG(ERROR) << "capture size mismatch";
    return false;
  }

  // SkBitmap calls the release function when it no longer access the memory
  // including failure cases.
  base::SharedMemory* buffer_pointer = bitmap_buffer.release();
  if (!bitmap->installPixels(bitmap->info(), buffer_pointer->memory(),
                              bitmap->rowBytes(), &ReleaseSharedMemoryPixels,
                              buffer_pointer)) {
    LOG(ERROR) << "data could not be copied to bitmap";
    return false;
  }
  return true;
}

}  // namespace namespace vivaldi
